#ifndef CONF_H
#define CONF_H

#define CONF_FILENAME        u"C:\\config.txt"

#include "vmtype.h"

VMBOOL read_conf_string(VMSTR variable_name, VMSTR output_buffer, VMUINT output_buffer_size);
VMBOOL read_conf_int(VMSTR variable_name, VMINT* output);

#endif /* CONF_H */
