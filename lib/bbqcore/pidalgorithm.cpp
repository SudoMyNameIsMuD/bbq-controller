#include "pidalgorithm.h"

#include <cmath>

PidControlAlgorithm::PidControlAlgorithm(const PidAlgorithmConfig& config) : m_config(config) {
}

float PidControlAlgorithm::requestedDuty(float pitCelsius, float targetCelsius, uint32_t nowMs) {
    const float error = targetCelsius - pitCelsius;

    if (m_hasPrevious) {
        // Conditional integration with anti-windup clamping.
        if (std::fabs(error) <= m_config.integralBandCelsius) {
            const float dtSeconds = (nowMs - m_lastUpdateMs) / 1000.0f;
            m_integral += m_config.integralGain * error * dtSeconds;

            if (m_integral > m_config.maxDuty) {
                m_integral = m_config.maxDuty;
            } else if (m_integral < 0.0f) {
                m_integral = 0.0f;
            }
        }

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
        m_integral = 0.0f;
    }

    m_lastUpdateMs = nowMs;

    const float duty = m_config.proportionalGain * error
                       + m_integral
                       - m_config.derivativeGain * m_ratePerSecond;

    return duty; // ControllerCore clamps to the configured output limits
}

void PidControlAlgorithm::reset() {
    m_hasPrevious = false;
    m_ratePerSecond = 0.0f;
    m_integral = 0.0f;
}
