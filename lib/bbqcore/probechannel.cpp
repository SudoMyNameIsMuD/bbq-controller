#include "probechannel.h"

#include <cmath>

ProbeChannel::ProbeChannel(const ProbeChannelConfig& config) : m_config(config) {
}

void ProbeChannel::submitReading(float celsius, uint32_t nowMs) {
    if (!std::isfinite(celsius)) {
        markFault(ProbeFault::Invalid);
        return;
    }

    if (celsius < m_config.minValidCelsius || celsius > m_config.maxValidCelsius) {
        markFault(ProbeFault::OutOfRange);
        return;
    }

    m_celsius = celsius;
    m_hasReading = true;

    if (m_consecutiveHealthy == 0) {
        m_recoveryStartMs = nowMs;
    }

    m_lastHealthyMs = nowMs;
    m_fault = ProbeFault::None;

    if (m_consecutiveHealthy < UINT16_MAX) {
        m_consecutiveHealthy++;
    }
}

void ProbeChannel::submitElectricalFault(ProbeFault fault, uint32_t nowMs) {
    (void)nowMs;
    markFault(fault == ProbeFault::Short ? ProbeFault::Short : ProbeFault::Open);
}

void ProbeChannel::update(uint32_t nowMs) {
    if (m_fault == ProbeFault::None &&
            (nowMs - m_lastHealthyMs) > m_config.staleTimeoutMs) {
        markFault(ProbeFault::Stale);
    }
}

bool ProbeChannel::healthy() const {
    return m_fault == ProbeFault::None;
}

bool ProbeChannel::stableHealthy() const {
    return healthy() &&
           m_consecutiveHealthy >= m_config.recoveryHealthyCount &&
           (m_lastHealthyMs - m_recoveryStartMs) >= m_config.recoveryIntervalMs;
}

float ProbeChannel::celsius() const {
    return m_celsius;
}

ProbeFault ProbeChannel::fault() const {
    return m_fault;
}

uint32_t ProbeChannel::ageMs(uint32_t nowMs) const {
    if (!m_hasReading) {
        return UINT32_MAX;
    }

    return nowMs - m_lastHealthyMs;
}

bool ProbeChannel::hasReading() const {
    return m_hasReading;
}

void ProbeChannel::markFault(ProbeFault fault) {
    m_fault = fault;
    m_consecutiveHealthy = 0;
}
