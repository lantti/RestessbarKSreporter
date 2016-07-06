#include <stdio.h>
#include <string.h>
#include "vmfs.h"
#include "vmtype.h"
#include "log.h"

#define LOGFILE u"C:\\log.txt"

static VM_FS_HANDLE log_handle = -1;

void write_log(char* message)
{
    char time_text[11];
    VMUINT rtc_time;
    VMUINT written;
    if (log_handle != -1)
    {
        vm_time_get_unix_time(&rtc_time);
        sprintf(time_text, "%u ", rtc_time);
        vm_fs_write(log_handle, time_text, strlen(time_text), &written);
        vm_fs_write(log_handle, message, strlen(message), &written);
        vm_fs_write(log_handle, "\n", 1, &written);
    }
}

void start_log(VMCWSTR filename)
{
    VMINT res;

    if (log_handle != -1)
    {
        vm_fs_close(log_handle);
        log_handle = -1;
    }

    res = vm_fs_open(LOGFILE, VM_FS_MODE_APPEND, VM_FALSE);
    if (res >= 0)
    {
        log_handle = res;
    }
    else
    {
        res = vm_fs_open(LOGFILE, VM_FS_MODE_CREATE_ALWAYS_WRITE, VM_FALSE);
        if (res >= 0)
        {
            log_handle = res;
        }
    }
}

void stop_log()
{
    if (log_handle != -1)
    {
        vm_fs_close(log_handle);
        log_handle = -1;
    }
}
