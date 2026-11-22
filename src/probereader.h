#pragma once

// Probe acquisition: reads the three probe ADC inputs, detects electrical
// open/short conditions and converts to Celsius.
//
// !! THE CONVERSION BELOW USES PLACEHOLDER NTC COEFFICIENTS !!
// The real probes must be characterized (resistance vs. verified reference
// temperatures) and these coefficients, the divider resistor value and the
// open/short thresholds replaced with measured values before the readings
// can be trusted. Until then readings are only suitable for bench bring-up.

#include <cstdint>

#include <probechannel.h>

struct ProbeHardwareConfig {
    uint8_t pin;
    // Voltage divider: 3.3V -- seriesResistor -- [ADC node] -- NTC -- GND.
    float seriesResistorOhms = 100000.0f;
    // PLACEHOLDER Steinhart-Hart beta model values; replace after probe
    // characterization.
    float nominalResistanceOhms = 100000.0f;
    float nominalTemperatureC = 25.0f;
    float betaCoefficient = 3950.0f;
    // Raw 12-bit ADC thresholds for electrical fault detection.
    uint16_t openCircuitRawAbove = 4080;  // probe missing -> pulled to rail
    uint16_t shortCircuitRawBelow = 15;   // probe shorted -> near ground
};

class ProbeReader {
public:
    ProbeReader(const ProbeHardwareConfig& config, ProbeChannel& channel);

    void begin();

    // Take one sample and feed the channel with a reading or a fault.
    void sample(uint32_t nowMs);

private:
    ProbeHardwareConfig m_config;
    ProbeChannel& m_channel;
};
