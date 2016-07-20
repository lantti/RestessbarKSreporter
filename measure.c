#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmdcl.h"
#include "vmtimer.h"
#include "vmmemory.h"
#include "vmdcl_gpio.h"
#include "log.h"
#include "console.h"
#include "measure.h"

typedef enum 
{
	A128, 
	B32, 
	A64, 
	TERMINATE,
	WAIT
} hx711_op;

typedef struct 
{
	hx711_op op;
	int delay;
	void (*callback)(void*, int); 
	void* callback_env;
	VM_DCL_HANDLE scl_handle;
	VM_DCL_HANDLE sda_handle;
} hx711_task;

typedef struct 
{
	vm_mutex_t mutex;
	hx711_task task;
} handle_details;

handle_details* open_handles[MAX_HANDLE + 1] = {NULL};

static void new_task(handle_details* details, hx711_op op, int delay, void (*callback)(void*, int), void* callback_env)
{
	vm_mutex_lock(&details->mutex);
	details->task.op = op;
	details->task.delay = delay;
	details->task.callback = callback;
	details->task.callback_env = callback_env;
	vm_mutex_unlock(&details->mutex);
}

static void get_task(handle_details* details, hx711_task* task)
{
	vm_mutex_lock(&details->mutex);
	*task = details->task;
	vm_mutex_unlock(&details->mutex);
}

static void enter_standby(hx711_task* task)
{
	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
	vm_thread_sleep(task->delay+1);
}

static VMBOOL wait_result(hx711_task* task)
{
	vm_dcl_gpio_control_level_status_t pin_status;
	int timeout_counter = 8;

	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
	vm_dcl_control(task->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
	while (timeout_counter > 0 && pin_status.level_status == VM_DCL_GPIO_IO_HIGH)
	{
		vm_thread_sleep(100);
		vm_dcl_control(task->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
		timeout_counter--;
	}

	if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static int read_bit(hx711_task* task)
{
	vm_dcl_gpio_control_level_status_t pin_status;

	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
	vm_dcl_control(task->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
	if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
	{
		return 0;
	}
	else
	{
		return 1;
	} 
}

static int read_result(hx711_task* task)
{
	int result;

	result = -read_bit(task);
	for(int i=0;i<23;i++)
	{
		result <<= 1;
		result |= read_bit(task);
	}
	return result;
}

static void clock_setup_pulses(hx711_task* task)
{
	int setup_pulses;

	switch (task->op)
	{
		case A128:
			setup_pulses = 1;
			break;
		case A64:
			setup_pulses = 3;
			break;
		case B32:
			setup_pulses = 2;
			break;
		default:
			setup_pulses = 1;
	}

	for (int i=0;i<setup_pulses;i++)
	{
		read_bit(task);
	}

}

static VMINT32 measurement(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	VMINT measured_result;
	handle_details* details;
	hx711_task current_task;


	write_console("thread enter\n");
	details = (handle_details*) user_data;
	get_task(details, &current_task);
	enter_standby(&current_task);

	while (1)
	{
		get_task(details, &current_task);

		if (current_task.op == TERMINATE)
		{
			break;
		}

		if (current_task.op == WAIT)
		{
			enter_standby(&current_task);
			continue;
		}

		if (current_task.op != A128)
		{
			wait_result(&current_task);
			read_result(&current_task);
			clock_setup_pulses(&current_task);
		}
		wait_result(&current_task);
		measured_result = read_result(&current_task);
		current_task.callback(current_task.callback_env, measured_result);
		enter_standby(&current_task);
	}

	write_console("thread exit\n");
	enter_standby(&current_task);
	vm_mutex_lock(&details->mutex);
	vm_dcl_close(current_task.sda_handle);
	vm_dcl_close(current_task.scl_handle);
	current_task.callback(current_task.callback_env, 0);
	vm_free(details);
	return 0;
}

static void dummy_callback(void* env, int result)
{
}

static handle_details* get_handle_details(ADC_HANDLE handle)
{
	if (handle < 0 && handle > MAX_HANDLE)
	{
		return NULL;
	}

	return open_handles[handle];
}

ADC_HANDLE open_hx711(int scl_pin, int sda_pin)
{
	ADC_HANDLE handle = 0;
	handle_details* details;
	hx711_task* task;

	while (open_handles[handle] != NULL)
	{
		handle++;
		if (handle > MAX_HANDLE)
		{
			return ADC_HANDLE_INVALID;
		}
	}

	details = vm_calloc(sizeof(handle_details));
	if (details == NULL)
	{
		return ADC_HANDLE_INVALID;
	}

	task = &details->task;
	task->scl_handle = vm_dcl_open(VM_DCL_GPIO, scl_pin);
	if (task->scl_handle == VM_DCL_HANDLE_INVALID)
	{
		vm_free(details);
		return ADC_HANDLE_INVALID;
	}
	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
	vm_dcl_control(task->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);

	task->sda_handle = vm_dcl_open(VM_DCL_GPIO, sda_pin);
	if (task->sda_handle == VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_close(task->scl_handle);
		vm_free(details);
		return ADC_HANDLE_INVALID;
	}
	vm_dcl_control(task->sda_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
	vm_dcl_control(task->sda_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_IN, NULL);

	vm_mutex_init(&details->mutex);
	task->op = WAIT;
	task->delay = 100;
	task->callback = dummy_callback;
	task->callback_env = NULL;
	open_handles[handle] = details;
	write_console("open\n");
	vm_thread_create(measurement, (void*) details, (VM_THREAD_PRIORITY) 0);
	return handle;
}

void close_hx711(ADC_HANDLE handle, void (*callback)(void*, int), void* callback_env)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		open_handles[handle] = NULL;
		new_task(details, TERMINATE, 0, callback, callback_env);
		write_console("close\n");
	}
}

void set_hx711_a128(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		new_task(details, A128, delay, callback, callback_env);
	}
}

void set_hx711_a64(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		new_task(details, A64, delay, callback, callback_env);
	}
}

void set_hx711_b32(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		new_task(details, B32, delay, callback, callback_env);
	}
}

void set_hx711_wait(ADC_HANDLE handle, int delay)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		new_task(details, WAIT, delay, dummy_callback, NULL);
	}
}
