#include "probereader.h"

#include <Arduino.h>
#include <cmath>

ProbeReader::ProbeReader(const ProbeHardwareConfig& config, ProbeChannel& channel)
    : m_config(config), m_channel(channel) {
}

void ProbeReader::begin() {
    pinMode(m_config.pin, INPUT);
    analogReadResolution(12);
}

void ProbeReader::sample(uint32_t nowMs) {
    const uint16_t raw = analogRead(m_config.pin);

    if (raw >= m_config.openCircuitRawAbove) {
        m_channel.submitElectricalFault(ProbeFault::Open, nowMs);
        return;
    }

    if (raw <= m_config.shortCircuitRawBelow) {
        m_channel.submitElectricalFault(ProbeFault::Short, nowMs);
        return;
    }

    // NTC to ground with series resistor to 3.3V: R_ntc = Rs * raw / (max - raw).
    const float maxCount = 4095.0f;
    const float resistance = m_config.seriesResistorOhms * raw / (maxCount - raw);

    // Beta-model conversion. PLACEHOLDER coefficients; see header warning.
    const float t0Kelvin = m_config.nominalTemperatureC + 273.15f;
    const float invT = 1.0f / t0Kelvin +
                       logf(resistance / m_config.nominalResistanceOhms) / m_config.betaCoefficient;
    const float celsius = 1.0f / invT - 273.15f;

    m_channel.submitReading(celsius, nowMs);
}
