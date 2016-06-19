#include <stdio.h>
#include <string.h>
#include "vmsystem.h"
#include "vmtype.h"
#include "vmdcl.h"
#include "vmdcl_sio.h"
#include "vmusb.h"
#include "vmgsm.h"
#include "vmgsm_sim.h"
#include "vmgsm_sms.h"
#include "vmgsm_cell.h"
#include "vmhttps.h"
#include "log.h"
#include "telecom.h"
#include "report.h"
#include "conf.h"
#include "console.h"


static VM_DCL_HANDLE usb_handle = VM_DCL_HANDLE_INVALID;
static VM_DCL_OWNER_ID g_owner_id = 0;

static char request_host[CMDLINE_SIZE+1] = {0};
static char request_path[CMDLINE_SIZE+1] = {0};
static char request_content[CMDLINE_SIZE+1] = {0};


static void http_done_callback(VM_HTTPS_RESULT result, VMUINT16 status, VM_HTTPS_METHOD method, char* url, char* headers, char* body)
{
    char buffer[64] = {0};
    sprintf(buffer, "result: %d, status: %d, method: %d\n", result, status, method);
    write_console(buffer);
    write_console(url);
    write_console("\n");
    write_console(headers);
    write_console("\n");
    write_console(body);
    write_console("\n");
}

static void run_command(char* cmdline)
{
    char buffer[64] = {0};
    VMINT intvalue;
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
                http_get(request_host, request_path, http_done_callback);
            }
            else if (cmdline[1] == 'p')
            {
                http_post(request_host, request_path, request_content, http_done_callback);
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
            break;
        case 'I':
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
            break;
        case 'E':
            write_console(&cmdline[1]);
            write_console("\n");
            break;
    }
}

static void usb_receive_callback(void* user_data, VM_DCL_EVENT event, VM_DCL_HANDLE device_handle)
{
    static char cmdline[CMDLINE_SIZE+1];
    static VMINT cmdline_end = 0;
    char recv[8];
    char error_text[] = "\nError: max. command line length exceeded\n";
    VM_DCL_BUFFER_LENGTH returned = 0;
    VM_DCL_BUFFER_LENGTH written = 0;
    if(event == VM_DCL_SIO_UART_READY_TO_READ)
    {
        vm_dcl_read(device_handle, (VM_DCL_BUFFER *)recv, 8, &returned, g_owner_id);
        vm_dcl_write(usb_handle, (VM_DCL_BUFFER *)recv, returned, &written, g_owner_id);
        if (recv[0] == '\n' || recv[0] == '\r')
        {
            cmdline[cmdline_end] = 0;
            cmdline_end = 0;
            run_command(cmdline);
        }
        else
        {
            cmdline[cmdline_end] = recv[0];
            cmdline_end++;
            if (cmdline_end >= CMDLINE_SIZE)
            {
                cmdline_end = 0;
                vm_dcl_write(usb_handle, (VM_DCL_BUFFER *)error_text, strlen(error_text), &written, g_owner_id);
            }
        }
    }
}



void start_console()
{
    vm_dcl_sio_control_dcb_t settings;
    if (usb_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_close(usb_handle);
        usb_handle = VM_DCL_HANDLE_INVALID;
    }

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
    vm_dcl_control(usb_handle, VM_DCL_SIO_COMMAND_SET_DCB_CONFIG, (void *)&settings);
    vm_dcl_register_callback(usb_handle, VM_DCL_SIO_UART_READY_TO_READ, usb_receive_callback, (void*)NULL);

    strcpy(request_host, "requestb.in");
    strcpy(request_path, "/r93i81r9");
    strcpy(request_content, "tititi=tyy");
}

void stop_console()
{
    if (usb_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_close(usb_handle);
    }
}

void write_console(char* message)
{
    VM_DCL_BUFFER_LENGTH written = 0;
    if (usb_handle != VM_DCL_HANDLE_INVALID)
    {
        vm_dcl_write(usb_handle, (VM_DCL_BUFFER *)message, strlen(message), &written, g_owner_id);
    }
}
