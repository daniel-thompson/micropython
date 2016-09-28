/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014, 2015 Damien P. George
 * Copyright (c) 2016 Linaro Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <zephyr.h>
#include <gpio.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "modpyb.h"

const mp_obj_base_t pyb_pin_obj_template = {&pyb_pin_type};

STATIC void pyb_pin_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_pin_obj_t *self = self_in;

    // pin name
    mp_printf(print, "Pin(%p@%d)", self->port, self->pin);
}

// pin.init(mode, pull=None, *, value)
STATIC mp_obj_t pyb_pin_obj_init_helper(pyb_pin_obj_t *self, mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_mode, ARG_pull, ARG_value };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mode, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pull, MP_ARG_OBJ, {.u_obj = mp_const_none}},
        { MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get io mode
    uint mode = args[ARG_mode].u_int;

    // get pull mode
    uint pull = GPIO_PUD_NORMAL;
    if (args[ARG_pull].u_obj != mp_const_none) {
        pull = mp_obj_get_int(args[ARG_pull].u_obj);
    }

    int ret = gpio_pin_configure(self->port, self->pin, mode | pull);
    if (ret) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
    }
    
    // get initial value
    int value;
    if (args[ARG_value].u_obj != MP_OBJ_NULL) {
	(void) gpio_pin_write(self->port, self->pin,
			mp_obj_is_true(args[ARG_value].u_obj));
    }


    return mp_const_none;
}

// constructor(drv_name, pin, ...)
STATIC mp_obj_t pyb_pin_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 2, MP_OBJ_FUN_ARGS_MAX, true);

    // get the wanted port
    const char *drv_name = mp_obj_str_get_str(args[0]);
    int wanted_pin = mp_obj_get_int(args[1]);
    struct device *wanted_port = device_get_binding(drv_name);
    if (!wanted_port) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
    }

    pyb_pin_obj_t *pin = m_new_obj(pyb_pin_obj_t);
    pin->base = pyb_pin_obj_template;
    pin->port = wanted_port;
    pin->pin = wanted_pin;

    if (n_args > 2 || n_kw > 0) {
        // pin mode given, so configure this GPIO
        mp_map_t kw_args;
        mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
        pyb_pin_obj_init_helper(pin, n_args - 2, args + 2, &kw_args);
    }

    return (mp_obj_t)pin;
}

// fast method for getting/setting pin value
STATIC mp_obj_t pyb_pin_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    pyb_pin_obj_t *self = self_in;
    if (n_args == 0) {
	uint32_t pin_val;
	(void) gpio_pin_read(self->port, self->pin, &pin_val);
        return MP_OBJ_NEW_SMALL_INT(pin_val);
    } else {
	(void) gpio_pin_write(self->port, self->pin, mp_obj_is_true(args[0]));
        return mp_const_none;
    }
}

// pin.init(mode, pull)
STATIC mp_obj_t pyb_pin_obj_init(mp_uint_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    return pyb_pin_obj_init_helper(args[0], n_args - 1, args + 1, kw_args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(pyb_pin_init_obj, 1, pyb_pin_obj_init);

// pin.value([value])
STATIC mp_obj_t pyb_pin_value(mp_uint_t n_args, const mp_obj_t *args) {
    return pyb_pin_call(args[0], n_args - 1, 0, args + 1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pyb_pin_value_obj, 1, 2, pyb_pin_value);

// pin.low()
STATIC mp_obj_t pyb_pin_low(mp_obj_t self_in) {
    pyb_pin_obj_t *self = self_in;
    (void) gpio_pin_write(self->port, self->pin, 0);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_pin_low_obj, pyb_pin_low);

// pin.high()
STATIC mp_obj_t pyb_pin_high(mp_obj_t self_in) {
    pyb_pin_obj_t *self = self_in;
    (void) gpio_pin_write(self->port, self->pin, 1);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_pin_high_obj, pyb_pin_high);

STATIC const mp_map_elem_t pyb_pin_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),    (mp_obj_t)&pyb_pin_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),   (mp_obj_t)&pyb_pin_value_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_low),     (mp_obj_t)&pyb_pin_low_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_high),    (mp_obj_t)&pyb_pin_high_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_IN),        MP_OBJ_NEW_SMALL_INT(GPIO_DIR_IN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_OUT),       MP_OBJ_NEW_SMALL_INT(GPIO_DIR_OUT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PULL_UP),   MP_OBJ_NEW_SMALL_INT(GPIO_PUD_PULL_UP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PULL_DOWN), MP_OBJ_NEW_SMALL_INT(GPIO_PUD_PULL_DOWN) },
};

STATIC MP_DEFINE_CONST_DICT(pyb_pin_locals_dict, pyb_pin_locals_dict_table);

const mp_obj_type_t pyb_pin_type = {
    { &mp_type_type },
    .name = MP_QSTR_Pin,
    .print = pyb_pin_print,
    .make_new = pyb_pin_make_new,
    .call = pyb_pin_call,
    .locals_dict = (mp_obj_t)&pyb_pin_locals_dict,
};
