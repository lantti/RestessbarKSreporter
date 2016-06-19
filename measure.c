#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmtimer.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "log.h"
#include "measure.h"


static VM_DCL_HANDLE gpio_adc_scl_handle = VM_DCL_HANDLE_INVALID;
static VM_DCL_HANDLE gpio_adc_sda_handle = VM_DCL_HANDLE_INVALID;

static VMINT averaging_buffer[AVERAGING];
static VMINT averaging_point;
static VMINT result_buffer[RESULT_BUFFER_SIZE];
static VMINT result_buffer_read_point;
static VMINT result_buffer_write_point;
static VMBOOL run_measurement;
static vm_mutex_t result_buffer_mutex;
static vm_mutex_t run_measurement_mutex;


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


void start_measurement()
{
    vm_mutex_init(&result_buffer_mutex);
    vm_mutex_init(&run_measurement_mutex);

    vm_mutex_lock(&result_buffer_mutex);
    result_buffer_read_point = 0;
    result_buffer_write_point = 0;
    vm_mutex_unlock(&result_buffer_mutex);

    vm_mutex_lock(&run_measurement_mutex);
    averaging_point = 0;
    run_measurement = TRUE;
    vm_mutex_unlock(&run_measurement_mutex);


    gpio_adc_scl_handle = vm_dcl_open(VM_DCL_GPIO, ADC_SCL);
    if (gpio_adc_scl_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
        vm_dcl_control(gpio_adc_scl_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
    }

    gpio_adc_sda_handle = vm_dcl_open(VM_DCL_GPIO, ADC_SDA);
    if (gpio_adc_sda_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
        vm_dcl_control(gpio_adc_sda_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_IN, NULL);
    }

    vm_thread_create(measurement, (void*) NULL, (VM_THREAD_PRIORITY) 0);
}

VMBOOL get_measurement_result(VMINT* result)
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

void stop_measurement()
{
    vm_mutex_lock(&run_measurement_mutex);
    run_measurement = FALSE;
    vm_mutex_unlock(&run_measurement_mutex);
}

