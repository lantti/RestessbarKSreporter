#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vmtype.h"
#include "vmtimer.h"
#include "vmhttps.h"
#include "vmthread.h"
#include "vmmemory.h"
#include "log.h"
#include "console.h"
#include "measure.h"
#include "telecom.h"
#include "report.h"

struct afifo
{
	vm_mutex_t mutex;
	double avg_acc;
	int avg_complete;
	int avg_current;
	int* buffer;
	int buffer_size;
	int head;
	int tail;
	VM_TIMER_ID_NON_PRECISE report_timer;
};


static VMBOOL report_http = 0;
static VMBOOL report_console = 0;
static char http_host[REPORT_HOSTNAME_MAX];
static char http_path[REPORT_PATH_MAX];

afifo* afifo_create(int averaging, int size)
{
	afifo* new_afifo;
	write_console("afifo being created\n");
	new_afifo = vm_calloc(sizeof(afifo));
	if (new_afifo == NULL)
	{
		return NULL;
	}
	new_afifo->buffer = vm_calloc(size*sizeof(int));
	if (new_afifo->buffer == NULL)
	{
		vm_free(new_afifo);
		return NULL;
	}
	new_afifo->buffer_size = size;
	vm_mutex_init(&new_afifo->mutex);
	new_afifo->avg_acc = 0;
	new_afifo->avg_complete = averaging;
	new_afifo->avg_current = 0;
	new_afifo->head = 0;
	new_afifo->tail = 0;
	new_afifo->report_timer = -1;
	write_console("afifo created\n");
	return new_afifo;
}

void afifo_destroy(afifo* target)
{
        vm_mutex_lock(&target->mutex);
	vm_free(target->buffer);
	vm_free(target);
}

void afifo_write(afifo* target, int value)
{
	char buffer[64];
	int head;
	int tail;
        vm_mutex_lock(&target->mutex);
	if (target->avg_complete > 0)
	{
		target->avg_acc += (double)value / target->avg_complete;
		target->avg_current++;
	}
	else
	{
		target->avg_acc = value;
	}

	if (target->avg_current >= target->avg_complete)
	{
		target->avg_current = 0;
		target->head = (target->head + 1) % target->buffer_size;
		target->buffer[target->head] = (int)target->avg_acc;
		target->avg_acc = 0;
		if (target->head == target->tail)
		{
			target->tail = (target->tail + 1) % target->buffer_size;
		}
	}
	head = target->head;
	tail = target->tail;
	vm_mutex_unlock(&target->mutex);
	sprintf(buffer, "%d, %d\n", head, tail);
	write_console("afifo written\n");
	write_console(buffer);
}

VMBOOL afifo_read(afifo* source, int* value)
{
	vm_mutex_lock(&source->mutex);
	if (source->head == source->tail)
	{
		vm_mutex_unlock(&source->mutex);
		write_console("afifo empty\n");
		return FALSE;
	}
	source->tail = (source->tail + 1) % source->buffer_size;
	*value = source->buffer[source->tail];
	vm_mutex_unlock(&source->mutex);
	return TRUE;
}


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
    afifo* source;

    source = (afifo*)user_data;

    if (report_console) write_console("\n-------------\n");
    if (report_http) strcpy(http_body, "measurements=");

    while(afifo_read(source, &result))
    {
        result_count++;
        sprintf(text_buffer, "%d_", result);
        if (report_console) write_console(text_buffer);
        if (report_http && result_count < 100) strcat(http_body, text_buffer);
    }

    if (report_console) write_console("\n-------------\n");
    if (report_http) http_post(http_host, http_path, http_body, http_done_callback);
}


void start_reporting(afifo* source, int interval)
{
	if (source->report_timer >= 0)
	{
		stop_reporting(source);
	}
	source->report_timer = vm_timer_create_non_precise(interval, report_timer_callback, source);
}

void stop_reporting(afifo* source)
{
    vm_timer_delete_non_precise(source->report_timer);
    source->report_timer = -1;
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

void set_report_http_host(char* host)
{
	if (strlen(host) < REPORT_HOSTNAME_MAX)
	{
		strcpy(http_host, host);
	}
}

void set_report_http_path(char* path)
{
	if (strlen(path) < REPORT_PATH_MAX)
	{
		strcpy(http_path, path);
	}
}
