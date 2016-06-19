#ifndef MEASURE_H
#define MEASURE_H

//#define AVERAGING            60
#define AVERAGING            5
#define MEASUREMENT_INTERVAL 1000
#define RESULT_BUFFER_SIZE   8
#define MAX_REPORT_INTERVAL  (MEASUREMENT_INTERVAL*AVERAGING*RESULT_BUFFER_SIZE)

#define ADC_SCL              43
#define ADC_SDA              44

void start_measurement();
void stop_measurement();
VMBOOL get_measurement_result(VMINT* result);
#endif /* MEASURE_H */
