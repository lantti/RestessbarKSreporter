#ifndef TELECOM_H
#define TELECOM_H

#include "vmtype.h"
#include "vmhttps.h"

#define GETHEADERS "User-Agent: restessbarKS1\r\nAccept: */*\r\nHost: "
#define POSTHEADERS "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: "

void init_telecom(char* apn);
VMBOOL http_get(char* host, char* path, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*));
VMBOOL http_post(char* host, char* path, char* body, void (*done_callback)(VM_HTTPS_RESULT, VMUINT16, VM_HTTPS_METHOD, char*, char*, char*));


#endif /* TELECOM_H */
