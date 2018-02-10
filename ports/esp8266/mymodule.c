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
#define GPIO2_H         (GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1<<2))
#define GPIO2_L         (GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1<<2))
#define GPIO2(x)        ((x)?GPIO2_H:GPIO2_L)

#include "espmissingincludes.h"

/*
 * >>> import mymodule
 * >>> mymodule.hello("foo")
 * Hello foo!
 * >>> a = mymodule.hellObj(12)
 * >>> a
 * hellObj(12)
 * >>> a.inc()
 * >>> a
 * hellObj(13)
 * >>>
 */

const mp_obj_type_t mymodule_hello_type;

/*
 * class definition
 * hellObj
 */
typedef struct _mymodule_hello_obj_t {
    mp_obj_base_t base;
    uint8_t hello_number;
} mymodule_hello_obj_t;

mp_obj_t mymodule_hello_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, true);
    mymodule_hello_obj_t *self = m_new_obj(mymodule_hello_obj_t);
    self->base.type = &mymodule_hello_type;
    self->hello_number = mp_obj_get_int(args[0]);
    return MP_OBJ_FROM_PTR(self);
}

STATIC void mymodule_hello_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mymodule_hello_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Hello(%u)", self->hello_number);
}

STATIC mp_obj_t mymodule_hello_increment(mp_obj_t self_in) {
    mymodule_hello_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->hello_number += 1;
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mymodule_hello_increment_obj, mymodule_hello_increment);

STATIC const mp_rom_map_elem_t mymodule_hello_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_inc), MP_ROM_PTR(&mymodule_hello_increment_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mymodule_hello_locals_dict, mymodule_hello_locals_dict_table);

const mp_obj_type_t mymodule_hello_type = {
    { &mp_type_type },
    .name = MP_QSTR_hellObj,
    .print = mymodule_hello_print,
    .make_new = mymodule_hello_make_new,
    .locals_dict = (mp_obj_dict_t*)&mymodule_hello_locals_dict,
};

/*
 * function definition
 * mymodule.blink()
 * using timers inspired by https://github.com/esp8266/source-code-examples/blob/master/blinky/user/user_main.c
 */
int blink_count;
static os_timer_t tmr;

/*
 * function called when timer triggers
 */
void tmrfc(void *arg) {
    //printf("count: %d\n", *((int *)arg));

    if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
        gpio_output_set(0, BIT2, BIT2, 0); // set GPIO2 low
    } else {
        gpio_output_set(BIT2, 0, BIT2, 0); // set GPIO2 high
        *((int *)arg) -= 1;
        if (*((int *)arg)<0) os_timer_disarm(&tmr);
    }
}

/*
 * blink() function
 * blinks 3 times
 */
STATIC mp_obj_t mymodule_blink(void) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); // set GPIO2 to output mode

    // number of times the led shall enlight us
    blink_count = 3;

    os_timer_disarm(&tmr); // disarm timer
    os_timer_setfn(&tmr, (os_timer_func_t *)tmrfc, &blink_count); // setup timer
    os_timer_arm(&tmr, 100, 1); // call every x ms, 0:once|1:repeat

    /*
    for(int i=0; i<3; i++) {
        gpio_output_set(0, BIT2, BIT2, 0); // set GPIO2 low
        os_delay_us(20000);
        gpio_output_set(BIT2, 0, BIT2, 0); // set GPIO2 high
        os_delay_us(20000);
    }
    */

    return mp_const_none;
};
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mymodule_blink_obj, mymodule_blink);

/*
 * function definition
 * mymodule.hello()
 */
STATIC mp_obj_t mymodule_hello(mp_obj_t what) {
    printf("Hello %s!\n", mp_obj_str_get_str(what));

    return mp_const_none;
};
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mymodule_hello_obj, mymodule_hello);

/*
 * module declarations
 */
STATIC const mp_map_elem_t mymodule_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_mymodule) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hello), (mp_obj_t)&mymodule_hello_obj },  // function name hello
    { MP_OBJ_NEW_QSTR(MP_QSTR_blink), (mp_obj_t)&mymodule_blink_obj },  // function name blink
    { MP_OBJ_NEW_QSTR(MP_QSTR_hellObj), (mp_obj_t)&mymodule_hello_type }, // class name hellObj
};

STATIC MP_DEFINE_CONST_DICT(mp_module_mymodule_globals, mymodule_globals_table);

const mp_obj_module_t mp_module_mymodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_mymodule_globals,
};
