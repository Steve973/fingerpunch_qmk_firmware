// Copyright 2024 burkfers (@burkfers)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include "action_layer.h"
#include "src/fp_rgb_common.h"
#include QMK_KEYBOARD_H
#include "vikstik.h"
#include "layers.h"
#include "analog.h"
#include <math.h>

joystick_profile_t js_profile = JOYSTICK_PROFILE;

typedef void (*stick_mode_handler)(int8_t x, int8_t y);

static uint32_t stick_timer;
static joystick_calibration_t vikstik_calibration;
static vikstik_config_t vikstik_config;

static void handle_analog(int8_t x, int8_t y);
static void handle_wasd(int8_t x, int8_t y);
static void handle_arrows(int8_t x, int8_t y);

static const stick_direction_t angle_to_direction[ANGLE_DIVISIONS] = {
    // first (right) quadrant divisions
    DIR_R, DIR_RRU, DIR_RU, DIR_UUR,
    // second (up) quadrant divisions
    DIR_U, DIR_UUL, DIR_UL, DIR_ULL,
    // third (left) quadrant divisions
    DIR_L, DIR_LLD, DIR_LD, DIR_DDL,
    // fourth (down) quadrant divisions
    DIR_D, DIR_DDR, DIR_DR, DIR_DRR
};

/**
 * @brief Reads the raw x-axis value of the joystick.
 */
static int16_t read_x_axis(void) {
    return analogReadPin(VIK_GPIO_1);
}

/**
 * @brief Reads the raw y-axis value of the joystick.
 */
static int16_t read_y_axis(void) {
    return analogReadPin(VIK_GPIO_2);
}

/**
 * @brief Reads the raw analog joystick values.
 *
 * This function reads the raw analog joystick values from the configured pins.
 *
 * @return vikstik coordinate type with the raw x and y coordinates
 */
static vikstik_coordinate_t read_vikstik_raw(void) {
    return (vikstik_coordinate_t){
        .x_coordinate = read_x_axis(),
        .y_coordinate = read_y_axis()
    };
}

/**
 * @brief Calibrates the joystick input.
 *
 * This function updates the neutral values for the joystick based on the raw input.
 *
 * @param stick Pointer to the joystick calibration data.
 * @param rawx Raw x-axis value.
 * @param rawy Raw y-axis value.
 */
static void calibrate_vikstik(joystick_calibration_t* stick) {
    int16_t ideal_neutral = (js_profile.raw_min + js_profile.raw_max) / 2;
    int32_t total_x = 0, total_y = 0;
    int16_t x = 0, y = 0, max_neutral_x = 0, max_neutral_y = 0;

    for (int i = 0; i < CALIBRATION_SAMPLE_COUNT; i++) {
        x = read_x_axis();
        y = read_y_axis();
        total_x += x;
        total_y += y;
        max_neutral_x = x > max_neutral_x ? x : max_neutral_x;
        max_neutral_y = y > max_neutral_y ? y : max_neutral_y;
        wait_ms(js_profile.stick_timer_ms);
    }

    stick->x_neutral = total_x / CALIBRATION_SAMPLE_COUNT;
    stick->y_neutral = total_y / CALIBRATION_SAMPLE_COUNT;

    // Calculate scale_factor using fixed-point arithmetic
    uint16_t max_neutral = max_neutral_x > max_neutral_y ? max_neutral_x : max_neutral_y;
    stick->scale_factor = (FIXED_POINT_SCALE * js_profile.out_max) / (js_profile.raw_max - max_neutral);

    int16_t x_drift = abs(stick->x_neutral - ideal_neutral);
    int16_t y_drift = abs(stick->y_neutral - ideal_neutral);
    int16_t min_deadzone_inner = x_drift > y_drift ? x_drift : y_drift;
    stick->deadzone_inner = min_deadzone_inner > js_profile.deadzone_inner ? min_deadzone_inner : js_profile.deadzone_inner;
    stick->deadzone_outer = js_profile.deadzone_outer;
}

/**
 * @brief Array of joystick mode handlers.
 *
 * This constant array defines the different handling modes for the joystick.
 * Each mode corresponds to a specific function that processes the joystick input
 * in a different way. The available modes are:
 * - `handle_analog`: Handles analog joystick input.
 * - `handle_wasd`: Maps joystick input to WASD keys.
 * - `handle_arrows`: Maps joystick input to arrow keys.
 */
static const stick_mode_handler stick_modes[] = {
    handle_analog,
    handle_wasd,
    handle_arrows
};

/**
 * @brief Clamps a value between a minimum and maximum range.
 *
 * This function ensures that the input value is within the specified range.
 * If the value is less than the minimum, it returns the minimum.
 * If the value is greater than the maximum, it returns the maximum.
 * Otherwise, it returns the value itself.
 *
 * @param val The value to be clamped.
 * @param min The minimum allowable value.
 * @param max The maximum allowable value.
 * @return The clamped value.
 */
static inline int16_t clamp(int16_t val, int16_t min, int16_t max) {
    return (val < min) ? min : ((val > max) ? max : val);
}

/**
 * @brief Projects a value from one range to another.
 *
 * This function normalizes the input value from the source range and scales it
 * to the target range. It ensures that the scaled value is within the target range.
 *
 * @param val The value to be projected.
 * @param rmin The minimum value of the source range.
 * @param rmax The maximum value of the source range.
 * @param tmin The minimum value of the target range.
 * @param tmax The maximum value of the target range.
 * @return The projected value within the target range.
 */
static inline int16_t project(int16_t val, int16_t rmin, int16_t rmax, int16_t tmin, int16_t tmax) {
    // Check if the range is zero or negative
    if (abs(rmax - rmin) <= 0) return tmin;

    // Perform the projection calculation
    int32_t normalized = ((int32_t)(val - rmin) * 1024) / (rmax - rmin);
    int32_t scaled = (normalized * (tmax - tmin) + 512) / 1024 + tmin;

    return clamp(scaled, tmin, tmax);
}

/**
 *
 * @brief Handles the registration and unregistration of keys based on joystick axis movement.
 *
 * This function determines the current and previous states of the joystick axis and registers
 * or unregisters the corresponding keys based on the actuation point.
 *
 * @param curr The current axis value.
 * @param prev The previous axis value.
 * @param pos_key The key to register when the axis value is positive.
 * @param neg_key The key to register when the axis value is negative.
 */
static void handle_axis(int8_t curr, int8_t prev, uint16_t pos_key, uint16_t neg_key) {
    int8_t curr_state = (curr > js_profile.actuation_point) - (curr < -js_profile.actuation_point);
    int8_t prev_state = (prev > js_profile.actuation_point) - (prev < -js_profile.actuation_point);
    bool should_register = (curr_state != 0);
    if (curr_state != prev_state) {
        uint16_t key_to_handle = (should_register) ?
            (curr_state > 0 ? pos_key : neg_key) :
            (prev_state > 0 ? pos_key : neg_key);
        if (should_register) {
            register_code16(key_to_handle);
            dprintf("registering %d\n", key_to_handle);
        } else {
            unregister_code16(key_to_handle);
            dprintf("unregistering %d\n", key_to_handle);
        }
    }
}

/**
 * @brief Handles the registration and unregistration of four keys based on joystick movement.
 *
 * This function processes the joystick x and y values and registers or unregisters
 * the corresponding keys (up, left, down, right) based on the actuation point.
 *
 * @param x The x-axis value.
 * @param y The y-axis value.
 * @param u The key to register when the y-axis value is positive (up).
 * @param l The key to register when the x-axis value is negative (left).
 * @param d The key to register when the y-axis value is negative (down).
 * @param r The key to register when the x-axis value is positive (right).
 */
static void handle_vikstik_keys_4(int8_t x, int8_t y, uint16_t u, uint16_t l, uint16_t d, uint16_t r) {
    static int8_t px, py;
    handle_axis(y, py, u, d);
    handle_axis(x, px, r, l);
    px = x;
    py = y;
}

/**
 * @brief Handles analog joystick input.
 *
 * This function sets the joystick axis values based on the input x and y values.
 *
 * @param x The x-axis value.
 * @param y The y-axis value.
 */
static void handle_analog(int8_t x, int8_t y) {
    joystick_set_axis(0, x);
    joystick_set_axis(1, y);
}

/**
 * @brief Handles WASD key input based on joystick movement.
 *
 * This function maps the joystick x and y values to the corresponding WASD keys.
 *
 * @param x The x-axis value.
 * @param y The y-axis value.
 */
static void handle_wasd(int8_t x, int8_t y) {
    handle_vikstik_keys_4(x, y, KC_W, KC_A, KC_S, KC_D);
}

/**
 * @brief Handles arrow key input based on joystick movement.
 *
 * This function maps the joystick x and y values to the corresponding arrow keys.
 * It also prints a message every second to indicate that it is handling arrow keys.
 *
 * @param x The x-axis value.
 * @param y The y-axis value.
 */
static void handle_arrows(int8_t x, int8_t y) {
    handle_vikstik_keys_4(x, y, KC_UP, KC_LEFT, KC_DOWN, KC_RGHT);
}

/**
 * @brief Gets the orientation of the joystick's direction facing "up" from the perspective
 * of the user, based on how it is installed.
 *
 * For example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 *
 *      R
 *      ↑
 * U ←  ●  → D
 *      ↓
 *      L
 *
 * the installed orientation has the "RIGHT" direction facing up.  So, the value for
 * the "up orientation", in this case, is "RIGHT".
 *
 * @return The current up orientation value.
 */
int8_t get_stick_up_orientation() {
    return vikstik_config.up_orientation;
}

/**
 * @brief Sets the orientation of the joystick's direction facing "up" from the perspective
 * of the user, based on how it is installed.
 *
 * For example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 *
 *      R
 *      ↑
 * U ←  ●  → D
 *      ↓
 *      L
 *
 * the installed orientation has the "RIGHT" direction facing up.  So, the value for
 * the "up orientation", in this case, is "RIGHT".
 *
 * @param up_orientation The new stick up orientation value.
 */
void set_stick_up_orientation(vikstik_up_orientation up_orientation) {
    vikstik_config.up_orientation = up_orientation;
}

/**
 * @brief Adjusts the installed north value by one step clockwise.
 *
 * This function increments the current installed north value by one step clockwise,
 * and updates the configuration. It also prints the new stick up orientation value.
 *
 * @param step The step value to adjust the stick up orientation.
 */
void step_stick_up_orientation(int8_t step) {
    vikstik_up_orientation new_orientation = (vikstik_up_orientation)((get_stick_up_orientation() + step) % ORIENTATION_COUNT);
    set_stick_up_orientation(new_orientation);
    dprintf("stick up orientation is now %i\n", get_stick_up_orientation());
}

/**
 * @brief Gets the angle of the joystick's "up" direction based on how it is installed.
 *
 * This function returns the current angle of the joystick's "up" direction based on how it is installed.
 * For example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 *
 *      R
 *      ↑
 * U ←  ●  → D
 *      ↓
 *      L
 *
 * the value would be 90 degrees.
 *
 * @return The current up angle value.
 */
int16_t get_stick_up_angle() {
    return vikstik_config.up_orientation * 90;
}

/**
 * @brief Cycles through the joystick modes.
 *
 * This function increments the joystick mode and wraps around if it exceeds the
 * maximum mode value. It also resets the joystick axes and prints the new mode.
 */
void step_stick_mode(void) {
    joystick_set_axis(0, 0);
    joystick_set_axis(1, 0);
    vikstik_config.mode = (vikstik_config.mode + 1) % VIKSTIK_SM_END;
    dprintf("Stick mode now %i\n", vikstik_config.mode);
}

/**
 * @brief Calculates the angle based on the joystick position.
 *
 * This function calculates the angle of the joystick position
 * in degrees, ranging from 0 to 360.  This is the "trig" implementation
 * that uses trigonometry to calculate the angle, and is more precise
 * than the "lite" implementation. This is most suitable for controllers
 * that have more resources available, and can handle floating-point
 * calculations.
 *
 * @param coordinates The coordinates read from the joystick
 * @param rotate flag to indicate if the calculated angle value should be rotated
 * @return The angle in degrees.
 */
static int16_t calculate_angle_trig(vikstik_coordinate_t coordinates, bool rotate) {
    float angle = atan2(coordinates.y_coordinate, coordinates.x_coordinate) * (180.0 / M_PI);
    angle = rotate ? fmod(angle - get_stick_up_angle() + 360, 360.0) : angle;
    return fmod(angle + 360, 360.0);
}

/**
 * @brief Calculates the angle based on the joystick position.
 *
 *
 * This function calculates the angle of the joystick position
 * in degrees, ranging from 0 to 360.  This is the "lite" implementation
 * that does not use trigonometry, and is more efficient for controllers
 * that are more limited in resources, and cannot handle floating-point
 * calculations.  This implementation is less precise than the "trig"
 * implementation, but is suitable for the majority of use cases.
 *
 * @param coordinates The coordinates read from the joystick
 * @param rotate flag to indicate if the calculated angle value should be rotated
 * @return The angle in degrees.
 */
static int16_t calculate_angle_lite(vikstik_coordinate_t coordinates, bool rotate) {
    int16_t x = coordinates.x_coordinate;
    int16_t y = coordinates.y_coordinate;

    if (x == 0 && y == 0) {
        return 0;  // Neutral position
    }

    uint8_t octant = 0;
    if (y < 0) octant = 4;
    if (x < 0) octant += 2;
    if (abs(y) > abs(x)) octant += 1;

    uint16_t ratio;
    if (octant & 1) {
        ratio = ((uint32_t)abs(x) << 8) / abs(y);
    } else {
        ratio = ((uint32_t)abs(y) << 8) / abs(x);
    }

    uint8_t fine = 0;
    if (ratio > 106) fine = 1;
    if (ratio > 181) fine = 2;

    uint8_t dir_index = octant * 2 + fine;

    int16_t angle = angle_to_direction[dir_index];

    if (rotate) {
        angle = (angle - get_stick_up_angle() + 360) % 360;
    }

    return angle;
}

/**
 * @brief Calculates the angle based on the joystick position.
 *
 * This function calculates the angle of the joystick position
 * in degrees, ranging from 0 to 360.  The calculation method
 * is determined by the `is_joystick_lite` flag, which is set
 * based on the arguments provided at compile time.
 *
 * @param coordinates The coordinates read from the joystick
 * @param rotate flag to indicate if the calculated angle value should be rotated
 * @return The angle in degrees.
 */
static int16_t calculate_angle(vikstik_coordinate_t coordinates, bool rotate) {
    return is_joystick_lite ? calculate_angle_lite(coordinates, rotate) : calculate_angle_trig(coordinates, rotate);
}

/**
 * @brief Applies scaling and deadzone to the joystick input.
 *
 * This function scales the raw joystick input values to the JOYSTICK_OUT_MIN to JOYSTICK_OUT_MAX range,
 * applies inner deadzone to the calibrated values, and assigns the scaled values to the output.
 *
 * @param rawx Raw x-axis value.
 * @param rawy Raw y-axis value.
 * @param outx Pointer to store the processed x-axis value.
 * @param outy Pointer to store the processed y-axis value.
 */
static vikstik_coordinate_t apply_scaling_and_deadzone(vikstik_coordinate_t raw_coordinates) {
    // Convert to signed values centered at 0
    int16_t x = raw_coordinates.x_coordinate - vikstik_calibration.x_neutral;
    int16_t y = raw_coordinates.y_coordinate - vikstik_calibration.y_neutral;

    // Calculate squared distance from center (avoid sqrt)
    int32_t distance_sq = (int32_t)x * x + (int32_t)y * y;

    // Filter for values outside of the deadzone
    if (distance_sq < (int32_t)vikstik_calibration.deadzone_inner * vikstik_calibration.deadzone_inner) {
        x = y = 0;
    } else {
        // Apply the scaling to x and y using fixed-point arithmetic
        x = (x * vikstik_calibration.scale_factor) / FIXED_POINT_SCALE;
        y = (y * vikstik_calibration.scale_factor) / FIXED_POINT_SCALE;
    }

    return (vikstik_coordinate_t){
        // Clamp to output range
        .x_coordinate = clamp(x, js_profile.out_min, js_profile.out_max),
        .y_coordinate = clamp(y, js_profile.out_min, js_profile.out_max)
    };
}

/**
 * @brief Applies rotation to the joystick values based on the current configuration.
 *
 * This function rotates the joystick values according to the configured up orientation value.
 * For example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 *
 *      R
 *      ↑
 * U ←  ●  → D
 *      ↓
 *      L
 *
 * Then the "up orientation" property of the joystick configuration would be "RIGHT", and
 * the joystick coordinates would have to be rotated 270 degrees counterclockwise.
 *
 * @param x Pointer to the x-axis value.
 * @param y Pointer to the y-axis value.
 */
static vikstik_coordinate_t handle_rotation(vikstik_coordinate_t coordinates) {
    switch (vikstik_config.up_orientation) {
        case JS_LEFT: // rotate 90 degrees counterclockwise
            return (vikstik_coordinate_t){
                .x_coordinate = -coordinates.y_coordinate,
                .y_coordinate = coordinates.x_coordinate
            };
            break;
        case JS_DOWN: // rotate 180 degrees counterclockwise
            return (vikstik_coordinate_t){
                .x_coordinate = -coordinates.x_coordinate,
                .y_coordinate = -coordinates.y_coordinate
            };
            break;
        case JS_RIGHT: // rotate 270 degrees counterclockwise
            return (vikstik_coordinate_t){
                .x_coordinate = coordinates.y_coordinate,
                .y_coordinate = -coordinates.x_coordinate
            };
            break;
        case JS_UP: // up is up, so do not rotate
        default:
            return (vikstik_coordinate_t){
                .x_coordinate = coordinates.x_coordinate,
                .y_coordinate = coordinates.y_coordinate
            };
            break;
    }
}

/**
 * @brief Get the raw (unaltered, un-rotated) direction based on joystick position.
 *
 * This function determines the direction based on the joystick position without any rotation.
 * It returns the direction number (0-3) based on the angle of the joystick.
 *
 * @return The direction number (0-3), or -1 if the joystick is at its neutral position.
 */
static int8_t calculate_raw_direction(vikstik_coordinate_t raw_coordinates) {
    int8_t direction = -1;
    vikstik_coordinate_t scaled_coordinates = apply_scaling_and_deadzone(raw_coordinates);

    if (scaled_coordinates.x_coordinate != 0 && scaled_coordinates.y_coordinate != 0) {
        // Some trig sauce to determine the angle for calculations
        // where we may need a more precise/exact rotation, e.g.,
        // when we need something other than arrow keys or "wasd"
        // directions:
        int16_t angle = calculate_angle(scaled_coordinates, false);

        // For more simple or discrete translation, determine direction
        // based on 45-degree sectors centered on cardinal directions:
        if (angle >= DIR_UUR && angle < DIR_UUL) {
            direction = JS_UP;
        } else if (angle >= DIR_ULL && angle < DIR_LLD) {
            direction = JS_LEFT;
        } else if (angle >= DIR_DDL && angle < DIR_DDR) {
            direction = JS_DOWN;
        } else if (((angle > DIR_DRR) && (angle <= 360)) ||
                ((angle >= DIR_R) && (angle < DIR_RRU))) {
            direction = JS_RIGHT;
        }
    }
    return direction;
}

/**
 * @brief Reads the joystick input.
 *
 * This function reads the raw joystick input values, processes
 * deadzones, scales the values, and applies rotation based on
 * the current configuration.
 *
 * @param outx Pointer to store the processed x-axis value.
 * @param outy Pointer to store the processed y-axis value.
 */
static vikstik_coordinate_t read_vikstik(void) {
    vikstik_coordinate_t coordinates = read_vikstik_raw();
    coordinates = apply_scaling_and_deadzone(coordinates);
    coordinates = handle_rotation(coordinates);

    // Directly assign the scaled values to the output
    return (vikstik_coordinate_t){
        .x_coordinate = clamp(coordinates.x_coordinate, js_profile.out_min, js_profile.out_max),
        .y_coordinate = clamp(coordinates.y_coordinate, js_profile.out_min, js_profile.out_max)
    };
}

/**
 * @brief Get the raw (unaltered, un-rotated) direction based on joystick position.
 *
 * This function determines the direction based on the joystick position without any rotation.
 * It returns the direction number (0-3) based on the angle of the joystick.
 *
 * @param out_direction a pointer to return the calculated direction to the caller
 * @param out_angle a pointer to return the calculated angle to the caller
 * @param rotate a boolean flag to indicate if the caller wants the joystick rotated
 *               based on the configured "up orientation" of the joystick
 * @return The direction number (0-3), or -1 if the joystick is at its neutral position.
 */
int8_t calculate_direction(bool rotate) {
    vikstik_coordinate_t coordinates = read_vikstik_raw();
    int8_t direction = calculate_raw_direction(coordinates);
    if (rotate && direction > -1) {
        switch (vikstik_config.up_orientation) {
            case JS_LEFT:
                direction = (direction + 3) % ORIENTATION_COUNT;
                break;
            case JS_DOWN:
                direction = (direction + 2) % ORIENTATION_COUNT;
                break;
            case JS_RIGHT:
                direction = (direction + 1) % ORIENTATION_COUNT;
                break;
            case JS_UP:
            default:
                break;
       }
    }
    return direction;
}

/**
 * @brief Handles actions for the LOWER layer based on the direction.
 *
 * This function performs specific actions when the active layer is LOWER.
 * The actions are determined by the direction provided as input.
 *
 * @param direction The direction of the joystick movement. Expected values are:
 *                 - UP: Perform the action associated with the UP direction.
 *                 - DOWN: Perform the action associated with the DOWN direction.
 *                 - RIGHT: Perform the action associated with the RIGHT direction.
 *                 - LEFT: Perform the action associated with the LEFT direction.
 */
static void handle_lower_layer_rgb(int8_t direction) {
    switch (direction) {
        case JS_UP:
            fp_rgblight_step();
            break;
        case JS_DOWN:
            fp_rgblight_step_reverse();
            break;
        case JS_RIGHT:
            fp_rgblight_increase_val();
            break;
        case JS_LEFT:
            fp_rgblight_decrease_val();
            break;
        default:
            break;
    }
}

/**
 * @brief Handles actions for the RAISE layer based on the direction.
 *
 * This function performs specific actions when the active layer is RAISE.
 * The actions are determined by the direction provided as input.
 *
 * @param direction The direction of the joystick direction. Expected values are:
 *                 - UP: Perform the action associated with the UP direction.
 *                 - DOWN: Perform the action associated with the DOWN direction.
 *                 - RIGHT: Perform the action associated with the RIGHT direction.
 *                 - LEFT: Perform the action associated with the LEFT direction.
 */
static void handle_raise_layer_rgb(int8_t direction) {
    switch (direction) {
        case JS_UP:
            fp_rgblight_increase_sat();
            break;
        case JS_DOWN:
            fp_rgblight_decrease_sat();
            break;
        case JS_RIGHT:
            fp_rgblight_increase_hue();
            break;
        case JS_LEFT:
            fp_rgblight_decrease_hue();
            break;
        default:
            break;
    }
}

/**
 * @brief Handles actions for the ADJUST layer based on the direction.
 *
 * This function performs specific actions when the active layer is ADJUST.
 * The actions are determined by the direction provided as input.
 *
 * @param direction The direction of the joystick direction. Expected values are:
 *                 - UP: Perform the action associated with the UP direction.
 *                 - DOWN: Perform the action associated with the DOWN direction.
 *                 - RIGHT: Perform the action associated with the RIGHT direction.
 *                 - LEFT: Perform the action associated with the LEFT direction.
 */
static void handle_adjust_layer_rgb(int8_t direction) {
    switch (direction) {
        case JS_UP:
            rgb_matrix_enable();
            break;
        case JS_DOWN:
            rgb_matrix_disable();
            break;
        case JS_RIGHT:
            rgb_matrix_increase_speed();
            break;
        case JS_LEFT:
            rgb_matrix_decrease_speed();
            break;
        default:
            break;
    }
}

/**
 * @brief Handles the joystick input and processes it according to the current mode.
 *
 * This function reads the joystick input, processes it based on the current mode,
 * and updates the joystick axes accordingly.
 */
static void handle_vikstik(void) {
    uint8_t active_layer = get_highest_layer(layer_state);
    if (active_layer < _LOWER) {
        if (vikstik_config.mode < VIKSTIK_SM_END) {
            vikstik_coordinate_t coordinates = read_vikstik();
            stick_modes[vikstik_config.mode](coordinates.x_coordinate, coordinates.y_coordinate);
        }
    } else {
        int8_t direction = calculate_direction(true);
        switch (active_layer) {
            case _LOWER:
                handle_lower_layer_rgb(direction);
                break;
            case _RAISE:
                handle_raise_layer_rgb(direction);
                break;
            case _ADJUST:
                handle_adjust_layer_rgb(direction);
                break;
            default:
                break;
        }
    }
}

/**
 * @brief Initializes the keyboard and reads the joystick configuration from EEPROM.
 *
 * This function sets the number of calibration samples for the joystick, reads the
 * joystick configuration from EEPROM, and checks if the configuration is uninitialized
 * or invalid. If so, it initializes the configuration with default values.
 */
void keyboard_post_init_user(void) {
    calibrate_vikstik(&vikstik_calibration);
    vikstik_config.mode = JS_MODE;
    vikstik_config.up_orientation = JS_UP_ORIENTATION;
}

/**
 * @brief Processes the joystick input at regular intervals.
 *
 * This function checks if the specified time interval has elapsed and, if so,
 * processes the joystick input by calling the handle_vikstik function.
 */
void process_vikstik(void) {
    if (timer_elapsed(stick_timer) > js_profile.stick_timer_ms) {
        stick_timer = timer_read32();
        handle_vikstik();
    }
}
