#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmtimer.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "log.h"
#include "measure.h"

typedef struct 
{
    VMUINT result_buffer_size;
    VMINT* result_buffer;
    vm_mutex_t result_buffer_mutex;
    VMUINT averaging;
    VMINT* averaging_buffer;
    VMUINT measurement_interval;
    VMBOOL running_measurement;
    VMBOOL stop_requested;
    vm_mutex_t measurement_mutex;
    VM_DCL_HANDLE scl_handle;
    VM_DCL_HANDLE sda_handle;

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



static void publish_result(VMINT result)
{
    vm_mutex_lock(&result_buffer_mutex);
    result_buffer[result_buffer_write_point] = result;

    result_buffer_write_point = (result_buffer_write_point + 1) % RESULT_BUFFER_SIZE;

    if (result_buffer_write_point == result_buffer_read_point)
    {
        result_buffer_read_point = (result_buffer_read_point + 1) % RESULT_BUFFER_SIZE;
        vm_mutex_unlock(&result_buffer_mutex);
        write_log("Result buffer overrun!");
    }
    else
    {
        vm_mutex_unlock(&result_buffer_mutex);
    }
}




static VMINT32 measurement(VM_THREAD_HANDLE handle, void* user_data)
{
    VMINT immediate_result;
    VMINT averaged_results;
    vm_dcl_gpio_control_level_status_t pin_status;
    vm_mutex_lock(&run_measurement_mutex);
    while(run_measurement)
    {
        vm_mutex_unlock(&run_measurement_mutex);
        vm_thread_sleep(MEASUREMENT_INTERVAL-100);
        vm_mutex_lock(&run_measurement_mutex);
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
        vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);

        while(pin_status.level_status != VM_DCL_GPIO_IO_LOW)
        {
            vm_thread_sleep(100);
            vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
        }

        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
        vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
        if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
        {
            immediate_result = 0;
        }
        else
        {
            immediate_result = -1;
        } 

        for(int i=0;i<23;i++)
        {
            vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
            vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_READ, &pin_status);
            vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
            if (pin_status.level_status == VM_DCL_GPIO_IO_LOW)
            {
                immediate_result <<= 1;
            }
            else
            {
                immediate_result <<= 1;
                immediate_result |= 1;
            } 
        }
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);

        if (averaging_point < AVERAGING)
        {
            averaging_buffer[averaging_point] = immediate_result;
            averaging_point++;
        }
        else
        {
            averaged_results = 0;
            for (int i=0;i<AVERAGING;i++)
            {
                averaged_results += averaging_buffer[i];
            }
            averaged_results /= AVERAGING;
            publish_result(averaged_results);
            averaging_point = 0;
            averaging_buffer[averaging_point] = immediate_result;
        
        }
    }
    vm_mutex_unlock(&run_measurement_mutex);
}

static handle_details* get_handle_details(ADC_HANDLE handle)
{
    if (handle < 0 && handle > MAX_HANDLE)
    {
        return NULL;
    }

    return open_handles[handle];
}


VMINT open_adc(int scl_pin, int sda_pin, int result_buffer_size)
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

    details = vm_calloc(size_of(handle_details);
    if (details == NULL)
    {
        return ADC_HANDLE_INVALID;
    }
    details->averaging = 0;
    details->averaging_buffer = NULL;
    details->measurement_interval = 0;
    details->running_measurement = FALSE;
    details->stop_requested = FALSE;
    vm_mutex_init(&details->result_buffer_mutex);
    vm_mutex_init(&details->measurement_mutex);
    details->result_buffer_size = result_buffer_size;

    details->result_buffer = vm_calloc(result_buffer_size);
    if (details->result_buffer == NULL)
    {
        vm_free(details);
        return ADC_HANDLE_INVALID;
    }

    details->scl_handle = vm_dcl_open(VM_DCL_GPIO, scl_pin);
    if (details->scl_handle == VM_DCL_HANDLE_INVALID)
    {
        vm_free(details->result_buffer);
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
        vm_free(details->result_buffer);
        vm_free(details);
        return ADC_HANDLE_INVALID;
    }
    vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
    vm_dcl_control(details->sda_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_IN, NULL);

    open_handles[handle] = details;
}


void close_adc(ADC_HANDLE handle)
{
    handle_details* details;
    VMBOOL running;
    details = get_handle_details(handle);
    if (details != NULL)
    {
        vm_mutex_lock(&details->measurement_mutex);
        vm_mutex_lock(&details->result_buffer_mutex);
        open_handles[handle] = NULL;
        if (details->running_measurement == TRUE)
        {
            write_log("OOPS! Closing adc handle while measurement runs. Measurement thread might be permanently blocked now");
        }
        vm_dcl_close(details->sda_handle);
        vm_dcl_close(details->scl_handle);
        vm_free(details->averaging_buffer);
        vm_free(details->result_buffer);
        vm_free(details);
    }
}

VMBOOL start_measurement(ADC_HANDLE handle, int averaging, int measurement_interval)
{

    vm_mutex_lock(&result_buffer_mutex);
    result_buffer_read_point = 0;
    result_buffer_write_point = 0;
    vm_mutex_unlock(&result_buffer_mutex);

    vm_mutex_lock(&run_measurement_mutex);
    result_buffer_size = 
    averaging_point = 0;
    interval = measurement_interval();
    run_measurement = TRUE;
    vm_mutex_unlock(&run_measurement_mutex);



    if ()
    {
        vm_thread_create(measurement, (void*) NULL, (VM_THREAD_PRIORITY) 0);
    }
    else
    {
        stop_measurement();
    }
}

VMBOOL get_measurement_result(ADC_HANDLE handle, VMINT* result)
{
    VMBOOL result_available = FALSE;

    vm_mutex_lock(&result_buffer_mutex);
    if (result_buffer_read_point != result_buffer_write_point)
    {
        *result = result_buffer[result_buffer_read_point];
        result_buffer_read_point = (result_buffer_read_point + 1) % RESULT_BUFFER_SIZE;
        result_available = TRUE;
    }
    vm_mutex_unlock(&result_buffer_mutex);
    return result_available;
}

void stop_measurement(ADC_HANDLE handle)
{
    vm_mutex_lock(&run_measurement_mutex);
    run_measurement = FALSE;
    vm_mutex_unlock(&run_measurement_mutex);
}

