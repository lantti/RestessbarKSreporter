#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmdcl.h"
#include "vmtimer.h"
#include "vmmemory.h"
#include "vmdcl_gpio.h"
#include "log.h"
#include "measure.h"

typedef struct 
{
	VM_SIGNAL_ID terminate_signal;
	VM_DCL_HANDLE scl_handle;
	VM_DCL_HANDLE sda_handle;
	void (*result_callback)(void*, VMINT);
	void* result_callback_env;
	VMUINT measurement_interval;
} handle_details;

handle_details* open_handles[MAX_HANDLE + 1] = {NULL};

//static VM_DCL_HANDLE gpio_adc_scl_handle = VM_DCL_HANDLE_INVALID;
//static VM_DCL_HANDLE gpio_adc_sda_handle = VM_DCL_HANDLE_INVALID;

//static VMINT* averaging_buffer = NULL;
//static VMINT averaging_buffer_size;
//static VMINT averaging_point;
//static VMINT* result_buffer = NULL;
//static VMINT result_buffer_size;
//static VMINT result_buffer_read_point;
//static VMINT result_buffer_write_point;
//static VMINT interval;
//static VMBOOL run_measurement;
//static vm_mutex_t result_buffer_mutex;
//static vm_mutex_t run_measurement_mutex;



//static void publish_result(VMINT result)
//{
//	vm_mutex_lock(&result_buffer_mutex);
//	result_buffer[result_buffer_write_point] = result;
//
//	result_buffer_write_point = (result_buffer_write_point + 1) % RESULT_BUFFER_SIZE;
//
//	if (result_buffer_write_point == result_buffer_read_point)
//	{
//		result_buffer_read_point = (result_buffer_read_point + 1) % RESULT_BUFFER_SIZE;
//		vm_mutex_unlock(&result_buffer_mutex);
//		write_log("Result buffer overrun!");
//	}
//	else
//	{
//		vm_mutex_unlock(&result_buffer_mutex);
//	}
//}




static VMINT32 measurement(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	VMINT measured_result;
	handle_details* details;
	vm_dcl_gpio_control_level_status_t pin_status;

	details = (handle_details*) user_data;
	do
	{
		vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
		vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);

		while(pin_status.level_status != VM_DCL_GPIO_IO_LOW)
		{
			vm_thread_sleep(100);
			vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
		}

		vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
		vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
		vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
		if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
		{
			measured_result = 0;
		}
		else
		{
			measured_result = -1;
		} 

		for(int i=0;i<23;i++)
		{
			vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
			vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
			vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
			if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
			{
				measured_result <<= 1;
			}
			else
			{
				measured_result <<= 1;
				measured_result |= 1;
			} 
		}
		vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);

		details->result_callback(details->result_callback_env, measured_result);

	} while (details->measurement_interval > 0 && vm_signal_timed_wait(details->terminate_signal, details->measurement_interval) == VM_SIGNAL_TIMEOUT);

	vm_dcl_close(details->sda_handle);
	vm_dcl_close(details->scl_handle);
	vm_free(details);
	return 0;
}

static handle_details* get_handle_details(ADC_HANDLE handle)
{
	if (handle < 0 && handle > MAX_HANDLE)
	{
		return NULL;
	}

	return open_handles[handle];
}


ADC_HANDLE start_measurement(int scl_pin, int sda_pin, int measurement_interval, void (*result_callback)(void*, VMINT), void* result_callback_env)
{
	ADC_HANDLE handle = 0;
	handle_details* details;
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

	details->scl_handle = vm_dcl_open(VM_DCL_GPIO, scl_pin);
	if (details->scl_handle == VM_DCL_HANDLE_INVALID)
	{
		vm_free(details);
		return ADC_HANDLE_INVALID;
	}
	vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
	vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
	vm_dcl_control(details->scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);

	details->sda_handle = vm_dcl_open(VM_DCL_GPIO, sda_pin);
	if (details->sda_handle == VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_close(details->scl_handle);
		vm_free(details);
		return ADC_HANDLE_INVALID;
	}
	vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
	vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_IN, NULL);

	details->terminate_signal = vm_signal_create();
	details->result_callback = result_callback;
	details->result_callback_env = result_callback_env;
	details->measurement_interval = measurement_interval;
	open_handles[handle] = details;
	vm_thread_create(measurement, (void*) details, (VM_THREAD_PRIORITY) 0);
	return handle;
}


void stop_measurement(ADC_HANDLE handle)
{
	handle_details* details;
	details = get_handle_details(handle);
	if (details != NULL)
	{
		open_handles[handle] = NULL;
		vm_signal_post(details->terminate_signal);
	}
}

//VMBOOL get_measurement_result(ADC_HANDLE handle, VMINT* result)
//{
//	VMBOOL result_available = FALSE;
//
//	vm_mutex_lock(&result_buffer_mutex);
//	if (result_buffer_read_point != result_buffer_write_point)
//	{
//		*result = result_buffer[result_buffer_read_point];
//		result_buffer_read_point = (result_buffer_read_point + 1) % RESULT_BUFFER_SIZE;
//		result_available = TRUE;
//	}
//	vm_mutex_unlock(&result_buffer_mutex);
//	return result_available;
//}
