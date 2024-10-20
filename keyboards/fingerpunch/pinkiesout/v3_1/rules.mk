# MCU name
MCU = RP2040

# Bootloader selection
BOOTLOADER = rp2040

# LTO must be disabled for RP2040 builds
LTO_ENABLE = no

# Build Options
#   change yes to no to disable
#
BOOTMAGIC_ENABLE = no       # Virtual DIP switch configuration
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = yes        # Console for debug
COMMAND_ENABLE = no        # Commands for debug and configuration
# Do not enable SLEEP_LED_ENABLE. it uses the same timer as BACKLIGHT_ENABLE
SLEEP_LED_ENABLE = no       # Breathing sleep LED during USB suspend
# if this doesn't work, see here: https://github.com/tmk/tmk_keyboard/wiki/FAQ#nkro-doesnt-work
NKRO_ENABLE = no            # USB Nkey Rollover
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
MOUSEKEY_ENABLE = yes

# Either do RGBLIGHT_ENABLE or RGB_MATRIX_ENABLE and RGB_MATRIX_DRIVER
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = no
RGB_MATRIX_DRIVER = ws2812

MIDI_ENABLE = no            # MIDI support
UNICODE_ENABLE = no         # Unicode
BLUETOOTH_ENABLE = no       # Enable Bluetooth with the Adafruit EZ-Key HID
AUDIO_ENABLE = yes          # Audio output on port C6
FAUXCLICKY_ENABLE = no      # Use buzzer to emulate clicky switches
ENCODER_ENABLE = no
# EXTRAFLAGS     += -flto     # macros disabled, if you need the extra space
MOUSEKEY_ENABLE = no

SRC += keyboards/fingerpunch/src/fp_matrix_74hc595_spi.c
QUANTUM_LIB_SRC += spi_master.c
CUSTOM_MATRIX = lite

AUDIO_ENABLE ?= no
AUDIO_DRIVER = pwm_hardware

HAPTIC_ENABLE ?= no
HAPTIC_DRIVER = drv2605l

# PIO serial/WS2812 drivers must be used on RP2040
SERIAL_DRIVER = vendor
WS2812_DRIVER = vendor

VIK_ENABLE = yes

OLED_ENABLE = yes
OLED_TRANSPORT = i2c

# For the analog mini joystick
# Check to see that VIK is enabled, and if either VIKSTIK_ENABLE or VIKSTIK_LITE_ENABLE is enabled
ifeq ($(and $(filter yes,$(VIK_ENABLE)),$(or $(filter yes,$(VIKSTIK_ENABLE)),$(filter yes,$(VIKSTIK_LITE_ENABLE)))),yes)
    JOYSTICK_ENABLE = yes
    ANALOG_DRIVER_REQUIRED = yes

    # Allow user to specify joystick profile, or default to 10-bit symmetric 8-bit
    JOYSTICK_PROFILE ?= JS_10BIT_SYM8BIT
    OPT_DEFS += -DJOYSTICK_PROFILE=$(JOYSTICK_PROFILE)

    # Include common VikStik code
    SRC += vikstik.c

    # Determine which version to include
    ifeq ($(strip $(VIKSTIK_LITE_ENABLE)), yes)
        VIKSTIK_LITE_ENABLE = yes
        OPT_DEFS += -DVIKSTIK_LITE_ENABLE
    else
        VIKSTIK_ENABLE = yes
        OPT_DEFS += -DVIKSTIK_ENABLE
    endif
endif

include keyboards/fingerpunch/src/rules.mk
