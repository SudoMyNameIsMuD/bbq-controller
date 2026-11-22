#pragma once

// ============================================================================
// Board configuration for an ESP32-S3 DevKitC-1-class board.
//
// !! ALL PIN ASSIGNMENTS ARE PROVISIONAL AND REQUIRE HARDWARE VERIFICATION !!
// Verify against the actual board's schematic that each pin is exposed, not a
// strapping/boot/flash/PSRAM/USB pin on your module, and that the ADC pins
// suit the final probe measurement circuit. See the plan's hardware gates.
// ============================================================================

#include <cstdint>

// Probe ADC inputs (ADC1 channels on a DevKitC-1: GPIO1..GPIO10).
constexpr uint8_t PIN_PROBE_PIT = 4;   // GPIO4 / ADC1_CH3
constexpr uint8_t PIN_PROBE_FOOD1 = 5; // GPIO5 / ADC1_CH4
constexpr uint8_t PIN_PROBE_FOOD2 = 6; // GPIO6 / ADC1_CH5

// Blower PWM output. Must drive a blower driver stage that is electrically
// default-off; never connect a blower directly to this GPIO.
constexpr uint8_t PIN_BLOWER_PWM = 16;
constexpr uint32_t BLOWER_PWM_FREQ_HZ = 25000; // adjust to the selected driver
constexpr uint8_t BLOWER_PWM_RESOLUTION_BITS = 10;

// HomeSpan status LED and control/reset button (BOOT button on DevKitC-1).
constexpr uint8_t PIN_STATUS_LED = 48; // on-board RGB LED data pin on many DevKitC-1s
constexpr uint8_t PIN_CONTROL_BUTTON = 0;

// Sampling cadence.
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
