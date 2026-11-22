#pragma once

// Blower PWM output using the Arduino-ESP32 3.x LEDC API. The GPIO only
// commands a driver stage; the driver hardware must itself default to OFF
// when this pin is low, floating, or the controller is in reset.

#include <Arduino.h>

#include "boardconfig.h"

class BlowerOutput {
public:
    void begin() {
        pinMode(PIN_BLOWER_PWM, OUTPUT);
        digitalWrite(PIN_BLOWER_PWM, LOW);
        ledcAttach(PIN_BLOWER_PWM, BLOWER_PWM_FREQ_HZ, BLOWER_PWM_RESOLUTION_BITS);
        ledcWrite(PIN_BLOWER_PWM, 0);
    }

    // duty in percent 0..100, already validated by the controller core.
    void apply(float dutyPercent) {
        const uint32_t maxCount = (1u << BLOWER_PWM_RESOLUTION_BITS) - 1;
        uint32_t counts = static_cast<uint32_t>(dutyPercent / 100.0f * maxCount + 0.5f);

        if (counts > maxCount) {
            counts = maxCount;
        }

        ledcWrite(PIN_BLOWER_PWM, counts);
    }
};
