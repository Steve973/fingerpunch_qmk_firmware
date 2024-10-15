// Copyright 2024 burkfers (@burkfers)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define STICK_TIMER_MS 5
#define CALIBRATION_SAMPLE_COUNT 100
#define INNER_DEADZONE 60
#define OUTER_DEADZONE 60
#define ACTUATION_POINT 40
#define JOYSTICK_MIN_RAW 0
#define JOYSTICK_MAX_RAW 1023

#define JOYSTICK_MIN_OUT -127
#define JOYSTICK_MAX_OUT 127

#define MIN_VAL(a, b) ((a) < (b) ? (a) : (b))
#define MAX_VAL(a, b) ((a) > (b) ? (a) : (b))

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
