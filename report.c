#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vmtype.h"
#include "vmpwr.h"
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
	int* bin;
	int bin_size;
	int bin_head;
	int* output_buffer;
	int output_buffer_size;
	int output_buffer_head;
	int output_buffer_tail;
};


static VMWCHAR current_report_filename[32];
static VMBOOL report_http = 0;
static VMBOOL report_console = 0;
static char http_host[REPORT_HOSTNAME_MAX];
static char http_path[REPORT_PATH_MAX];
static VMBYTE hmac_key[MAX_HMAC_KEY_LENGTH];
static VMINT hmac_key_length = 0;
static int http_failure_count = 0;

afifo* afifo_create(int aggregation, int size)
{
	afifo* new_afifo;
	new_afifo = vm_calloc(sizeof(afifo));
	if (new_afifo == NULL)
	{
		return NULL;
	}
	new_afifo->bin = vm_calloc(aggregation*sizeof(int));
	if (new_afifo->bin == NULL)
	{
		vm_free(new_afifo);
		return NULL;
	}
	new_afifo->output_buffer = vm_calloc(size*sizeof(int));
	if (new_afifo->output_buffer == NULL)
	{
		vm_free(new_afifo->bin);
		vm_free(new_afifo);
		return NULL;
	}
	new_afifo->bin_size = aggregation;
	new_afifo->bin_head = 0;
	new_afifo->output_buffer_size = size;
	new_afifo->output_buffer_head = 0;
	new_afifo->output_buffer_tail = 0;
	vm_mutex_init(&new_afifo->mutex);
	write_console("afifo created\n");
	return new_afifo;
}

void afifo_destroy(afifo* target)
{
	vm_mutex_lock(&target->mutex);
	vm_free(target->bin);
	vm_free(target->output_buffer);
	vm_free(target);
}

void afifo_write(afifo* target, int value)
{
	int head;
	int tail;
	vm_mutex_lock(&target->mutex);
	if (target->bin_size > 1)
	{
		head = target->bin_head;
		while (head > 0 && target->bin[head-1] > value)
		{
			target->bin[head] = target->bin[head-1];
			head--;
		}
		target->bin[head] = value;
		target->bin_head++;
	}
	else
	{
		target->bin[0] = value;
		target->bin_head = 1;
	}

	if (target->bin_head >= target->bin_size)
	{
		target->bin_head = 0;
		target->output_buffer_head = (target->output_buffer_head + 1) % target->output_buffer_size;
		target->output_buffer[target->output_buffer_head] = target->bin[target->bin_size/2];
		if (target->output_buffer_head == target->output_buffer_tail)
		{
			target->output_buffer_tail = (target->output_buffer_tail + 1) % target->output_buffer_size;
		}
	}
	vm_mutex_unlock(&target->mutex);
}

VMBOOL afifo_read(afifo* source, int* value)
{
	vm_mutex_lock(&source->mutex);
	if (source->output_buffer_head == source->output_buffer_tail)
	{
		vm_mutex_unlock(&source->mutex);
		return FALSE;
	}
	source->output_buffer_tail = (source->output_buffer_tail + 1) % source->output_buffer_size;
	*value = source->output_buffer[source->output_buffer_tail];
	vm_mutex_unlock(&source->mutex);
	return TRUE;
}


static void http_done_callback(VM_HTTPS_RESULT result, VMUINT16 status, VM_HTTPS_METHOD method, char* url, char* headers, char* body)
{
	char buffer[64] = {0};
	if (report_console)
	{
		sprintf(buffer, "result: %d, status: %d, method: %d\n", (signed char)result, status, (signed char)method);
		write_console(buffer);
		write_console(url);
		write_console("\n");
		write_console(headers);
		write_console("\n");
		write_console(body);
		write_console("\n");
	}

	if (result == VM_HTTPS_OK)
	{
		vm_fs_delete(current_report_filename);
		http_failure_count = 0;
		blink_green();
	}
	else
	{
		http_failure_count++;
	}
	blue_led_off();
}

void compile_report(afifo* source)
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
	char report_text[1024];
	VMWCHAR tmpf_w_filename[32];

	strcpy(report_text, "mes=");

	while(afifo_read(source, &result) && result_count < 80)
	{
		result_count++;
		sprintf(tmp_buffer, "%d_", result);
		strcat(report_text, tmp_buffer);
	}

	strcat(report_text, "&time=");
	vm_time_get_unix_time(&rtc_time);
	sprintf(tmp_buffer, "%u", rtc_time);
	strcat(report_text, tmp_buffer);

	vm_chset_ucs2_to_ascii(tmpf_filename, 32, REPORT_TMP_FOLDER);
	strcat(tmpf_filename, tmp_buffer);
	strcat(tmpf_filename, ".rpt");
	
	strcat(report_text, "&sig=");
	vm_ssl_sha1_hmac(hmac_key, hmac_key_length, report_text, strlen(report_text), hmac);
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
	strcat(report_text, tmp_buffer);

	vm_chset_ascii_to_ucs2(tmpf_w_filename, 64, tmpf_filename);
	tmpf_handle = vm_fs_open(tmpf_w_filename, VM_FS_MODE_CREATE_ALWAYS_WRITE, VM_FALSE);
	if (tmpf_handle >= 0)
	{
		vm_fs_write(tmpf_handle, report_text, strlen(report_text), &tmpf_written);
		vm_fs_close(tmpf_handle);
	}
}


void send_report()
{
	vm_fs_info_t found_file;
	VM_FS_HANDLE find_handle;
	VM_FS_HANDLE report_handle;
	VM_RESULT res;
	VMUINT bytes_read;
	char tmp_buffer[32];
	char tmpf_filename[32];
	char http_body[1024];

	if (!report_http)
	{
		return;
	}

	find_handle = vm_fs_find_first(REPORT_TMP_FOLDER u"*.rpt", &found_file);
	if (find_handle < 0)
	{
		return;
	}
	blue_led_on();
	if (report_console)
	{
		write_console("sending report\n");
	}
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
	vm_fs_find_close(find_handle);
}

int http_failures()
{
	return http_failure_count;
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
