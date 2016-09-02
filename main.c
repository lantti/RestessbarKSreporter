#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vmtype.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmchset.h"
#include "vmfs.h"
#include "vmpwr.h"
#include "vmgsm_sim.h"
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
#include "leds.h"

#define ADC_SCL_A              43
#define ADC_SDA_A              44

#define CONF_FILENAME        u"C:\\config.txt"
#define LOG_FILENAME        u"C:\\event_log.txt"

ADC_HANDLE adc_handle_a = ADC_HANDLE_INVALID;
afifo* result_buffer_a;

int bootup_blink_counter = 0;
int sleep_enabled = 0;
int sleep_time;
int wakeup_time;
int http_failure_reboot_enabled = 0;
int http_failure_limit;
VM_TIMER_ID_NON_PRECISE watchdog_timer_id;
VM_TIMER_ID_NON_PRECISE report_send_timer_id;
VM_TIMER_ID_NON_PRECISE report_write_timer_id;

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

void report_send_cb(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	if (vm_gsm_sim_get_card_count() == 0)
	{
		red_led_on();
	}
	else
	{
		red_led_off();
		send_report();
	}
}

void report_write_cb(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	vm_date_time_t datetime;
	int current_time;

	compile_report(result_buffer_a);

	if (sleep_enabled)
	{
		vm_time_get_date_time(&datetime);
		current_time = datetime.hour * 60 + datetime.minute;
		if ((current_time < wakeup_time && current_time > sleep_time) || (sleep_time > wakeup_time && (current_time < wakeup_time || current_time > sleep_time)))
		{
			datetime.year = 2000;
			datetime.month = 1;
			datetime.day = 1;
			datetime.hour = wakeup_time / 60;
			datetime.minute = wakeup_time % 60;
			datetime.second = 0;
			vm_pwr_scheduled_startup(&datetime, VM_PWR_STARTUP_ENABLE_CHECK_HMS);
			vm_pwr_shutdown(100);
			return;
		}
	}

	if (http_failure_reboot_enabled && http_failures() > http_failure_limit)
	{
		vm_pwr_reboot();
	}
}

void bootup_blink_cb(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	vm_date_time_t datetime;
	switch(bootup_blink_counter%5)
	{
		case 0:
			red_led_off();
			green_led_off();
			blue_led_off();
			break;
		case 1:
			red_led_on();
			green_led_off();
			blue_led_off();
			break;
		case 2:
			red_led_off();
			green_led_off();
			blue_led_on();
			break;
		case 3:
			red_led_off();
			green_led_on();
			blue_led_off();
			break;
		case 4:
			red_led_on();
			green_led_on();
			blue_led_on();
			break;
	}
	bootup_blink_counter++;
	if (bootup_blink_counter > 20)
	{
		if (vm_gsm_sim_get_card_count() == 0)
		{
			vm_time_get_date_time(&datetime);
			datetime.second = (datetime.second + 20) % 60;
			vm_pwr_scheduled_startup(&datetime, VM_PWR_STARTUP_ENABLE_CHECK_S);
			vm_pwr_shutdown(100);
			return;
		}
		else
		{
			vm_https_set_channel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			vm_timer_delete_non_precise(timer_id);
		}
	}
}

void signal_failure()
{
	red_led_on();
	green_led_on();
	blue_led_on();
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
	int aggregation;
	int res_buf_size;
	int measure_interval;
	int report_console;
	int report_http;
	int report_interval;
	int send_interval;
	int sleep_h;
	int sleep_m;
	int wakeup_h;
	int wakeup_m;


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
					read_conf_int("measurement_aggregation", &aggregation) == FALSE ||
					read_conf_int("measurement_buffer_size", &res_buf_size) == FALSE ||
					read_conf_int("measurement_interval", &measure_interval) == FALSE ||
					read_conf_int("report_console", &report_console) == FALSE ||
					read_conf_int("report_http", &report_http) == FALSE ||
					read_conf_int("http_failure_limit", &http_failure_limit) == FALSE ||
					read_conf_int("reboot_on_http_fail", &http_failure_reboot_enabled) == FALSE ||
					read_conf_int("sleep_hour", &sleep_h) == FALSE ||
					read_conf_int("sleep_minute", &sleep_m) == FALSE ||
					read_conf_int("wakeup_hour", &wakeup_h) == FALSE ||
					read_conf_int("wakeup_minute", &wakeup_m) == FALSE ||
					read_conf_int("daily_sleep", &sleep_enabled) == FALSE ||
					read_conf_int("report_send_interval", &send_interval) == FALSE ||
					read_conf_int("report_write_interval", &report_interval) == FALSE
			   )
			{
				close_conf();
				signal_failure();
				break;
			}
			close_conf();

			sleep_time = (sleep_h * 60 + sleep_m) % 1440;
			wakeup_time = (wakeup_h * 60 + wakeup_m) % 1440;

			http_hmac_key_length = convert_hmac_key_str(http_hmac_key, http_hmac_key_str);
			if (http_hmac_key_length < 0)
			{
				signal_failure();
				break;
			}

			adc_handle_a = open_hx711(ADC_SCL_A, ADC_SDA_A);
			result_buffer_a = afifo_create(aggregation, res_buf_size);
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

			init_telecom(apn);

			if (send_interval > 0)
			{
				report_send_timer_id = vm_timer_create_non_precise(send_interval, report_send_cb, NULL);
			}

			if (report_interval > 0)
			{
				report_write_timer_id = vm_timer_create_non_precise(report_interval, report_write_cb, NULL);
			}

			vm_timer_create_non_precise(300, bootup_blink_cb, NULL);

			//write_log("System started!");
			break;

		case VM_EVENT_QUIT:
			vm_timer_delete_non_precise(watchdog_timer_id);
			vm_timer_delete_non_precise(report_send_timer_id);
			vm_timer_delete_non_precise(report_write_timer_id);
			close_hx711(adc_handle_a, measure_end, result_buffer_a);
			stop_console();
			free_leds();
			write_log("System stopped.");
			stop_log();
			break;
			//default:
			//sprintf(text_buffer, "SysEvent: %u:%u", event, param);
			//write_log(text_buffer);

	}
}


void vm_main(void)
{
	start_log(LOG_FILENAME);
	vm_pmng_register_system_event_callback(handle_sysevent);
}

