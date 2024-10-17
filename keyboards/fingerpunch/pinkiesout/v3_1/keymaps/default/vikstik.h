// Copyright 2024 burkfers (@burkfers)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define CALIBRATION_SAMPLE_COUNT 100

#define MIN_VAL(a, b) ((a) < (b) ? (a) : (b))
#define MAX_VAL(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
    uint16_t actuation_point;
    uint16_t deadzone_inner;
    uint16_t deadzone_outer;
    int8_t   out_min;
    int8_t   out_max;
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t stick_timer_ms;
} joystick_profile_t;

#define JS_ADAFRUIT_2765 ((const joystick_profile_t) { \
    .actuation_point = 40, \
    .deadzone_inner = 60, \
    .deadzone_outer = 60, \
    .out_min = -127, \
    .out_max = 127, \
    .raw_min = 0, \
    .raw_max = 1023, \
    .stick_timer_ms = 5 \
})

enum vikstik_keycodes {
    VJS_QUAD = QK_KB,  // stick rotation quadrants
    VJS_SMOD           // stick mode
};

enum vikstik_stick_modes {
    VIKSTIK_SM_ANALOG, // stick mode: analog
    VIKSTIK_SM_WASD,   // stick mode: wasd
    VIKSTIK_SM_ARROWS, // stick mode: arrow keys
    VIKSTIK_SM_END     // end marker
};

typedef enum {
    UP    = 0,             // joystick's up is facing up
    RIGHT = 1,             // joystick's right is facing up
    DOWN  = 2,             // joystick's down is facing up
    LEFT  = 3,             // joystick's left is facing up
    ORIENTATION_COUNT = 4  // number of orientations
} vikstik_up_orientation;

typedef struct {
    uint8_t mode;
    int8_t up_orientation;
    int16_t up_angle;
} vikstik_config_t;

int8_t  get_stick_up_orientation(void);
void    set_stick_up_orientation(vikstik_up_orientation up_orientation);
int16_t get_stick_up_angle(void);
void    set_stick_up_angle(int16_t angle);
void    step_stick_up_orientation(int8_t step);
void    step_stick_mode(void);
void    get_raw_quadrant(int8_t* out_quadrant, int16_t* out_angle);
void    process_vikstik(void);
