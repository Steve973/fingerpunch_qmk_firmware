// Copyright 2024 burkfers (@burkfers)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "vikstik.h"
#include "analog.h"
#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define MAX_DISTANCE_RAW sqrt(2 * JOYSTICK_MAX_RAW * JOYSTICK_MAX_RAW)
#define MAX_DISTANCE_OUT sqrt(2 * JOYSTICK_MAX_OUT * JOYSTICK_MAX_OUT)

_Static_assert(sizeof(vikstik_config_t) == EECONFIG_USER_DATA_SIZE, "Mismatch in keyboard EECONFIG stored data");

typedef void (*stick_mode_handler)(int8_t x, int8_t y);

typedef struct joystick_calibration {
    int16_t x_neutral;
    int16_t y_neutral;
    int16_t deadzone_inner;
    int16_t deadzone_outer;
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
 * inner and outer deadzones, and the installed orientation of the joystick.  For
 * example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 * 
 *          R
 *       U--|--D
 *          L
 * 
 * the installed orientation has the "RIGHT" direction facing up.  So, the value for
 * the "up orientation", in this case, is "RIGHT".
 */
static const vikstik_config_t VIKSTIK_CONFIG_DEFAULT = {
    .mode = VIKSTIK_SM_ARROWS,
    .up_orientation = LEFT,
    .up_angle = 0
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
 *          R
 *       U--|--D
 *          L
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
 *          R
 *       U--|--D
 *          L
 * 
 * the installed orientation has the "RIGHT" direction facing up.  So, the value for
 * the "up orientation", in this case, is "RIGHT".
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
 * For example, if the joystick is installed where it is rotated a quarter turn
 * counterclockwise, like this:
 * 
 *          R
 *       U--|--D
 *          L
 * 
 * the value would be 90 degrees.
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

/**
 * @brief Reads the raw analog joystick values. 
 * 
 * This function reads the raw analog joystick values from the configured pins.
 * 
 * @param rawx Pointer to store the raw x-axis value.
 * @param rawy Pointer to store the raw y-axis value.
 */
void read_vikstik_raw(int16_t* rawx, int16_t* rawy) {
    *rawx = analogReadPin(VIK_GPIO_1);
    *rawy = analogReadPin(VIK_GPIO_2);
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
void apply_scaling_and_deadzone(int16_t rawx, int16_t rawy, int8_t* outx, int8_t* outy) {
    // Convert to signed values centered at 0
    int16_t x = rawx - vikstik_calibration.x_neutral;
    int16_t y = rawy - vikstik_calibration.y_neutral;

    // Calculate distance from center
    float distance = sqrt(x*x + y*y);

    // Filter for values outside of the deadzone
    if (distance < vikstik_calibration.deadzone_inner) {
        *outx = 0;
        *outy = 0;
        return;
    }

    // Calculate the scaling factor
    float scale = ((float)(JOYSTICK_MAX_OUT)) / (JOYSTICK_MAX_RAW / 2);
    
    // Apply the scaling to x and y while preserving direction
    float scaled_x = x * scale;
    float scaled_y = y * scale;

    // Clamp to output range
    *outx = clamp(scaled_x, JOYSTICK_MIN_OUT, JOYSTICK_MAX_OUT);
    *outy = clamp(scaled_y, JOYSTICK_MIN_OUT, JOYSTICK_MAX_OUT);
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
        case LEFT: // rotate 270 degrees counterclockwise
            *outx = y;
            *outy = -x;
            break;
        case DOWN: // rotate 180 degrees counterclockwise
            *outx = -x;
            *outy = -y;
            break;
        case RIGHT: // rotate 90 degrees counterclockwise
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

    int16_t ideal_neutral = (JOYSTICK_MIN_RAW + JOYSTICK_MAX_RAW) / 2;
    int16_t x_drift = abs(stick->x_neutral - ideal_neutral);
    int16_t y_drift = abs(stick->y_neutral - ideal_neutral);
    int16_t min_deadzone_inner = MAX_VAL(x_drift, y_drift);
    stick->deadzone_inner = MAX_VAL(INNER_DEADZONE, min_deadzone_inner);
    stick->deadzone_outer = OUTER_DEADZONE;
}

/**
 * @brief Get the raw (unaltered, un-rotated) quadrant based on joystick position.
 *
 * This function determines the quadrant based on the joystick position without any rotation.
 * It returns the quadrant number (0-3) based on the angle of the joystick.
 * @return The quadrant number (0-3), or -1 if the joystick is at its neutral position.
 */
static void calculate_raw_quadrant(int16_t rawx, int16_t rawy, int8_t* out_quadrant, int16_t* out_angle) {
    int8_t scalex = 0, scaley = 0;
    apply_scaling_and_deadzone(rawx, rawy, &scalex, &scaley);

    // if the joystick is neutral, bail
    if (scalex == 0 && scaley == 0) {
        *out_quadrant = -1;
        *out_angle = -1;
        return;
    }

    // Some trig sauce to determine the angle for calculations
    // where we may need a more precise/exact rotation, e.g.,
    // when we need something other than arrow keys or "wasd"
    // directions:
    float angle = atan2(scaley, scalex) * (180.0 / M_PI);
    angle = angle < 0 ? angle + 360 : angle;
    *out_angle = (int16_t)angle;

    // For more simple or discrete translation, determine quadrant
    // based on 45-degree sectors centered on cardinal directions:
    if (angle >= 67 && angle < 113) {
        *out_quadrant = 3; // installed with "left" up
    } else if (angle >= 157 && angle < 203) {
        *out_quadrant = 2; // installed with "bottom" up
    } else if (angle >= 247 && angle < 293) {
        *out_quadrant = 1; // installed with "right" up
    } else if ((angle > 337) || (angle < 23)) {
        *out_quadrant = 0; // installed with "top" up
    } else {
        *out_quadrant = -1;
    }
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
    const char* vikstik_up_orientation_names[] = {
        "UP",
        "RIGHT",
        "DOWN",
        "LEFT"
    };
    int16_t rawx = 0, rawy = 0;
    read_vikstik_raw(&rawx, &rawy);

    int8_t scalex = rawx, scaley = rawy;
    apply_scaling_and_deadzone(rawx, rawy, &scalex, &scaley);

    // Apply rotation
    int8_t rotatex = scalex, rotatey = scaley;
    handle_rotation(scalex, scaley, &rotatex, &rotatey);
    
    // Directly assign the scaled values to the output
    *outx = clamp(rotatex, JOYSTICK_MIN_OUT, JOYSTICK_MAX_OUT);
    *outy = clamp(rotatey, JOYSTICK_MIN_OUT, JOYSTICK_MAX_OUT);

    // Debuggery!
    static uint32_t last_print_time = 0;
    uint32_t current_time = timer_read32();
    if (current_time - last_print_time > 1000) {  // 1000 milliseconds = 1 second
        last_print_time = current_time;
        uprintf("Up orientation: %s\n", vikstik_up_orientation_names[vikstik_config.up_orientation]);
        uprintf("Raw joystick values:     x=%d, y=%d\n", rawx, rawy);
        uprintf("Scaled joystick values:  x=%d, y=%d\n", scalex, scaley);
        uprintf("Rotated joystick values: x=%d, y=%d\n", rotatex, rotatey);
        uprintf("Clamped joystick values: x=%d, y=%d\n", *outx, *outy);
    }
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
    int16_t rawx = 0, rawy = 0;
    read_vikstik_raw(&rawx, &rawy);
    calculate_raw_quadrant(rawx, rawy, out_quadrant, out_angle);
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
                             vikstik_config.up_orientation == UP &&
                             vikstik_config.up_angle == 0);
    // Check if any field has an invalid value
    bool is_invalid = (vikstik_config.mode >= VIKSTIK_SM_END ||
                       vikstik_config.up_orientation < UP || vikstik_config.up_orientation >= ORIENTATION_COUNT || 
                       vikstik_config.up_angle < 0 || vikstik_config.up_angle > 360);
    if (is_uninitialized || is_invalid || true) {
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
    vikstik_config = VIKSTIK_CONFIG_DEFAULT;
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
