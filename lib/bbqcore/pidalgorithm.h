#pragma once

#include <cstdint>

#include "controllercore.h"

struct PidAlgorithmConfig {
    // PLACEHOLDER gains - validated against the simulated grill
    // (libtest/src/test_controlsim.hpp); tune in supervised grill trials
    // before real use.
    float proportionalGain = 2.0f;        // % duty per degC of error
    float integralGain = 0.02f;           // % duty per degC-second of error
    float derivativeGain = 30.0f;         // % duty per degC/s of pit change
    uint32_t derivativeWindowMs = 5000;   // time base for the rate estimate
    // Integrate only when the error is within this band, so approaching the
    // target from a cold start does not wind up and overshoot.
    float integralBandCelsius = 20.0f;
    float maxDuty = 100.0f;               // anti-windup clamp for the I term
};

// Proportional-integral-derivative control algorithm.
//
// Chosen after simulating both this and the legacy fuzzy controller
// (lib/bbq/bbqfanonly.*) against the first-order grill model: the fuzzy
// controller converged with no steady-state error but overshot a cold start
// by ~36 degC; a plain PD had no overshoot but a ~10 degC steady-state
// offset. Conditional integral action combines both behaviors (see
// test_controlsim.hpp for the enforced acceptance criteria).
class PidControlAlgorithm : public ControlAlgorithm {
public:
    explicit PidControlAlgorithm(const PidAlgorithmConfig& config = PidAlgorithmConfig());

    float requestedDuty(float pitCelsius, float targetCelsius, uint32_t nowMs) override;
    void reset() override;

private:
    PidAlgorithmConfig m_config;
    bool m_hasPrevious = false;
    float m_previousCelsius = 0.0f;
    uint32_t m_previousMs = 0;
    uint32_t m_lastUpdateMs = 0;
    float m_ratePerSecond = 0.0f;
    float m_integral = 0.0f; // accumulated duty contribution, %
};
