#include <stdio.h>
#include <string.h>
#include "vmtype.h"
#include "vmtimer.h"
#include "vmhttps.h"
#include "log.h"
#include "console.h"
#include "measure.h"
#include "telecom.h"
#include "report.h"

static VM_TIMER_ID_NON_PRECISE report_timer;

static VMBOOL report_http = 0;
static VMBOOL report_console = 0;

static void http_done_callback(VM_HTTPS_RESULT result, VMUINT16 status, VM_HTTPS_METHOD method, char* url, char* headers, char* body)
{
    if (report_console)
    {
        if (result == VM_HTTPS_OK)
        {
            write_console(url);
            write_console(headers);
            write_console(body);
        }
        else
        {
            write_console("HTTP request failed\n");
        }
    }

}

static void report_timer_callback(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
    VMINT result;
    VMINT result_count = 0;
    char text_buffer[10];
    char http_body[1024];

    if (report_console) write_console("\n-------------\n");
    if (report_http) strcpy(http_body, "measurements=");

    while(FALSE)//get_measurement_result(&result))
    {
        result_count++;
        sprintf(text_buffer, "%d_", result);
        if (report_console) write_console(text_buffer);
        if (report_http && result_count < 100) strcat(http_body, text_buffer);
    }

    if (report_console) write_console("\n-------------\n");
    if (report_http) http_post(REPORTHOST, REPORTPATH, http_body, http_done_callback);
}


void start_reporting()
{
    report_timer = vm_timer_create_non_precise(REPORT_INTERVAL, report_timer_callback, NULL);
}

void stop_reporting()
{
    vm_timer_delete_non_precise(report_timer);
}

void enable_http_report()
{
    report_http = TRUE;
}

void disable_http_report()
{
    report_http = FALSE;
}

void enable_console_report()
{
    report_console = TRUE;
}

void disable_console_report()
{
    report_console = FALSE;
}

