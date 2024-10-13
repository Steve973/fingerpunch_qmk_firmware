// Copyright 2024 burkfers (@burkfers)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "vikstik.h"
#include "analog.h"
#include <math.h>

_Static_assert(sizeof(vikstik_config_t) == EECONFIG_USER_DATA_SIZE, "Mismatch in keyboard EECONFIG stored data");

typedef void (*stick_mode_handler)(int8_t x, int8_t y);

typedef struct joystick_calibration {
    int16_t x_neutral;
    int16_t y_neutral;
} joystick_calibration_t;

static uint32_t stick_timer;
static joystick_calibration_t vikstik_calibration;
static vikstik_config_t vikstik_config;

static void handle_analog(int8_t x, int8_t y);
static void handle_wasd(int8_t x, int8_t y);
static void handle_arrows(int8_t x, int8_t y);

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
 * @brief Default configuration for the joystick.
 *
 * This constant defines the default settings for the joystick, including the mode,
 * inner and outer deadzones, and the installed orientation of the joystick that
 * indicates which way "up" is facing, as physically installed.  For example, if
 * up is facing left, the value would be "L".
 */
static const vikstik_config_t vikstik_config_DEFAULT = {
    .mode = VIKSTIK_SM_ARROWS,
    .deadzone_inner = INNER_DEADZONE,
    .deadzone_outer = OUTER_DEADZONE,
    .up_orientation = UP
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
static inline float clamp(float val, float min, float max) {
    return MAX_VAL(min, MIN_VAL(val, max));
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
static inline float project(float val, float rmin, float rmax, float tmin, float tmax) {
    if (fabs(rmax - rmin) < 1e-6) return tmin;
    float normalized = (val - rmin) / (rmax - rmin);
    float scaled = normalized * (tmax - tmin) + tmin;
    return clamp(scaled, tmin, tmax);
}

/**
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
    int8_t curr_state = (curr > ACTUATION_POINT) - (curr < -ACTUATION_POINT);
    int8_t prev_state = (prev > ACTUATION_POINT) - (prev < -ACTUATION_POINT);
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
    handle_axis(y, py, d, u);
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
    static uint32_t last_print = 0;
    if (timer_elapsed32(last_print) > 1000) { // Print every second
        uprintf("Handling arrows\n");
        last_print = timer_read32();
    }
    handle_vikstik_keys_4(x, y, KC_UP, KC_LEFT, KC_DOWN, KC_RGHT);
}

/**
 * @brief Sets the inner deadzone for the joystick.
 *
 * This function updates the inner deadzone value for the joystick and saves the
 * configuration to persistent storage.
 *
 * @param idz The new inner deadzone value.
 */
void set_deadzone_inner(int8_t idz) {
    vikstik_config.deadzone_inner = idz;
    eeconfig_update_user_datablock(&vikstik_config);
}

/**
 * @brief Gets the current inner deadzone value for the joystick.
 *
 * This function returns the current inner deadzone value from the joystick configuration.
 *
 * @return The current inner deadzone value.
 */
int8_t get_deadzone_inner() {
    return vikstik_config.deadzone_inner;
}

/**
 * @brief Sets the outer deadzone value for the joystick.
 *
 * This function returns the current outer deadzone value from the joystick configuration.
 * 
 * @param odz The new outer deadzone value.
 */
void set_deadzone_outer(int8_t odz) {
    vikstik_config.deadzone_outer = odz;
    eeconfig_update_user_datablock(&vikstik_config);
}

/**
 * @brief Gets the current outer deadzone value for the joystick.
 *
 * This function returns the current outer deadzone value from the joystick configuration.
 *
 * @return The current outer deadzone value.
 */
int8_t get_deadzone_outer() {
    return vikstik_config.deadzone_outer;
}

/**
 * @brief Gets the orientation of the joystick's "up" direction based on how it is installed.
 *
 * This function returns the current installed "north" direction based on how it is installed.
 * For example, if the joystick is installed with the "up" direction facing left, the value
 * would be "L".
 *
 * @return The current up orientation value.
 */
int8_t get_stick_up_orientation() {
    return vikstik_config.up_orientation;
}

/**
 * @brief Sets the orientation of the joystick's "up" direction based on how it is installed.
 *
 * This function updates the orientation of the joystick's "up" direction, and saves the
 * configuration to persistent storage.
 *
 * @param quadrants The new stick up orientation value.
 */
void set_stick_up_orientation(vikstik_up_orientation up_orientation) {
    vikstik_config.up_orientation = up_orientation;
    eeconfig_update_user_datablock(&vikstik_config);
}

/**
 * @brief Adjusts the installed north value by one step clockwise.
 *
 * This function increments the current installed north value by one step clockwise,
 * and updates the configuration. It also prints the new stick quadrants value.
 *
 * @param step The step value to adjust the stick quadrants.
 */
void step_stick_up_orientation(int8_t step) {
    vikstik_up_orientation new_orientation = (vikstik_up_orientation)((get_stick_up_orientation() + step) % ORIENTATION_COUNT);
    set_stick_up_orientation(new_orientation);
    dprintf("stick quadrants now %i\n", get_stick_up_orientation());
}

/**
 * @brief Gets the angle of the joystick's "up" direction based on how it is installed.
 *
 * This function returns the current angle of the joystick's "up" direction based on how it is installed.
 * For example, if the joystick is installed with the "up" direction facing left, the value
 * would be 90 degrees.
 *
 * @return The current up angle value.
 */
int16_t get_stick_up_angle() {
    return vikstik_config.up_angle;
}

/**
 * @brief Sets the angle of the joystick's "up" direction based on how it is installed.
 *
 * This function updates the angle of the joystick's "up" direction, and saves the
 * configuration to persistent storage.
 *
 * @param angle The new stick up angle value.
 */
void set_stick_up_angle(int16_t angle) {
    vikstik_config.up_angle = angle;
    eeconfig_update_user_datablock(&vikstik_config);
}

/**
 * @brief Cycles through the joystick modes.
 *
 * This function increments the joystick mode and wraps around if it exceeds the
 * maximum mode value. It also resets the joystick axes and prints the new mode.
 */
void step_stick_mode() {
    joystick_set_axis(0, 0);
    joystick_set_axis(1, 0);
    vikstik_config.mode = (vikstik_config.mode + 1) % VIKSTIK_SM_END;
    dprintf("Stick mode now %i\n", vikstik_config.mode);
}

void read_vikstik_raw(int16_t* rawx, int16_t* rawy) {
    *rawx = analogReadPin(VIK_GPIO_1);
    *rawy = analogReadPin(VIK_GPIO_2);
}

void apply_scaling_and_deadzone(int16_t rawx, int16_t rawy, int8_t* outx, int8_t* outy) {
    // Convert to signed values centered at 0
    int16_t x = rawx - vikstik_calibration.x_neutral;
    int16_t y = rawy - vikstik_calibration.y_neutral;

    // Apply inner deadzone to calibrated values
    x = (abs(x) < INNER_DEADZONE) ? 0 : x;
    y = (abs(y) < INNER_DEADZONE) ? 0 : y;

    // Scale x and y to JOYSTICK_OUT_MIN to JOYSTICK_OUT_MAX range
    int8_t scalex = (x > 0) ? project(x, 0, JOYSTICK_MAX_RAW/2 - OUTER_DEADZONE, 0, JOYSTICK_OUT_MAX) :
                    (x < 0) ? project(x, -JOYSTICK_MAX_RAW/2 + OUTER_DEADZONE, 0, JOYSTICK_OUT_MIN, 0) : 0;
                    
    int8_t scaley = (y > 0) ? project(y, 0, JOYSTICK_MAX_RAW/2 - OUTER_DEADZONE, 0, JOYSTICK_OUT_MAX) :
                    (y < 0) ? project(y, -JOYSTICK_MAX_RAW/2 + OUTER_DEADZONE, 0, JOYSTICK_OUT_MIN, 0) : 0;
    
    // Directly assign the scaled values to the output
    *outx = clamp(scalex, JOYSTICK_OUT_MIN, JOYSTICK_OUT_MAX);
    *outy = clamp(scaley, JOYSTICK_OUT_MIN, JOYSTICK_OUT_MAX);
}

/**
 * @brief Applies rotation to the joystick values based on the current configuration.
 *
 * This function rotates the joystick values according to the configured quadrants value.
 *
 * @param x Pointer to the x-axis value.
 * @param y Pointer to the y-axis value.
 */
static void handle_rotation(int8_t x, int8_t y, int8_t* outx, int8_t* outy) {
    switch (vikstik_config.up_orientation) {
        case LEFT: // rotate 90 degrees
            *outx = y;
            *outy = -x;
            break;
        case DOWN: // rotate 180 degrees
            *outx = -x;
            *outy = -y;
            break;
        case RIGHT: // rotate 270 degrees
            *outx = -y;
            *outy = x;
            break;
        case UP: // up is up, so do not rotate
        default:
            *outx = x;
            *outy = y;
            break;
    }
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
    int16_t x = 0, y = 0;
    int32_t total_x = 0;
    int32_t total_y = 0;

    for (int i = 0; i < CALIBRATION_SAMPLE_COUNT; i++) {
        read_vikstik_raw(&x, &y);
        total_x += x;
        total_y += y;
        wait_ms(5); // 5 ms delay between samples
    }

    stick->x_neutral = total_x / CALIBRATION_SAMPLE_COUNT;
    stick->y_neutral = total_y / CALIBRATION_SAMPLE_COUNT;
}

/**
 * @brief Reads and calibrates the joystick input.
 *
 * This function reads the raw joystick input values, processes
 * deadzones, scales the values, and applies rotation based on
 * the current configuration.
 *
 * @param outx Pointer to store the processed x-axis value.
 * @param outy Pointer to store the processed y-axis value.
 */
static void read_vikstik(int8_t* outx, int8_t* outy) {
    int16_t x = 0, y = 0;
    read_vikstik_raw(&x, &y);

    int8_t scalex = 0, scaley = 0;
    apply_scaling_and_deadzone(x, y, &scalex, &scaley);

    // Apply rotation
    int8_t rotatex = 0, rotatey = 0;
    handle_rotation(scalex, scaley, &rotatex, &rotatey);
    
    // Directly assign the scaled values to the output
    *outx = clamp(scalex, JOYSTICK_OUT_MIN, JOYSTICK_OUT_MAX);
    *outy = clamp(scaley, JOYSTICK_OUT_MIN, JOYSTICK_OUT_MAX);
}

/**
 * @brief Handles the joystick input and processes it according to the current mode.
 *
 * This function reads the joystick input, processes it based on the current mode,
 * and updates the joystick axes accordingly.
 *
 * @param stick Pointer to the joystick calibration data.
 */
static void handle_vikstik(void) {
    int8_t x = 0, y = 0;
    read_vikstik(&x, &y);
    
    if (vikstik_config.mode < VIKSTIK_SM_END) {
        stick_modes[vikstik_config.mode](x, y);
    }
}

/**
 * @brief Get the raw (unaltered, un-rotated) quadrant based on joystick position.
 *
 * This function determines the quadrant based on the joystick position without any rotation.
 * It returns the quadrant number (0-3) based on the angle of the joystick.
 * @return The quadrant number (0-3), or -1 if the joystick is at its neutral position.
 */
void get_raw_quadrant(int8_t* out_quadrant, int16_t* out_angle) {
    int8_t x = 0, y = 0;
    read_vikstik(&x, &y);
    if (x == 0 && y == 0) {
        *out_quadrant = -1;
        *out_angle = -1;
    } else {
        float angle = atan2(y, x) * (180.0 / M_PI);
        angle = angle < 0 ? angle + 360 : angle;
        int8_t result = -1;
        if (((angle >= 337) && (angle < 360)) ||((angle >= 0) && (angle < 23))) {
            result = 0; // raw "top" is up
        } else if (angle >= 67 && angle < 113) {
            result = 3; // raw "right" is up
        } else if (angle >= 157 && angle < 203) {
            result = 2; // raw "bottom" is up
        } else if (angle >= 247 && angle < 293) {
            result = 1; // raw "left" is up
        }
        *out_quadrant = result;
        *out_angle = angle;
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
    eeconfig_read_user_datablock(&vikstik_config);
    // Check if all values are zero (indicating uninitialized EEPROM)
    bool is_uninitialized = (vikstik_config.mode == 0 &&
                             vikstik_config.deadzone_inner == 0 &&
                             vikstik_config.deadzone_outer == 0 &&
                             vikstik_config.up_orientation == UP);
    // Check if any field has an invalid value
    bool is_invalid = (vikstik_config.mode >= VIKSTIK_SM_END ||
                       vikstik_config.deadzone_inner < -127 || vikstik_config.deadzone_inner > 127 ||
                       vikstik_config.deadzone_outer < -127 || vikstik_config.deadzone_outer > 127 ||
                       vikstik_config.up_orientation < UP || vikstik_config.up_orientation >= ORIENTATION_COUNT);
    if (is_uninitialized || is_invalid) {
        eeconfig_init_user();
    }
}

/**
 * @brief Initializes the EEPROM with default joystick configuration values.
 *
 * This function sets the joystick configuration to default values and updates
 * the EEPROM with these values.
 */
void eeconfig_init_user(void) {
    vikstik_config = vikstik_config_DEFAULT;
    eeconfig_update_user_datablock(&vikstik_config);
}

/**
 * @brief Processes the joystick input at regular intervals.
 *
 * This function checks if the specified time interval has elapsed and, if so,
 * processes the joystick input by calling the handle_vikstik function.
 */
void process_vikstik(void) {
    if (timer_elapsed(stick_timer) > STICK_TIMER_MS) {
        handle_vikstik();
        stick_timer = timer_read32();
    }
}
