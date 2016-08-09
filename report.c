#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vmtype.h"
#include "vmtimer.h"
#include "vmhttps.h"
#include "vmthread.h"
#include "vmmemory.h"
#include "vmssl.h"
#include "vmdatetime.h"
#include "vmfs.h"
#include "vmchset.h"
#include "log.h"
#include "console.h"
#include "measure.h"
#include "telecom.h"
#include "leds.h"
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


static VMBOOL reporter_busy = FALSE;
static VMWCHAR current_report_filename[32];
static VMBOOL report_http = 0;
static VMBOOL report_console = 0;
static char http_host[REPORT_HOSTNAME_MAX];
static char http_path[REPORT_PATH_MAX];
static VMBYTE hmac_key[MAX_HMAC_KEY_LENGTH];
static VMINT hmac_key_length = 0;

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

	if (result == VM_HTTPS_OK)
	{
		vm_fs_delete(current_report_filename);
	}
	reporter_busy = FALSE;
	blue_led_off();
}

static void report_timer_callback(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	VMINT result;
	VMINT tmpf_handle;
	char tmpf_filename[32];
	VMUINT tmpf_written;
	VMINT result_count = 0;
	VMINT base64_length;
	VMUINT rtc_time;
	VMBYTE hmac[20];
	char tmp_buffer[32];
	char http_body[1024];
	VMWCHAR tmpf_w_filename[32];
	afifo* source;

	source = (afifo*)user_data;


	if (report_http) 
	{
		strcpy(http_body, "mes=");
	}

	while(afifo_read(source, &result))
	{
		result_count++;
		sprintf(tmp_buffer, "%d_", result);
		if (report_console) 
		{
			write_console(tmp_buffer);
		}
		if (report_http && result_count < 80)
		{
			strcat(http_body, tmp_buffer);
		}
	}

	if (report_http)
	{
		strcat(http_body, "&time=");
		vm_time_get_unix_time(&rtc_time);
		sprintf(tmp_buffer, "%u", rtc_time);
		strcat(http_body, tmp_buffer);
		vm_chset_ucs2_to_ascii(tmpf_filename, 32, REPORT_TMP_FOLDER);
		strcat(tmpf_filename, tmp_buffer);
		strcat(tmpf_filename, ".rpt");
		strcat(http_body, "&sig=");
		vm_ssl_sha1_hmac(hmac_key, hmac_key_length, http_body, strlen(http_body), hmac);
		memset(tmp_buffer, 0, 32);
		base64_length = 32;
		vm_ssl_base64_encode(tmp_buffer, &base64_length, hmac, 20);
		for (int i=0;i<base64_length;i++)
		{
			if (tmp_buffer[i] == '+')
			{
				tmp_buffer[i] = '-';
			}
			else if (tmp_buffer[i] == '/')
			{
				tmp_buffer[i] = '_';
			}
			else if (tmp_buffer[i] == '=')
			{
				tmp_buffer[i] = 0;
			}
		}
		strcat(http_body, tmp_buffer);

		vm_chset_ascii_to_ucs2(tmpf_w_filename, 64, tmpf_filename);
		tmpf_handle = vm_fs_open(tmpf_w_filename, VM_FS_MODE_CREATE_ALWAYS_WRITE, VM_FALSE);
		if (tmpf_handle >= 0)
		{
			vm_fs_write(tmpf_handle, http_body, strlen(http_body), &tmpf_written);
			vm_fs_close(tmpf_handle);
		}

		if (reporter_busy == FALSE)
		{
			blue_led_on();
			reporter_busy = TRUE;
			vm_chset_convert(VM_CHSET_ENCODING_UCS2, VM_CHSET_ENCODING_UCS2, (VMCHAR*)tmpf_w_filename, (VMCHAR*)current_report_filename, 64);
			http_post(http_host, http_path, http_body, http_done_callback);
		}
		else
		{
			write_log("Reporter busy while sending new report");
		}
	}
}


void send_delayed_report()
{
	vm_fs_info_t found_file;
	VM_FS_HANDLE find_handle;
	VM_FS_HANDLE report_handle;
	VM_RESULT res;
	VMUINT bytes_read;
	char tmp_buffer[32];
	char tmpf_filename[32];
	char http_body[1024];


	find_handle = vm_fs_find_first(REPORT_TMP_FOLDER u"*.rpt", &found_file);
	if (find_handle >= 0)
	{
		if (reporter_busy == FALSE)
		{
			blue_led_on();
			reporter_busy = TRUE;
			vm_chset_ucs2_to_ascii(tmp_buffer, 32, found_file.filename);
			vm_chset_ucs2_to_ascii(tmpf_filename, 32, REPORT_TMP_FOLDER);
			strcat(tmpf_filename, tmp_buffer);
			vm_chset_ascii_to_ucs2(current_report_filename, 64, tmpf_filename);
			report_handle = vm_fs_open(current_report_filename, VM_FS_MODE_READ, FALSE);
			if (report_handle >= 0)
			{
				memset(http_body, 0, 1024);
				res = vm_fs_read(report_handle, http_body, 1023, &bytes_read);
				vm_fs_close(report_handle);
				if (res >= 0)
				{
					http_post(http_host, http_path, http_body, http_done_callback);
				}
			}
		}
		else
		{
			write_log("Reporter busy while sending delayed report");
		}
		vm_fs_find_close(find_handle);
	}
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

void set_report_http_hmac_key(VMBYTE* key, VMINT key_length)
{
	if (key_length > MAX_HMAC_KEY_LENGTH)
	{
		memcpy(hmac_key, key, MAX_HMAC_KEY_LENGTH);
		hmac_key_length = MAX_HMAC_KEY_LENGTH;
	}
	else
	{
		memcpy(hmac_key, key, key_length);
		hmac_key_length = key_length;
	}
}
