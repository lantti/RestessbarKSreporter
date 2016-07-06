#ifndef MEASURE_H
#define MEASURE_H

#define ADC_HANDLE_INVALID -1
#define MAX_HANDLE 3

typedef VMINT ADC_HANDLE;

ADC_HANDLE start_measurement(int scl_pin, int sda_pin, int measurement_interval, void (*result_callback)(void*, VMINT), void* result_callback_env);
void stop_measurement(ADC_HANDLE handle);

#endif /* MEASURE_H */
