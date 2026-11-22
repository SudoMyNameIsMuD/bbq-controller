#include "controllercore.h"

#include <cmath>

ControllerCore::ControllerCore(const ControllerConfig& config, ProbeChannel& pitProbe)
    : m_config(config), m_pitProbe(pitProbe), m_targetCelsius(config.minTargetCelsius) {
}

void ControllerCore::begin(bool persistedLatchedFault, RequestedMode restoredMode,
                           float restoredTargetCelsius) {
    m_latchedFault = persistedLatchedFault;

    // Manual override never survives a reboot.
    m_requestedMode = (restoredMode == RequestedMode::Automatic)
                      ? RequestedMode::Automatic : RequestedMode::Off;

    if (std::isfinite(restoredTargetCelsius) &&
            restoredTargetCelsius >= m_config.minTargetCelsius &&
            restoredTargetCelsius <= m_config.maxTargetCelsius) {
        m_targetCelsius = restoredTargetCelsius;
    } else if (m_requestedMode == RequestedMode::Automatic) {
        // Invalid restored configuration: do not resume the cook.
        m_requestedMode = RequestedMode::Off;
    }

    m_effectiveState = EffectiveState::Starting;
    m_effectiveDuty = 0.0f;
    m_requestedDuty = 0.0f;
    m_started = true;
    m_algorithmNeedsReset = true;
}

bool ControllerCore::setTarget(float celsius) {
    if (!std::isfinite(celsius) ||
            celsius < m_config.minTargetCelsius || celsius > m_config.maxTargetCelsius) {
        return false;
    }

    m_targetCelsius = celsius;
    return true;
}

bool ControllerCore::requestAutomatic() {
    if (!m_started || m_latchedFault) {
        return false;
    }

    if (m_requestedMode != RequestedMode::Automatic) {
        m_algorithmNeedsReset = true;
    }

    m_requestedMode = RequestedMode::Automatic;
    return true;
}

bool ControllerCore::requestManual(float duty) {
    if (!m_started || m_latchedFault) {
        return false;
    }

    if (!std::isfinite(duty) || duty < 0.0f || duty > m_config.maxDuty) {
        return false;
    }

    m_manualDuty = duty;
    m_requestedMode = RequestedMode::Manual;
    return true;
}

bool ControllerCore::requestOff() {
    m_requestedMode = RequestedMode::Off;
    return true;
}

bool ControllerCore::clearLatchedFault() {
    if (!m_latchedFault) {
        return true;
    }

    const float clearBelow = m_config.overTempTripCelsius - m_config.overTempClearMarginCelsius;

    if (m_pitProbe.stableHealthy() && m_pitProbe.celsius() < clearBelow) {
        m_latchedFault = false;
        return true;
    }

    return false;
}

void ControllerCore::update(uint32_t nowMs, ControlAlgorithm& algorithm) {
    if (!m_started) {
        m_effectiveDuty = 0.0f;
        return;
    }

    m_pitProbe.update(nowMs);

    // Over-temperature check uses the fresh pit sample.
    if (m_pitProbe.healthy() && m_pitProbe.celsius() >= m_config.overTempTripCelsius) {
        m_latchedFault = true;
    }

    // Safety precedence: latched fault, then pit health, then requested mode.
    if (m_latchedFault) {
        m_effectiveState = EffectiveState::LatchedFault;
        m_requestedDuty = 0.0f;
        m_effectiveDuty = 0.0f;
        m_algorithmNeedsReset = true;
        return;
    }

    if (m_requestedMode == RequestedMode::Off) {
        m_effectiveState = EffectiveState::Off;
        m_requestedDuty = 0.0f;
        m_effectiveDuty = 0.0f;
        m_algorithmNeedsReset = true;
        return;
    }

    // Automatic or Manual requested: pit health gates all blower operation.
    // Recovery (and leaving Starting) requires stable health, not a single
    // good sample.
    const bool active = (m_effectiveState == EffectiveState::Automatic ||
                         m_effectiveState == EffectiveState::Manual);
    const bool pitOk = active ? m_pitProbe.healthy() : m_pitProbe.stableHealthy();

    if (!pitOk) {
        m_effectiveState = (m_effectiveState == EffectiveState::Starting)
                           ? EffectiveState::Starting : EffectiveState::PitInhibited;
        m_requestedDuty = 0.0f;
        m_effectiveDuty = 0.0f;
        m_algorithmNeedsReset = true;
        return;
    }

    if (m_requestedMode == RequestedMode::Manual) {
        m_effectiveState = EffectiveState::Manual;
        m_requestedDuty = m_manualDuty;
        m_algorithmNeedsReset = true;
    } else {
        m_effectiveState = EffectiveState::Automatic;

        if (m_algorithmNeedsReset) {
            algorithm.reset();
            m_algorithmNeedsReset = false;
        }

        m_requestedDuty = algorithm.requestedDuty(m_pitProbe.celsius(), m_targetCelsius, nowMs);
    }

    m_effectiveDuty = clampDuty(m_requestedDuty);
}

ControllerSnapshot ControllerCore::snapshot() const {
    return ControllerSnapshot{
        m_requestedMode, m_effectiveState, m_targetCelsius,
        m_requestedDuty, m_effectiveDuty, m_latchedFault
    };
}

float ControllerCore::clampDuty(float duty) const {
    if (!std::isfinite(duty) || duty <= 0.0f) {
        return 0.0f;
    }

    if (duty > m_config.maxDuty) {
        return m_config.maxDuty;
    }

    if (duty < m_config.minRunDuty) {
        return m_config.minRunDuty;
    }

    return duty;
}
