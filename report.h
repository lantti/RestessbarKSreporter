#ifndef REPORT_H
#define REPORT_H

#include "vmtype.h"

#define REPORT_TMP_FOLDER     u"C:\\tmp\\" 
#define REPORT_HOSTNAME_MAX   128
#define REPORT_PATH_MAX       128
#define MAX_HMAC_KEY_LENGTH   32

typedef struct afifo afifo;

afifo* afifo_create(int aggregation, int size);
void afifo_destroy(afifo* target);
void afifo_write(afifo* target, int value);
VMBOOL afifo_read(afifo* source, int* value);
void compile_report(afifo* source);
void send_report();
int http_failures();
void enable_http_report();
void disable_http_report();
void enable_console_report();
void disable_console_report();
void set_report_http_host(char* host);
void set_report_http_path(char* path);
void set_report_http_hmac_key(VMBYTE* key, VMINT key_length);

#endif /* REPORT_H */
