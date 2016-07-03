#ifndef CONF_H
#define CONF_H


#include "vmtype.h"

void open_conf(VMCWSTR conf_file_name);
void close_conf();
VMBOOL read_conf_string(VMSTR variable_name, VMSTR output_buffer, VMUINT output_buffer_size);
VMBOOL read_conf_int(VMSTR variable_name, VMINT* output);

#endif /* CONF_H */
