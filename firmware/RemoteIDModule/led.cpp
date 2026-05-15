#include <Arduino.h>
#include "led.h"
#include "board_config.h"

Led led;

void Led::init(void)
{
    if (done_init) {
        return;
    }
    done_init = true;
    feedback_until_ms = 0;
#ifdef PIN_STATUS_LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, STATUS_LED_OK);
#endif
#ifdef WS2812_LED_PIN
    pinMode(WS2812_LED_PIN, OUTPUT);
    ledStrip.begin();
#endif
}

void Led::pulse_feedback(uint16_t duration_ms)
{
    init();
    const uint32_t now_ms = millis();
    feedback_until_ms = now_ms + duration_ms;
#ifdef PIN_STATUS_LED
    digitalWrite(PIN_STATUS_LED, !STATUS_LED_OK);
#endif
#ifdef WS2812_LED_PIN
    ledStrip.clear();
    ledStrip.setPixelColor(0, ledStrip.Color(255, 0, 0));
    ledStrip.setPixelColor(1, ledStrip.Color(255, 0, 0));
    ledStrip.show();
#endif
}

void Led::update(void)
{
    init();

    const uint32_t now_ms = millis();

    if (feedback_until_ms != 0 && int32_t(feedback_until_ms - now_ms) > 0) {
        return;
    }
    feedback_until_ms = 0;

#ifdef PIN_STATUS_LED
    switch (state) {
    case LedState::ARM_OK: {
        digitalWrite(PIN_STATUS_LED, STATUS_LED_OK);
        last_led_trig_ms = now_ms;
        break;
    }

    default:
        if (now_ms - last_led_trig_ms > 100) {
            digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
            last_led_trig_ms = now_ms;
        }
        break;
    }
#endif

#ifdef WS2812_LED_PIN
    ledStrip.clear();

    switch (state) {
    case LedState::ARM_OK:
        ledStrip.setPixelColor(0, ledStrip.Color(0, 255, 0));
        ledStrip.setPixelColor(0, ledStrip.Color(1, 255, 0)); //for db210pro, set the second LED to have the same output (for now)
        break;

    default:
        ledStrip.setPixelColor(0, ledStrip.Color(255, 0, 0));
        ledStrip.setPixelColor(1, ledStrip.Color(255, 0, 0)); //for db210pro, set the second LED to have the same output (for now)
        break;
    }
    if (now_ms - last_led_strip_ms >= 200) {
        last_led_strip_ms = now_ms;
        ledStrip.show();
    }
#endif
}

