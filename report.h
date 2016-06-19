#ifndef REPORT_H
#define REPORT_H

//#define REPORT_INTERVAL      480000
#define REPORT_INTERVAL      30000
#define REPORTHOST "requestb.in"
#define REPORTPATH "/1fw3pol1"


void start_reporting();
void stop_reporting();
void enable_http_report();
void disable_http_report();
void enable_console_report();
void disable_console_report();
#endif /* REPORT_H */
