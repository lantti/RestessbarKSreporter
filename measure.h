#ifndef MEASURE_H
#define MEASURE_H

#define ADC_HANDLE_INVALID -1
#define MAX_HANDLE 3

typedef VMINT ADC_HANDLE;

VMINT open_adc(int scl_pin, int sda_pin, int result_buffer_size);
void close_adc(ADC_HANDLE handle);

VMBOOL start_measurement(ADC_HANDLE handle, int averaging, int measurement_interval);
void stop_measurement(ADC_HANDLE handle);

VMBOOL get_measurement_result(ADC_HANDLE handle, VMINT* result);
#endif /* MEASURE_H */
