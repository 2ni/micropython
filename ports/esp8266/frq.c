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

void clear_isr() {
    // clear interrupts status
    uint32 s = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, s);
    ETS_GPIO_INTR_ENABLE();
}

void subscribe_isr(void (*isr)()) {
    ETS_GPIO_INTR_DISABLE();

    ETS_GPIO_INTR_ATTACH(isr, 12); // GPIO12 interrupt handler
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12); // Set GPIO12 function
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(12)); // Set GPIO12 as input
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(12)); // Clear GPIO12 status
    gpio_pin_intr_state_set(GPIO_ID_PIN(12), GPIO_PIN_INTR_POSEDGE); // Interrupt on any GPIO12 edge

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

volatile uint32 count;

void ICACHE_FLASH_ATTR isr_measure_count() {
    ETS_GPIO_INTR_DISABLE();
    count++;

    clear_isr();
}

/*
 * function get()
 * measures frequency by counting ticks during given time period
 */
STATIC mp_obj_t frq_get(void) {
    unsigned long s;

    subscribe_isr(isr_measure_count);
    count = 0;

    #define MEASURE_TIME 100000 // in us
    s = system_get_time();
    while (system_get_time()<(s+MEASURE_TIME)) {
        ;
    }
    //printf("count: %d\n", count);
    return mp_obj_new_int(count);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(frq_get_obj, frq_get);



// ********************************* measure by time *********************************

#define TIMEOUT 1000000 // in usec
#define SAMPLES 10
uint32 f_start;
uint32 f_end;
volatile int isr_finished;
volatile int isr_count;

//void ICACHE_FLASH_ATTR isr_time(uint mask, void* arg) {
void ICACHE_FLASH_ATTR isr_measure_time() {
    ETS_GPIO_INTR_DISABLE();
    if (isr_count==0) {
        f_start = system_get_rtc_time();
    } else if (isr_count==SAMPLES) {
        f_end = system_get_rtc_time();
        isr_finished=1;
    }
    isr_count++;

    clear_isr();

    if (isr_finished != 1) {
        ETS_GPIO_INTR_ENABLE();
    }
}

/*
 * function get_byduration()
 * measures frequency by measuring time some ticks take
 */
STATIC mp_obj_t frq_get_byduration(void) {

    subscribe_isr(isr_measure_time);

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

    printf("ticks start: %d, ticks end: %d, ticks diff: %d\n", f_start, f_end, f_end-f_start);
    printf("frequency: %d.%01dkHz\n", (int)f, (int)(f*10)%10);
    printf("isr_count: %d\n", isr_count);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(frq_get_byduration_obj, frq_get_byduration);

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
