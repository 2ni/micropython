/*
 * ESP8266 reference: https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf
 * Base docs on howto add a module:
 * - http://micropython-dev-docs.readthedocs.io/en/latest/adding-module.html
 * - https://www.stefannaumann.de/en/2017/07/adding-a-module-to-micropython/
 *
 * c implementation of frequency counter with GPIO interrupt
 * as same implementation in python is way too slow
 *
 * frequency on GPIO12 up to ~280kHz
 *
 * Usage example:
 * >>> import frq as f
 * >>> f.get()
 * frequency: 3.2kHz
 */

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include <stdio.h>
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"

#include "espmissingincludes.h"

// ********************************* common functions *********************************

// http://www.openmyr.com/blog/2016/06/esp8266-output-gpio-and-optimization/
// https://github.com/trebisky/esp8266/blob/06436dc11c74cac69c0c47ba37e8fe54ea807474/Projects/misc/blink_any.c
//  https://myesp8266.blogspot.ch/2015/03/esp8266-with-arduino-flavor.html
#define Z 0
static const int mux[] = {
	PERIPHS_IO_MUX_GPIO0_U, /* 0 - D3 */
	PERIPHS_IO_MUX_U0TXD_U, /* 1 - uart */
	PERIPHS_IO_MUX_GPIO2_U, /* 2 - D4 */
	PERIPHS_IO_MUX_U0RXD_U, /* 3 - uart */
	PERIPHS_IO_MUX_GPIO4_U, /* 4 - D2 */
	PERIPHS_IO_MUX_GPIO5_U, /* 5 - D1 */
	Z, /* 6  mystery */
	Z, /* 7  mystery */
	Z, /* 8  mystery */
	PERIPHS_IO_MUX_SD_DATA2_U, /* 9 - D11 (SD2) */
	PERIPHS_IO_MUX_SD_DATA3_U, /* 10 - D12 (SD3) */
	Z, /* 11  mystery */
	PERIPHS_IO_MUX_MTDI_U, /* 12 - D6 */
	PERIPHS_IO_MUX_MTCK_U, /* 13 - D7 */
	PERIPHS_IO_MUX_MTMS_U, /* 14 - D5 */
	PERIPHS_IO_MUX_MTDO_U /* 15 - D8 */
};
static const int func[] = { 0, 3, 0, 3, 0, 0, Z, Z, Z, 3, 3, Z, 3, 3, 3, 3 };
// for some reasons some pins are not advised to be used
static const int evil[] = { 0, 1, 0, 1,   0, 0, 1, 1,   1, 1, 1, 1,   0, 0, 0, 0 };

void subscribe_isr(void (*isr)(), int gpio, int *ok) {
    ETS_GPIO_INTR_DISABLE();
    if(ok) {*ok=0;}

    if (!evil[gpio]) {
        PIN_FUNC_SELECT(mux[gpio], func[gpio]);

        gpio_output_set(0, 0, 0, GPIO_ID_PIN(gpio));
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpio));

        ETS_GPIO_INTR_ATTACH(isr, gpio);
        gpio_pin_intr_state_set(GPIO_ID_PIN(gpio), GPIO_PIN_INTR_POSEDGE);

        if(ok) {*ok=1;}
    }

    ETS_GPIO_INTR_ENABLE() ;
}

/*
 * just a dummy function to check how time measurement with ticks works
 * not exposed
 */
void _check_ticks() {
    uint32 factor = (system_rtc_clock_cali_proc()*1000)>>12;
    uint32 time = system_get_rtc_time(); // usec
    os_delay_us(1000);
    uint32 end = system_get_rtc_time(); // usec
    printf("example time: %d, end: %d, diff: %d\n", time, end, (end-time)*factor/1000);
}

// ********************************* measure by ticks *********************************
#define MEASURE_TIME 10000 // in us
#define MEASURE_AMOUNT 3 // how many times shall we measure to calculate average
volatile uint32 count;

void ICACHE_FLASH_ATTR isr_measure_count(int gpio) {
    ETS_GPIO_INTR_DISABLE();

    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    if(gpio_status & BIT(gpio)) {
        count++;
    }

    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

    ETS_GPIO_INTR_ENABLE();
}

/*
 * function get(GPIO)
 * measures frequency in kHz by counting ticks during given time period
 */
STATIC mp_obj_t frq_get(mp_obj_t gpio) {
    unsigned long s;
    int ok;

    subscribe_isr(isr_measure_count, mp_obj_get_int(gpio), &ok);
    if(!ok) {return mp_obj_new_float((float)-1);}

    int total_count = 0;

    // get average measurements
    for (int c=0; c<MEASURE_AMOUNT; c++) {
        count = 0;
        s = system_get_time();
        while (system_get_time()<(s+MEASURE_TIME)) {
            ; // wait for interrupts
        }
        total_count += count;
        //printf("count (%d): %d\n", c, count);
    }
    return mp_obj_new_float((float)total_count/MEASURE_AMOUNT*1000/MEASURE_TIME);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(frq_get_obj, frq_get);



// ********************************* measure by time *********************************

#define TIMEOUT 1000000 // in usec
#define SAMPLES 10
uint32 f_start;
uint32 f_end;
volatile int isr_finished;
volatile int isr_count;

//void ICACHE_FLASH_ATTR isr_time(uint mask, void* arg) {
void ICACHE_FLASH_ATTR isr_measure_time(int gpio) {
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    if(gpio_status & BIT(gpio)) {
        ETS_GPIO_INTR_DISABLE();
        if (isr_count==0) {
            f_start = system_get_rtc_time();
        } else if (isr_count==SAMPLES) {
            f_end = system_get_rtc_time();
            isr_finished=1;
        }
        isr_count++;

        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

        if (isr_finished != 1) {
            ETS_GPIO_INTR_ENABLE();
        }
    }
}

/*
 * function get_byduration()
 * measures frequency by measuring time some ticks take
 */
STATIC mp_obj_t frq_get_byduration(mp_obj_t gpio) {

    int ok;
    subscribe_isr(isr_measure_time, mp_obj_get_int(gpio), &ok);
    if(!ok) {return mp_obj_new_float((float)-1);}

    isr_finished = 0;
    isr_count = 0;
    f_end = 0;
    f_start = 0;

    unsigned long s = system_get_time();
    while(isr_finished == 0) {
        if (system_get_time()>(s+TIMEOUT)) {
            printf("moisture sensor timeout\n");
            f_end = f_start;
            break;
        }
    }

    uint32 factor = (system_rtc_clock_cali_proc()*1000)>>12;
    float f = 0;
    if (f_end-f_start > 0) {
        f = SAMPLES/((f_end-f_start)*factor/1e3/1e6)/1e3; // kHz
    }

    //printf("ticks start: %d, ticks end: %d, ticks diff: %d\n", f_start, f_end, f_end-f_start);
    //printf("frequency: %d.%01dkHz\n", (int)f, (int)(f*10)%10);
    //printf("isr_count: %d\n", isr_count);

    return mp_obj_new_float(f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(frq_get_byduration_obj, frq_get_byduration);

/*
 * module declarations
 */
STATIC const mp_map_elem_t frq_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_frq) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get), (mp_obj_t)&frq_get_obj },  // function name get()
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_byduration), (mp_obj_t)&frq_get_byduration_obj },  // function name get()
};

STATIC MP_DEFINE_CONST_DICT(mp_module_frq_globals, frq_globals_table);

const mp_obj_module_t mp_module_frq = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_frq_globals,
};
