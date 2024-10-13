#include QMK_KEYBOARD_H

// Defines names for use in layer keycodes and the keymap
enum layer_names {
    _QWERTY,
    _COLEMAK,
    _LOWER,
    _RAISE,
    _ADJUST
};

enum custom_keycodes {
    QWERTY = SAFE_RANGE,
    LOWER,
    RAISE,
    ADJUST
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {


[_QWERTY] = LAYOUT_pinkiesout(
    KC_ESC,    KC_1,      KC_2,      KC_3,      KC_4,      KC_5,      KC_LPRN,                      KC_RPRN,   KC_6,      KC_7,      KC_8,      KC_9,      KC_0,      KC_BSPC,
    KC_TAB,    KC_Q,      KC_W,      KC_E,      KC_R,      KC_T,      KC_LBRC,                      KC_RBRC,   KC_Y,      KC_U,      KC_I,      KC_O,      KC_P,      KC_BSLS,
    KC_CAPS,   KC_A,      KC_S,      KC_D,      KC_F,      KC_G,      KC_LCBR,                      KC_RCBR,   KC_H,      KC_J,      KC_K,      KC_L,      KC_SCLN,   KC_QUOT,
    KC_LSFT,   KC_Z,      KC_X,      KC_C,      KC_V,      KC_B,      ADJUST,                       ADJUST,    KC_N,      KC_M,      KC_COMM,   KC_DOT,    KC_SLSH,   KC_ENT,
                          KC_LCTL,   KC_LGUI,   LOWER,     KC_SPC,    KC_LALT,   KC_MUTE, KC_MUTE,  KC_RALT,   KC_SPC,    RAISE,     KC_RGUI,   KC_RCTL
),


[_COLEMAK] = LAYOUT_pinkiesout(
    KC_ESC,    KC_1,      KC_2,      KC_3,      KC_4,      KC_5,      KC_LPRN,                      KC_RPRN,   KC_6,      KC_7,      KC_8,      KC_9,      KC_0,      KC_BSPC,
    KC_TAB,    KC_Q,      KC_W,      KC_F,      KC_P,      KC_B,      KC_LBRC,                      KC_RBRC,   KC_J,      KC_L,      KC_U,      KC_Y,      KC_SCLN,   KC_BSLS,
    KC_CAPS,   KC_A,      KC_R,      KC_S,      KC_T,      KC_G,      KC_LCBR,                      KC_RCBR,   KC_M,      KC_N,      KC_E,      KC_I,      KC_O,      KC_QUOT,
    KC_LSFT,   KC_Z,      KC_X,      KC_C,      KC_D,      KC_V,      ADJUST,                       ADJUST,    KC_K,      KC_H,      KC_COMM,   KC_DOT,    KC_SLSH,   KC_ENT,
                          KC_LCTL,   KC_LGUI,   LOWER,     KC_SPC,    KC_LALT,  _______,  _______,  KC_RALT,   KC_SPC,    RAISE,     KC_RGUI,   KC_RCTL
),


[_LOWER] = LAYOUT_pinkiesout(
    KC_GRV,        _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       KC_MINS,       KC_EQL,        _______,
    _______,       _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
    _______,       _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
    _______,       _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
                                  _______,       _______,       _______,       _______,       _______,  _______,  _______,  _______,       _______,       _______,       _______,       _______
),

[_RAISE] = LAYOUT_pinkiesout(
    KC_GRV,        _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       KC_MINS,       KC_EQL,        _______,
    _______,       _______,       KC_PGUP,       KC_UP,         KC_PGDN,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
    _______,       KC_HOME,       KC_LEFT,       KC_DOWN,       KC_RGHT,       KC_END,        _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
    _______,       _______,       _______,       _______,       _______,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
                                  _______,       _______,       _______,       _______,       _______,  _______,  _______,  _______,       _______,       _______,       _______,       _______
),

[_ADJUST] =  LAYOUT_pinkiesout(
    QK_BOOT,         KC_F1,         KC_F2,         KC_F3,         KC_F4,         KC_F5,         _______,                      _______,       KC_F6,         KC_F7,         KC_F8,         KC_F9,         KC_F10,        _______,
    _______,       RGB_TOG,       RGB_RMOD,      RGB_MOD,       _______,       TG(_QWERTY),   _______,                      _______,       _______,       _______,       _______,       KC_F11,        KC_F12,        _______,
    _______,       RGB_SPI,       RGB_HUI,       RGB_SAI,       RGB_VAI,       TG(_COLEMAK),  _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
    _______,       RGB_SPD,       RGB_HUD,       RGB_SAD,       RGB_VAD,       _______,       _______,                      _______,       _______,       _______,       _______,       _______,       _______,       _______,
                                  _______,       _______,       _______,       _______,       _______,  _______,  _______,  _______,       _______,       _______,       _______,       _______
)
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LOWER:
            if (record->event.pressed) {
                layer_on(_LOWER);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_LOWER);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case RAISE:
            if (record->event.pressed) {
                layer_on(_RAISE);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_RAISE);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case ADJUST:
            if (record->event.pressed) {
                layer_on(_ADJUST);
            } else {
                layer_off(_ADJUST);
            }
            return false;
            break;
        default:
            break;
    }

    return true;
}

void process_joystick(void) {
    int16_t x = analogRead(VIK_GPIO_1);
    int16_t y = analogRead(VIK_GPIO_2);

    // Define thresholds for movement detection
    const int THRESHOLD = 100; // Adjust as necessary
    
    // Map X-axis
    if (x < 512 - THRESHOLD) { // Left
        tap_code(KC_LEFT);
    } else if (x > 512 + THRESHOLD) { // Right
        tap_code(KC_RIGHT);
    }

    // Map Y-axis
    if (y < 512 - THRESHOLD) { // Down
        tap_code(KC_DOWN);
    } else if (y > 512 + THRESHOLD) { // Up
        tap_code(KC_UP);
    }
}

void process_joystick_by_quadrant(void) {
    // Define constants
    const int16_t CENTER = 512;     // Whatever is the neutral position
    const int16_t THRESHOLD = 100;  // Some portion of the max value
    const float PI = 3.14159;       // Our favorite trig constant
    const float PI_OVER_4 = PI / 4; // For determining octant

    // Friendly names for the octant numbers
    const JS_OCTANT_RIGHT = 0;
    const JS_OCTANT_UP_RIGHT = 1;
    const JS_OCTANT_UP = 2;
    const JS_OCTANT_UP_LEFT = 3;
    const JS_OCTANT_LEFT = 4;
    const JS_OCTANT_DOWN_LEFT = 5;
    const JS_OCTANT_DOWN = 6;
    const JS_OCTANT_DOWN_RIGHT = 7;

    // Read joystick values and calculate offsets
    int16_t x = analogRead(VIK_GPIO_1) - CENTER;
    int16_t y = analogRead(VIK_GPIO_2) - CENTER;
    
    float magnitude = sqrt(x*x + y*y);
    
    if (magnitude > THRESHOLD) {
        float angle = atan2(y, x);
        if (angle < 0) angle += 2 * PI;
        
        uint8_t octant = (uint8_t)(angle / PI_OVER_4);
        
        switch (octant) {
            case JS_OCTANT_RIGHT: tap_code(KC_RIGHT); break;
            case JS_OCTANT_UP_RIGHT: tap_code(KC_RIGHT); tap_code(KC_UP); break;
            case JS_OCTANT_UP: tap_code(KC_UP); break;
            case JS_OCTANT_UP_LEFT: tap_code(KC_UP); tap_code(KC_LEFT); break;
            case JS_OCTANT_LEFT: tap_code(KC_LEFT); break;
            case JS_OCTANT_DOWN_LEFT: tap_code(KC_LEFT); tap_code(KC_DOWN); break;
            case JS_OCTANT_DOWN: tap_code(KC_DOWN); break;
            case JS_OCTANT_DOWN_RIGHT: tap_code(KC_DOWN); tap_code(KC_RIGHT); break;
        }
    }
}

void matrix_scan_user(void) {
    process_joystick();
}
