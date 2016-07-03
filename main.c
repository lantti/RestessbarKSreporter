#include <stdio.h>
#include <string.h>
#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmchset.h"
#include "vmfs.h"
#include "vmtimer.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmdcl_sio.h"
#include "log.h"
#include "report.h"
#include "console.h"
#include "measure.h"
#include "telecom.h"


#define CONSOLE_CMDLINE_SIZE_NAME "console_cmdline_size"
#define DEFAULT_CONSOLE_CMDLINE_SIZE 64

#define TELECOM_APN_NAME "apn"
#define DEFAULT_TELECOM_APN "gprs.swisscom.ch"

#define DEFAULT_MEASUREMENT_AVERAGING            5
#define DEFAULT_MEASUREMENT_INTERVAL 1000
#define DEFAULT_MEASUREMENT_RESULT_BUFFER_SIZE   8

#define ADC_SCL              43
#define ADC_SDA              44

#define REDLED               17
#define GREENLED             15
#define BLUELED              12

#define CONF_FILENAME        u"C:\\config.txt"
#define LOG_FILENAME        u"C:\\event_log.txt"

VM_DCL_HANDLE gpio_red_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_green_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_blue_handle = VM_DCL_HANDLE_INVALID;





void init_leds() {
    gpio_red_handle = vm_dcl_open(VM_DCL_GPIO, REDLED);
    if (gpio_red_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
        vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
        vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
    }

    gpio_green_handle = vm_dcl_open(VM_DCL_GPIO, GREENLED);
    if (gpio_green_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
        vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
        vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
    }

    gpio_blue_handle = vm_dcl_open(VM_DCL_GPIO, BLUELED);
    if (gpio_blue_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
        vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
        vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
    }
}



static void handle_sysevent(VMINT event, VMINT param)
{
    char text_buffer[64];
    int console_cmdline_size;
    switch (event) {
        case VM_EVENT_CREATE:
            open_conf(CONF_FILENAME);
            if (!read_conf_int(CONSOLE_CMDLINE_SIZE_NAME, &console_cmdline_size))
            {
                console_cmdline_size = DEFAULT_CONSOLE_CMDLINE_SIZE;
            }
            close_conf();
            start_log(LOG_FILENAME);
            init_leds();
            start_console();
            init_telecom(APN);            
            start_measurement();
            start_reporting();
            write_log("System started!");
            vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
            break;

        case VM_EVENT_QUIT:
            stop_reporting();
            stop_measurement();
            stop_console();
            vm_dcl_close(gpio_red_handle);
            vm_dcl_close(gpio_green_handle);
            vm_dcl_close(gpio_blue_handle);
            write_log("System stopped.");
            stop_log();
            break;
        default:
            sprintf(text_buffer, "SysEvent: %u:%u", event, param);
            write_log(text_buffer);

    }
}


void vm_main(void)
{
    start_log(LOG_FILENAME);
    vm_pmng_register_system_event_callback(handle_sysevent);
}

