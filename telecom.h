#ifndef TELECOM_H
#define TELECOM_H

#include "vmhttps.h"

void init_telecom(char* apn);
void http_get(char* host, char* path, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*));
void http_post(char* host, char* path, char* body, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*));


#endif /* TELECOM_H */
