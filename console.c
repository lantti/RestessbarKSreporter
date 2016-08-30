#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vmsystem.h"
#include "vmpwr.h"
#include "vmchset.h"
#include "vmfs.h"
#include "vmtype.h"
#include "vmdcl.h"
#include "vmdcl_sio.h"
#include "vmusb.h"
#include "vmgsm.h"
#include "vmgsm_sim.h"
#include "vmgsm_sms.h"
#include "vmgsm_cell.h"
#include "vmhttps.h"
#include "vmmemory.h"
#include "vmdatetime.h"
#include "vmtimer.h"
#include "log.h"
#include "telecom.h"
#include "report.h"
#include "conf.h"
#include "measure.h"
#include "console.h"


static VM_DCL_HANDLE usb_handle = VM_DCL_HANDLE_INVALID;
static VM_DCL_OWNER_ID g_owner_id = 0;

static int max_cmdline_size = 0;
static VMBOOL cmdline_overflow = FALSE;
static char* cmdline = NULL;
static char* request_host = NULL;
static char* request_path = NULL;
static char* request_content = NULL;
static ADC_HANDLE m_handle = ADC_HANDLE_INVALID;
static afifo* result_buffer;

static int shutdown_counter = 0;

static void http_done_callback(VM_HTTPS_RESULT result, VMUINT16 status, VM_HTTPS_METHOD method, char* url, char* headers, char* body)
{
	char buffer[64] = {0};
	sprintf(buffer, "result: %d, status: %d, method: %d\n", (signed char)result, status, (signed char)method);
	write_console(buffer);
	write_console(url);
	write_console("\n");
	write_console(headers);
	write_console("\n");
	write_console(body);
	write_console("\n");
}
static void measure_end(void* env, int result)
{
	afifo_destroy(result_buffer);
}
static void delayed_shutdown_cb(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	shutdown_counter++;
	if (shutdown_counter > 10)
	{
		vm_pwr_shutdown(100);
	}
}

static void run_command()
{
	char buffer[64] = {0};
	VMINT intvalue;
	char* time_str_ptr;
	VMUINT unixtime;
	vm_date_time_t datetime;

	switch(cmdline[0])
	{
		case '?':
			write_console("!\n");
			break;
		case 'R':
			if (cmdline[1] == 'c')
			{
				if (cmdline[2] == '+')
				{
					enable_console_report();
					write_console("console report enabled\n");
				}
				else if (cmdline[2] == '-')
				{
					disable_console_report();
					write_console("console report disabled\n");
				}
			}
			else if (cmdline[1] == 'h')
			{
				if (cmdline[2] == '+')
				{
					enable_http_report();
					write_console("http report enabled\n");
				}
				else if (cmdline[2] == '-')
				{
					disable_http_report();
					write_console("http report disabled\n");
				}
			}
			break;
		case 'H':
			strcpy(request_host, &cmdline[1]);
			write_console("request host: ");
			write_console(request_host);
			write_console("\n");
			break;
		case 'P':
			strcpy(request_path, &cmdline[1]);
			write_console("request path: ");
			write_console(request_path);
			write_console("\n");
			break;
		case 'B':
			strcpy(request_content, &cmdline[1]);
			write_console("request body: ");
			write_console(request_content);
			write_console("\n");
			break;
		case 'X':
			if (cmdline[1] == 'g')
			{
				if (http_get(request_host, request_path, http_done_callback) == TRUE)
				{
					write_console("ok\n");
				}
				else
				{
					write_console("Nope!\n");
				}
			}
			else if (cmdline[1] == 'p')
			{
				if (http_post(request_host, request_path, request_content, http_done_callback) == TRUE)
				{
					write_console("ok\n");
				}
				else
				{
					write_console("Nope!\n");
				}
			}
			else if (cmdline[1] == 's')
			{
				vm_https_set_channel(0,0,0,0,0,0,0,0,0,0,0,0,0,0);
			}
			else if (cmdline[1] == 'u')
			{
				vm_https_unset_channel(0);
			}
			else if (cmdline[1] == 'i')
			{
				init_telecom(NULL);
			}
			break;
		case 'N':
			if (!vm_gsm_sim_get_network_plmn(1, buffer, 64))
			{
				write_console(buffer);
				write_console("\n");
			}
			else
			{
				write_console("Nope!\n");
			}
			break;
		case 'S':
			open_conf(u"C:\\config.txt");
			if (read_conf_string(&cmdline[1], buffer, 64))
			{
				write_console("value for configuration key ");
				write_console(&cmdline[1]);
				write_console(": ");
				write_console(buffer);
				write_console("\n");
			}
			else
			{
				write_console("value for configuration key ");
				write_console(&cmdline[1]);
				write_console(" not found\n");
			}
			close_conf();
			break;
		case 'I':
			open_conf(u"C:\\config.txt");
			if (read_conf_int(&cmdline[1], &intvalue))
			{
				write_console("value for configuration key ");
				write_console(&cmdline[1]);
				write_console(": ");
				sprintf(buffer, "%d\n", intvalue);
				write_console(buffer);
			}
			else
			{
				write_console("value for configuration key ");
				write_console(&cmdline[1]);
				write_console(" not found\n");
			}
			close_conf();
			break;
		case 'E':
			write_console(&cmdline[1]);
			write_console("\n");
			break;
		case 'T':
			strcpy(buffer, &cmdline[1]);
			time_str_ptr = strtok(buffer, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.year = strtol(time_str_ptr, NULL, 10);
			time_str_ptr = strtok(NULL, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.month = strtol(time_str_ptr, NULL, 10);
			time_str_ptr = strtok(NULL, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.day = strtol(time_str_ptr, NULL, 10);
			time_str_ptr = strtok(NULL, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.hour = strtol(time_str_ptr, NULL, 10);
			time_str_ptr = strtok(NULL, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.minute = strtol(time_str_ptr, NULL, 10);
			time_str_ptr = strtok(NULL, "/");
			if (time_str_ptr == NULL)
			{
				write_console("Invalid date time\n");
				break;
			}
			datetime.second = strtol(time_str_ptr, NULL, 10);

			if (vm_time_set_date_time(&datetime) < 0)
			{
				write_console("Invalid date time\n");
				break;
			}

			vm_time_get_unix_time(&unixtime);
			sprintf(buffer, "new time: %u\n", unixtime);
			write_console(buffer);
			break;
		case 'V':
			while (afifo_read(result_buffer, &intvalue) == TRUE)
			{
				sprintf(buffer, "%d, ", intvalue);
				write_console(buffer);
			}
			write_console("\n");
			break;
		case 'D':
			send_report();
			break;
		case 'M':
			if (cmdline[1] == '0')
			{
				intvalue = VM_GSM_SIM_NO_SIM;
			}
			else if (cmdline[1] == '1')
			{
				intvalue = VM_GSM_SIM_SIM1;
			}
			else
			{
				sprintf(buffer, "%d sim card(s) detected\n", vm_gsm_sim_get_card_count());
				write_console(buffer);
				break;
			}
			if (vm_gsm_sim_set_active_sim_card(intvalue) == VM_TRUE)
			{
				write_console("ok\n");
			}
			else
			{
				write_console("fail\n");
			}
			break;
		case 'L':
			datetime.year = 2000;
			datetime.month = 1;
			datetime.day = 1;
			datetime.hour = 6;
			datetime.minute = 30;
			datetime.second = 30;
			if (vm_pwr_scheduled_startup(&datetime, VM_PWR_STARTUP_ENABLE_CHECK_HMS))
			{
				write_console("ok\n");
			}
			else
			{
				write_console("Nope!\n");
			}
			break;
		case 'Q':
			write_console("Shutting down...\n");
			vm_timer_create_non_precise(1000, delayed_shutdown_cb, NULL);
			break;
	}
}

static void append_cmdline(char* new_tail)
{
	int i = 0;
	char* trimmed;
	char* end;

	while (new_tail[i] == '\n' || new_tail[i] == '\r')
	{
		i++;
	}
	trimmed = &new_tail[i];
	end = trimmed;
	while (new_tail[i] != 0)
	{
		i++;
		if (new_tail[i] != '\n' || new_tail[i] != '\r')
		{
			end = &new_tail[i];
		}
	}
	end[1] = 0;

	if (strlen(cmdline) + strlen(trimmed) <= max_cmdline_size)
	{ 
		strcat(cmdline, trimmed);
	}
	else
	{
		cmdline[0] = 0;
		cmdline_overflow = TRUE;
		write_console("\nError: max. command line length exceeded\n");
	}
}

static void usb_receive_callback(void* user_data, VM_DCL_EVENT event, VM_DCL_HANDLE device_handle)
{
	char recv[9];
	char* head;
	char* leftovers;
	char error_text[] = "\nError: max. command line length exceeded\n";
	VM_DCL_BUFFER_LENGTH returned = 0;
	VM_DCL_BUFFER_LENGTH written = 0;
	if(event == VM_DCL_SIO_UART_READY_TO_READ)
	{
		while (vm_dcl_read(device_handle, (VM_DCL_BUFFER *)recv, 8, &returned, g_owner_id) == VM_DCL_STATUS_OK && returned > 0)
			recv[returned] = 0;
		write_console(recv);
		head = recv;
		leftovers = strpbrk(head, "\n\r");
		while (leftovers != NULL)
		{
			*leftovers = 0;
			append_cmdline(head);
			if (!cmdline_overflow)
			{
				run_command();
			}
			cmdline[0] = 0;
			cmdline_overflow = FALSE;
			head = &leftovers[1];
			leftovers = strpbrk(head, "\n\r");
		}

		append_cmdline(head);
	}
}


void start_console(int cmdline_length)
{
	vm_dcl_sio_control_dcb_t settings;

	stop_console();

	max_cmdline_size = cmdline_length;

	cmdline = vm_calloc(max_cmdline_size+1);
	request_host = vm_calloc(max_cmdline_size+1);
	request_path = vm_calloc(max_cmdline_size+1);
	request_content = vm_calloc(max_cmdline_size+1);

	g_owner_id = vm_dcl_get_owner_id();
	settings.owner_id = g_owner_id;
	settings.config.dsr_check = 0;
	settings.config.data_bits_per_char_length = VM_DCL_SIO_UART_BITS_PER_CHAR_LENGTH_8;
	settings.config.flow_control = VM_DCL_SIO_UART_FLOW_CONTROL_NONE;
	settings.config.parity = VM_DCL_SIO_UART_PARITY_NONE;
	settings.config.stop_bits = VM_DCL_SIO_UART_STOP_BITS_1;
	settings.config.baud_rate = VM_DCL_SIO_UART_BAUDRATE_115200;
	settings.config.sw_xoff_char = 0x13;
	settings.config.sw_xon_char = 0x11;

	usb_handle = vm_dcl_open(VM_DCL_SIO_USB_PORT1, g_owner_id);
	if (cmdline != NULL && request_host != NULL && request_path != NULL && request_content != NULL && usb_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_control(usb_handle, VM_DCL_SIO_COMMAND_SET_DCB_CONFIG, (void *)&settings);
		vm_dcl_register_callback(usb_handle, VM_DCL_SIO_UART_READY_TO_READ, usb_receive_callback, (void*)NULL);
	}
	else
	{
		stop_console();
	}
}

void stop_console()
{
	if (usb_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_close(usb_handle);
		usb_handle = VM_DCL_HANDLE_INVALID;
	}

	max_cmdline_size = 0;
	vm_free(cmdline);
	cmdline = NULL;
	vm_free(request_host);
	request_host = NULL;
	vm_free(request_path);
	request_path = NULL;
	vm_free(request_content);
	request_content = NULL;
}

void write_console(char* message)
{
	VM_DCL_BUFFER_LENGTH written = 0;
	if (usb_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_write(usb_handle, (VM_DCL_BUFFER *)message, strlen(message), &written, g_owner_id);
	}
}
