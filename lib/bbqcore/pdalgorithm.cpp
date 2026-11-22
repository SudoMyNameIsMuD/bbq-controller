#include "pdalgorithm.h"

PdControlAlgorithm::PdControlAlgorithm(const PdAlgorithmConfig& config) : m_config(config) {
}

float PdControlAlgorithm::requestedDuty(float pitCelsius, float targetCelsius, uint32_t nowMs) {
    if (m_hasPrevious) {
        const uint32_t elapsed = nowMs - m_previousMs;

        if (elapsed >= m_config.derivativeWindowMs) {
            m_ratePerSecond = (pitCelsius - m_previousCelsius) * 1000.0f / elapsed;
            m_previousCelsius = pitCelsius;
            m_previousMs = nowMs;
        }
    } else {
        m_hasPrevious = true;
        m_previousCelsius = pitCelsius;
        m_previousMs = nowMs;
        m_ratePerSecond = 0.0f;
    }

    const float error = targetCelsius - pitCelsius;
    const float duty = m_config.proportionalGain * error
                       - m_config.derivativeGain * m_ratePerSecond;

    return duty; // ControllerCore clamps to the configured output limits
}

void PdControlAlgorithm::reset() {
    m_hasPrevious = false;
    m_ratePerSecond = 0.0f;
}
