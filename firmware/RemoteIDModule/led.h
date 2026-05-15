#pragma once

#include <stdint.h>
#include "board_config.h"

#ifdef WS2812_LED_PIN
#include <Adafruit_NeoPixel.h>
#endif

class Led {
public:
    enum class LedState {
        INIT=0,
        PFST_FAIL,
        ARM_FAIL,
        ARM_OK
    };

    void set_state(LedState _state) {
        state = _state;
    }
    void pulse_feedback(uint16_t duration_ms = 180);
    void update(void);

private:
    void init(void);
    bool done_init;
    uint32_t last_led_trig_ms;
    uint32_t feedback_until_ms;
    LedState state;

#ifdef WS2812_LED_PIN
    uint32_t last_led_strip_ms;
    Adafruit_NeoPixel ledStrip{2, WS2812_LED_PIN, NEO_GRB + NEO_KHZ800}; //the BlueMark db210pro boards has two LEDs, therefore we need to use 2.
#endif
};

extern Led led;
