#pragma once

// Simple first-order kamado grill thermal model for host simulation tests.
// This is a coarse plant model for comparing control algorithms and checking
// gross stability; it does not prove safety or tuning on a real grill.

#include <cstdint>

struct GrillSimConfig {
    float ambientCelsius = 20.0f;
    // Natural-draft heating with the blower off, degC/s at low temperature.
    float naturalDraftHeat = 0.05f;
    // Additional heating at 100% blower duty, degC/s.
    float fanHeatGain = 0.5f;
    // Heat loss coefficient, 1/s (fraction of (T - ambient) lost per second).
    float coolingRate = 1.0f / 600.0f;
    // First-order probe/plenum lag time constant, seconds.
    float probeLagSeconds = 30.0f;
};

class GrillSim {
public:
    explicit GrillSim(const GrillSimConfig& config = GrillSimConfig())
        : m_config(config), m_pitCelsius(config.ambientCelsius),
          m_probeCelsius(config.ambientCelsius) {
    }

    // Advance the model by dtSeconds with the given blower duty (0..100).
    void step(float dutyPercent, float dtSeconds) {
        const float heat = m_config.naturalDraftHeat +
                           m_config.fanHeatGain * dutyPercent / 100.0f;
        const float loss = m_config.coolingRate * (m_pitCelsius - m_config.ambientCelsius);
        m_pitCelsius += (heat - loss) * dtSeconds;

        // Probe lags the pit.
        m_probeCelsius += (m_pitCelsius - m_probeCelsius) * dtSeconds / m_config.probeLagSeconds;
    }

    float pitCelsius() const {
        return m_pitCelsius;
    }

    // What the temperature probe reports.
    float probeCelsius() const {
        return m_probeCelsius;
    }

    void setPit(float celsius) {
        m_pitCelsius = celsius;
        m_probeCelsius = celsius;
    }

private:
    GrillSimConfig m_config;
    float m_pitCelsius;
    float m_probeCelsius;
};
