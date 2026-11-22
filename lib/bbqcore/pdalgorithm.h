#pragma once

#include <cstdint>

#include "controllercore.h"

struct PdAlgorithmConfig {
    // PLACEHOLDER gains - tune against the simulated grill and then in
    // supervised grill trials before real use.
    float proportionalGain = 2.0f;        // % duty per degC of error
    float derivativeGain = 30.0f;         // % duty per degC/s of pit change
    uint32_t derivativeWindowMs = 5000;   // time base for the rate estimate
};

// Initial proportional-derivative control algorithm. The legacy fuzzy
// controller remains available in lib/bbq for comparison in simulation.
class PdControlAlgorithm : public ControlAlgorithm {
public:
    explicit PdControlAlgorithm(const PdAlgorithmConfig& config = PdAlgorithmConfig());

    float requestedDuty(float pitCelsius, float targetCelsius, uint32_t nowMs) override;
    void reset() override;

private:
    PdAlgorithmConfig m_config;
    bool m_hasPrevious = false;
    float m_previousCelsius = 0.0f;
    uint32_t m_previousMs = 0;
    float m_ratePerSecond = 0.0f;
};
