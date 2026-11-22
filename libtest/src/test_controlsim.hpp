#pragma once

// Control simulation acceptance tests (plan: "Control simulation" test row).
//
// The simulated grill is a coarse first-order model; passing here establishes
// gross stability and convergence, not real-grill safety or tuning.
//
// Algorithm selection record (2026-11): on this model, cold start to 120 degC
// over 4 simulated hours gave:
//   legacy fuzzy (lib/bbq) : ~36 degC overshoot, 0 steady-state error
//   plain PD               :   0 overshoot, ~10 degC steady-state offset
//   PID (shipped)          : ~3.6 degC overshoot, 0 steady-state error
// Acceptance criteria below are defined before tuning per the plan.

#include "catch2/catch.hpp"

#include <cmath>
#include <algorithm>

#include <pidalgorithm.h>

#include "grillsim.hpp"

namespace controlsim {

struct SimResult {
    float overshoot = 0.0f;        // max degC above target
    float meanAbsErrorLast30 = 0;  // mean |error| over the last 30 min
    float maxSwingLast30 = 0;      // max-min band over the last 30 min
};

inline SimResult simulate(PidControlAlgorithm& algorithm, GrillSim& sim,
                          float target, int seconds, uint32_t startMs = 0) {
    SimResult result;
    float errSum = 0.0f;
    int errCount = 0;
    float minT = 1e9f, maxT = -1e9f;

    for (int t = 0; t < seconds; t++) {
        const uint32_t nowMs = startMs + static_cast<uint32_t>(t) * 1000u;
        float duty = algorithm.requestedDuty(sim.probeCelsius(), target, nowMs);
        duty = std::min(100.0f, std::max(0.0f, duty));
        sim.step(duty, 1.0f);

        const float T = sim.probeCelsius();
        result.overshoot = std::max(result.overshoot, T - target);

        if (t >= seconds - 1800) {
            errSum += std::fabs(T - target);
            errCount++;
            minT = std::min(minT, T);
            maxT = std::max(maxT, T);
        }
    }

    result.meanAbsErrorLast30 = errSum / errCount;
    result.maxSwingLast30 = maxT - minT;
    return result;
}

} // namespace controlsim

TEST_CASE("PID converges from a cold start without excessive overshoot", "[controlsim]") {
    PidControlAlgorithm pid;
    GrillSim sim;
    const auto r = controlsim::simulate(pid, sim, 120.0f, 4 * 3600);

    CHECK(r.overshoot < 10.0f);
    CHECK(r.meanAbsErrorLast30 < 2.0f);
    CHECK(r.maxSwingLast30 < 5.0f);
}

TEST_CASE("PID follows target changes in both directions", "[controlsim]") {
    PidControlAlgorithm pid;
    GrillSim sim;
    controlsim::simulate(pid, sim, 120.0f, 2 * 3600);

    SECTION("raise target") {
        const auto r = controlsim::simulate(pid, sim, 150.0f, 2 * 3600, 2u * 3600u * 1000u);
        CHECK(r.overshoot < 10.0f);
        CHECK(r.meanAbsErrorLast30 < 2.0f);
    }

    SECTION("lower target (blower cannot cool; natural decay only)") {
        const auto r = controlsim::simulate(pid, sim, 100.0f, 3 * 3600, 2u * 3600u * 1000u);
        CHECK(r.meanAbsErrorLast30 < 2.0f);
        CHECK(r.maxSwingLast30 < 5.0f);
    }
}

TEST_CASE("PID recovers from a disturbance (lid open)", "[controlsim]") {
    PidControlAlgorithm pid;
    GrillSim sim;
    controlsim::simulate(pid, sim, 120.0f, 2 * 3600);

    // Lid open: sudden pit temperature drop.
    sim.setPit(sim.pitCelsius() - 40.0f);
    const auto r = controlsim::simulate(pid, sim, 120.0f, 3600, 2u * 3600u * 1000u);

    CHECK(r.overshoot < 10.0f);
    CHECK(r.meanAbsErrorLast30 < 2.0f);
}
