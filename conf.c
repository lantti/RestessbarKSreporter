#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vmfs.h"
#include "vmmemory.h"
#include "conf.h"

static VM_FS_HANDLE conf_handle;
static VMBOOL conf_handle_valid = FALSE;

void open_conf(VMCWSTR conf_file_name)
{
    VMINT res;
    if (conf_handle_valid)
    {
        close_conf();
    }
    res = vm_fs_open(conf_file_name, VM_FS_MODE_READ, VM_FALSE);
    if (VM_IS_SUCCEEDED(res))
    {
        conf_handle = res;
        VMBOOL conf_handle_valid = TRUE;
    }
}

void close_conf();
{
        vm_fs_close(conf_handle);
        VMBOOL conf_handle_valid = FALSE;
}

VMBOOL read_conf_string(VMSTR variable_name, VMSTR output_buffer, VMUINT output_buffer_size) {
    VMINT res;
    VMUINT conf_size;
    VMUINT bytes_read;
    VMSTR conf_data;
    VMSTR key;
    VMSTR value;
    VMBOOL result_string_valid = FALSE;

    if (conf_handle_valid)
    {
        res = vm_fs_get_size(conf_handle, &conf_size);
        if (VM_IS_SUCCEEDED(res))
        {
            conf_data = vm_calloc(conf_size + 1);
            if (conf_data != NULL)
            {
                res = vm_fs_read(conf_handle, conf_data, conf_size, &bytes_read);
                if (VM_IS_SUCCEEDED(res))
                {
                    key = strtok(conf_data, "=");
                    while (key != NULL) 
                    {
                        value = strtok(NULL, "\n");
                        if (value != NULL && strcmp(variable_name, key) == 0 && strlen(value) < output_buffer_size)
                        {
                            strcpy(output_buffer, value);
                            result_string_valid = TRUE;
                            break;
                        }
                        key = strtok(NULL, "=");
                    } 
                }
                vm_free(conf_data);
            }
        }
    }
    return result_string_valid;
}

VMBOOL read_conf_int(VMSTR variable_name, VMINT* output)
{
    char value_string[12];
    char* end_ptr;
    VMBOOL value_valid = FALSE;
    if (read_conf_string(variable_name, value_string, 12))
    {
        *output = (VMINT)strtol(value_string, &end_ptr, 0);
        if (end_ptr != value_string && *end_ptr == 0)
        {
            value_valid = TRUE;
        } 
    }
    return value_valid;
}

