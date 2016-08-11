#ifndef LEDS_H
#define LEDS_H

#define REDLED               17
#define GREENLED             15
#define BLUELED              12

void init_leds();
void free_leds();
void red_led_on();
void red_led_off();
void green_led_on();
void green_led_off();
void blue_led_on();
void blue_led_off();
#endif /* LEDS_H */
