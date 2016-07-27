#include <stdlib.h>
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
#include "conf.h"

#define ADC_SCL_A              43
#define ADC_SDA_A              44

#define REDLED               17
#define GREENLED             15
#define BLUELED              12

#define CONF_FILENAME        u"C:\\config.txt"
#define LOG_FILENAME        u"C:\\event_log.txt"

VM_DCL_HANDLE gpio_red_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_green_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_blue_handle = VM_DCL_HANDLE_INVALID;

ADC_HANDLE adc_handle_a = ADC_HANDLE_INVALID;
afifo* result_buffer_a;




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

int convert_hmac_key_str(VMBYTE* hmac, char* hmac_str)
{
	int str_pos = 0;
	int pos = 0;
	int str_end;
	int end;
	char tmp_str[3] = {0,0,0};
	char* tmp_end;
	long int value;
	
	str_end = strlen(hmac_str);
	end = (str_end+1)/2;

	if (end > MAX_HMAC_KEY_LENGTH)
	{
		write_log("Too big");
		return -1;
	}

	if (str_end%2 == 1)
	{
		tmp_str[1] = hmac_str[0];
		value = strtol(tmp_str, &tmp_end, 16);
		if (tmp_end != &tmp_str[2])
		{
			write_log("Wrong1");
			return -1;
		}
		hmac[pos] = value;
		str_pos++;
		pos++;
	}

	for (;str_pos<str_end;str_pos+=2, pos++)
	{
		tmp_str[0] = hmac_str[str_pos];
		tmp_str[1] = hmac_str[str_pos+1];
		value = strtol(tmp_str, &tmp_end, 16);
		if (tmp_end != &tmp_str[2])
		{
			write_log("Wrong2");
			write_log(&hmac_str[str_pos]);
			return -1;
		}
		hmac[pos] = value;
	}
	return end;
}

void measure_end(void* buffer, int result)
{
	afifo_destroy((afifo*)buffer);
}

void signal_failure()
{
	vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
	vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
	vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
}

static void handle_sysevent(VMINT event, VMINT param)
{
	char text_buffer[64];
	char apn[32];
	char http_host[REPORT_HOSTNAME_MAX];
	char http_path[REPORT_PATH_MAX];
	char http_hmac_key_str[2*MAX_HMAC_KEY_LENGTH+1];
	VMBYTE http_hmac_key[MAX_HMAC_KEY_LENGTH];
	VMINT http_hmac_key_length = -1;
	int cmdline_size;
	int averaging;
	int res_buf_size;
	int measure_interval;
	int report_console;
	int report_http;
	int report_interval;


	switch (event) {
		case VM_EVENT_CREATE:
			init_leds();
			start_log(LOG_FILENAME);
			open_conf(CONF_FILENAME);

			if (read_conf_int("cmdline_size", &cmdline_size) == FALSE)
			{
				close_conf();
				signal_failure();
				break;
			}
			start_console(cmdline_size);

			if (
					read_conf_string("apn", apn, 32) == FALSE ||
					read_conf_string("report_http_host", http_host, REPORT_HOSTNAME_MAX) == FALSE ||
					read_conf_string("report_http_path", http_path, REPORT_PATH_MAX) == FALSE ||
					read_conf_string("report_http_hmac_key", http_hmac_key_str, 2*MAX_HMAC_KEY_LENGTH+1) == FALSE ||
					read_conf_int("measurement_averaging", &averaging) == FALSE ||
					read_conf_int("measurement_buffer_size", &res_buf_size) == FALSE ||
					read_conf_int("measurement_interval", &measure_interval) == FALSE ||
					read_conf_int("report_console", &report_console) == FALSE ||
					read_conf_int("report_http", &report_http) == FALSE ||
					read_conf_int("report_interval", &report_interval) == FALSE
			   )
			{
				close_conf();
				signal_failure();
				break;
			}
			close_conf();

			http_hmac_key_length = convert_hmac_key_str(http_hmac_key, http_hmac_key_str);
			if (http_hmac_key_length < 0)
			{
				signal_failure();
				break;
			}

			init_telecom(apn);

			adc_handle_a = open_hx711(ADC_SCL_A, ADC_SDA_A);
			result_buffer_a = afifo_create(averaging, res_buf_size);
			if (adc_handle_a == ADC_HANDLE_INVALID || result_buffer_a == NULL)
			{
				signal_failure();
				break;
			}
			set_hx711_a128(adc_handle_a, measure_interval, afifo_write, result_buffer_a);
			set_report_http_host(http_host);
			set_report_http_path(http_path);
			set_report_http_hmac_key(http_hmac_key, http_hmac_key_length);
			if (report_console)
			{
				enable_console_report();
			}
			if (report_http)
			{
				enable_http_report();
			}
			start_reporting(result_buffer_a, report_interval);

			write_log("System started!");
			break;

		case VM_EVENT_QUIT:
			stop_reporting(result_buffer_a);
			close_hx711(adc_handle_a, measure_end, result_buffer_a);
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

