/*
 * missing includes from espressif
 * see https://github.com/Spritetm/libesphttpd/blob/master/include/espmissingincludes.h
 */
void ets_delay_us(uint16_t ms);
void ets_timer_disarm(os_timer_t *a);
void ets_timer_setfn(os_timer_t *t, ETSTimerFunc *fn, void *parg);
void ets_timer_arm_new(os_timer_t *a, uint32_t b, bool repeat, bool isMstimer);

void ets_isr_unmask(unsigned intr);
void ets_isr_mask(unsigned intr);
void ets_isr_attach(int intr, void (*handler)(void *), void *arg);


/*
 * https://github.com/espressif/esp8266-rtos-sample-code/blob/master/01System/UART0/include/driver/gpio.h
 */
void gpio_output_conf(uint32 set_mask, uint32 clear_mask, uint32 enable_mask, uint32 disable_mask);
