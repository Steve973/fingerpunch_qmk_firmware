/* Copyright 2021 Sadek Baroudi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define CALIBRATION_SAMPLE_COUNT 100
#define FIXED_POINT_SCALE 1024
#define ANGLE_DIVISIONS 16

#ifdef VIKSTIK_LITE_ENABLE
    static const bool is_joystick_lite = true;
#elif VIKSTIK_ENABLE
    static const bool is_joystick_lite = false;
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

typedef struct {
    int8_t  ordinal;
    int16_t angle;
} axis_values;

/**
 * Represents the directions of the joystick, which are used to
 * determine the angle of the joystick. This is a lookup table
 * that is better for resource-constrained processors at the
 * expense of resolution.
 */
typedef enum {
    DIR_R = 0,
    DIR_RRU = 23,
    DIR_RU = 45,
    DIR_UUR = 68,
    DIR_U = 90,
    DIR_UUL = 113,
    DIR_UL = 135,
    DIR_ULL = 158,
    DIR_L = 180,
    DIR_LLD = 203,
    DIR_LD = 225,
    DIR_DDL = 248,
    DIR_D = 270,
    DIR_DDR = 293,
    DIR_DR = 315,
    DIR_DRR = 338
} stick_direction_t;

/**
 * Represents the axis directions in a cartesian coordinate system,
 * and their corresponding angle values.
 */
typedef enum {
    AXIS_RIGHT = 0,    // Positive x-axis
    AXIS_UP    = 90,   // Positive y-axis
    AXIS_LEFT  = 180,  // Negative x-axis
    AXIS_DOWN  = 270   // Negative y-axis
} axis_directions;

/**
 * Represents the analog joystick directions, which is useful for
 * setting the electrical orientation of the joystick, as installed,
 * that is installed in the physical "up" position.
 */
typedef enum {
    JS_RIGHT = 0,     // joystick's right is facing up
    JS_UP    = 1,     // joystick's up is facing up
    JS_LEFT  = 2,     // joystick's left is facing up
    JS_DOWN  = 3,     // joystick's down is facing up
    ORIENTATION_COUNT = 4
} vikstik_up_orientation;

/**
 * Represents the modes to designate how the joystick values are
 * interpreted.  The joystick can be used to emulate arrow keys,
 * W, A, S, and D for movement (similar to arrow keys), or it
 * can be used as-is, with analog values adjusted for the
 * configured range.
 */
typedef enum {
    VIKSTIK_SM_ANALOG, // stick mode: analog
    VIKSTIK_SM_WASD,   // stick mode: wasd
    VIKSTIK_SM_ARROWS, // stick mode: arrow keys
    VIKSTIK_SM_END     // end marker
} vikstik_stick_modes;

/**
 * Represents the joystick calibration data, including the neutral
 * values for the x and y axes, the inner and outer deadzones, and
 * the scaling factor for the joystick.
 */
typedef struct joystick_calibration {
    int16_t x_neutral, y_neutral;
    int16_t deadzone_inner, deadzone_outer;
    int32_t scale_factor;
} joystick_calibration_t;


/**
 * Used to hold x and y coordinates of the joystick for easy
 * returning from a function to avoid mutation and side-effects.
 */
typedef struct {
    int16_t x_coordinate;
    int16_t y_coordinate;
} vikstik_coordinate_t;

/**
 * Represents keycodes specifically for the joystick.
 */
enum vikstik_keycodes {
    VJS_QUAD = QK_KB,  // stick rotation quadrants
    VJS_SMOD           // stick mode
};

/**
 * The structure for persisted joystick configuration, including
 * the mode, and the electrical direction is installed in the physical
 * "up" direction.
 */
typedef struct {
    uint8_t mode;
    int16_t up_orientation;
} vikstik_config_t;

/**
 * Represents a joystick profile, including the parameters that are
 * necessary for calibration and usage (e.g., adjustments like scaling
 * and rotation if the joystick is installed with its electrical "up"
 * direction in a physical orientation other than "up").
 */
typedef struct {
    uint16_t actuation_point, deadzone_inner, deadzone_outer;
    int8_t   out_min, out_max;
    uint16_t raw_min, raw_max, stick_timer_ms;
} joystick_profile_t;

/**
 * A macro that sets up a profile for an analog thumb joystick.
 * This is a standard configuration that is used for joysticks
 * that have 10-bit resolution (1024 steps) potentiometers for
 * each axis, and a symmetrical 8-bit output range (-127 to 127).
 * The actuation point is set to 40, and the deadzones are set
 * to 60, for a comfortable range to avoid accidental inputs.
 */
#define JS_10BIT_SYM8BIT ((const joystick_profile_t) { \
    .actuation_point = 40, \
    .deadzone_inner = 60, \
    .deadzone_outer = 60, \
    .out_min = -127, \
    .out_max = 127, \
    .raw_min = 0, \
    .raw_max = 1023, \
    .stick_timer_ms = 5 \
})

int8_t  get_stick_up_orientation(void);
void    set_stick_up_orientation(vikstik_up_orientation up_orientation);
int16_t get_stick_up_angle(void);
void    step_stick_up_orientation(int8_t step);
void    step_stick_mode(void);
int8_t  calculate_direction(bool rotate);
void    process_vikstik(void);
