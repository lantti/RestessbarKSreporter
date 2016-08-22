#include <stdio.h>
#include <string.h>
#include "vmtype.h"
#include "vmhttps.h"
#include "vmgsm_tel.h"
#include "vmgsm_sms.h"
#include "vmgsm_gprs.h"
#include "vmchset.h"
#include "log.h"
#include "console.h"
#include "telecom.h"

static VM_HTTPS_METHOD method;
static VMUINT16 http_status;
static char url[256];
static char headers[256];
static char body[1024];

static void (*done_cb)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*);

static VMUINT8 retry_count = 0;
static VMUINT8 channel;

static void https_set_channel_cb(VMUINT32 req_id, VMUINT8 channel_id, VMUINT8 result)
{
	channel = channel_id;
	vm_https_send_request(
			req_id,
			method,
			VM_HTTPS_OPTION_NO_CACHE,
			VM_HTTPS_DATA_TYPE_BUFFER,
			1024,
			url,
			strlen(url),
			headers,
			strlen(headers),
			body,
			strlen(body));

}
static void https_unset_channel_cb(VMUINT8 channel_id, VMUINT8 result)
{
}
static void https_release_cb(VMUINT8 result)
{
}
static void https_termination_cb(void)
{
}
static void https_response_cb(VMUINT16 request_id, VM_HTTPS_RESULT result, 
		VMUINT16 status, VMINT32 cause, VMUINT8 protocol, 
		VMUINT32 content_length,VMBOOL more,
		VMUINT8 *content_type, VMUINT8 content_type_len,  
		VMUINT8 *new_url, VMUINT32 new_url_len,
		VMUINT8 *reply_header, VMUINT32 reply_header_len,  
		VMUINT8 *reply_segment, VMUINT32 reply_segment_len)
{
	if (result == VM_HTTPS_OK)
	{
		memset(url,0,256);
		memset(headers, 0, 256);
		memset(body,0,1024);
		http_status =  status;
		if(new_url_len > 255)
		{
			strncpy(url, new_url, 255);
		}
		else
		{
			strncpy(url, new_url, new_url_len);
		}
		if(reply_header_len > 255)
		{
			strncpy(headers, reply_header, 255);
		}
		else
		{
			strncpy(headers, reply_header, reply_header_len);
		}
		if(reply_segment_len > 1024)
		{
			strncpy(body, reply_segment, 1023);
		}
		else
		{
			strncpy(body, reply_segment, reply_segment_len);
		}
	}
	if (more)
	{
		vm_https_read_content(request_id, 0, 1024);
	}
	else
	{
		vm_https_cancel(request_id);
		vm_https_unset_channel(channel);
		done_cb(result, http_status, method, url, headers, body);
	}
}
static void https_read_content_cb(VMUINT16 request_id, VMUINT8 seq_num, 
		VMUINT8 result, VMBOOL more, 
		VMUINT8 *reply_segment, VMUINT32 reply_segment_len)
{
	if (more)
	{
		vm_https_read_content(request_id, seq_num + 1, 1024);
	}
	else
	{
		vm_https_cancel(request_id);
		vm_https_unset_channel(channel);
		done_cb(result, http_status, method, url, headers, body);
	}
}
static void https_cancel_cb(VMUINT16 request_id, VMUINT8 result)
{
}
static void https_status_query_cb(VMUINT8 status)
{
}

static void gsm_telephone_cb(vm_gsm_tel_call_listener_data_t* call_data)
{
	char text_buffer[VM_GSM_TEL_MAX_NUMBER_LENGTH + 64];
	vm_gsm_tel_call_info_t* call_info;

	if (call_data->call_type == VM_GSM_TEL_INDICATION_INCOMING_CALL)
	{
		call_info = (vm_gsm_tel_call_info_t*)call_data->data;
		sprintf(text_buffer, "Incoming telephone call from number %s", call_info->num_uri);
		write_log(text_buffer);
	}
	if (call_data->call_type == VM_GSM_TEL_INDICATION_CALL_ENDED)
	{
		write_log("call ended");
	}
}

static void gsm_delete_sms_cb(vm_gsm_sms_callback_t* callback_data)
{
	if (callback_data->action == VM_GSM_SMS_ACTION_DELETE && callback_data->cause == VM_GSM_SMS_CAUSE_NO_ERROR)
	{
		write_log("SMS deleted");
	}
}

static int gsm_new_sms_cb(vm_gsm_sms_event_t* event_data) {
	vm_gsm_sms_event_new_sms_t* event_info;    
	vm_gsm_sms_new_message_t* sms;
	char text_buffer[320] = {0};
	if (event_data->event_id == VM_GSM_SMS_EVENT_ID_SMS_NEW_MESSAGE)
	{
		event_info = (vm_gsm_sms_event_new_sms_t *)event_data->event_info;
		sms  =  event_info->message_data;
		sprintf(text_buffer, "SMS from number %s", sms->number);
		write_log(text_buffer); 
		if (sms->dcs == VM_GSM_SMS_DCS_UCS2)
		{
			vm_chset_ucs2_to_ascii((VMSTR)text_buffer, 319, (VMWSTR)sms->data);
		}
		else
		{
			strncpy(text_buffer, sms->data, 319);
		}
		write_log(text_buffer);
		vm_gsm_sms_delete_message(sms->message_id, (vm_gsm_sms_callback)gsm_delete_sms_cb, NULL);
		return 0;
	}
	else 
	{
		sprintf(text_buffer, "SMS event: %d", event_data->event_id);
		write_log(text_buffer);
		return 0;
	}
}


static void compose_url(char* host, char* path)
{
	memset(url,0,256);
	strcpy(url, "http://");
	if(strlen(host) + strlen(path) > 248)
	{
		strncat(url, host, 248);
		if (strlen(host) < 248)
		{
			strncat(url, path, 248-strlen(host));
		}
	}
	else
	{
		strcat(url, host);
		strcat(url, path);
	}

}

static void compose_get_headers(char* host)
{
	memset(headers, 0, 256);
	strcpy(headers, GETHEADERS);
	if (strlen(host) < 253-strlen(GETHEADERS))
	{
		strcat(headers, host);
		strcat(headers, "\r\n");
	}
}

static void compose_post_headers(char* host)
{
	char buffer[64];
	sprintf(buffer, "%d\r\n", strlen(body));
	compose_get_headers(host);
	strcpy(headers, POSTHEADERS);
	strcat(headers, buffer);
}

static void compose_post_body(char* request_body)
{
	memset(body,0,1024);
	strncpy(body, request_body, 1021);
}


void init_telecom(char* apn)
{
	vm_https_callbacks_t callbacks = {
		(vm_https_set_channel_response_callback)https_set_channel_cb,
		(vm_https_unset_channel_response_callback)https_unset_channel_cb,
		(vm_https_release_all_request_response_callback)https_release_cb,
		(vm_https_termination_callback)https_termination_cb,
		(vm_https_send_response_callback)https_response_cb,
		(vm_https_read_content_response_callback)https_read_content_cb,
		(vm_https_cancel_response_callback)https_cancel_cb,
		(vm_https_status_query_response_callback)https_status_query_cb
	};

	vm_gsm_gprs_apn_info_t apn_info = {0};

	//vm_gsm_tel_call_reg_listener((vm_gsm_tel_call_listener_callback)gsm_telephone_cb);
	//vm_gsm_sms_set_interrupt_event_handler(VM_GSM_SMS_EVENT_ID_SMS_NEW_MESSAGE, gsm_new_sms_cb, NULL);

	strncpy(apn_info.apn, apn, VM_GSM_GPRS_APN_MAX_LENGTH);
	vm_gsm_gprs_set_customized_apn_info(&apn_info);

	vm_https_register_context_and_callback(VM_BEARER_DATA_ACCOUNT_TYPE_GPRS_CUSTOMIZED_APN, &callbacks);
}

void http_get(char* host, char* path, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*)) {
	memset(body,0,1024);
	method = VM_HTTPS_METHOD_GET;
	compose_url(host, path);
	compose_get_headers(host);
	done_cb = done_callback;
	http_status = 0;
	vm_https_set_channel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void http_post(char* host, char* path, char* request_body, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*)){
	memset(body,0,1024);
	method = VM_HTTPS_METHOD_POST;
	compose_url(host, path);
	compose_post_body(request_body);
	compose_post_headers(host);
	done_cb = done_callback;
	http_status = 0;
	vm_https_set_channel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

