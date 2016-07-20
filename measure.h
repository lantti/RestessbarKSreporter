#ifndef MEASURE_H
#define MEASURE_H

#define ADC_HANDLE_INVALID -1
#define MAX_HANDLE 3
#define TASK_QUEUE_LENGTH 5

typedef VMINT ADC_HANDLE;

ADC_HANDLE open_hx711(int scl_pin, int sda_pin);
void set_hx711_a128(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env);
void set_hx711_a64(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env);
void set_hx711_b32(ADC_HANDLE handle, int delay, void (*callback)(void*, int), void* callback_env);
void set_hx711_wait(ADC_HANDLE handle, int delay);
void close_hx711(ADC_HANDLE handle, void (*callback)(void*, int), void* callback_env);

#endif /* MEASURE_H */
