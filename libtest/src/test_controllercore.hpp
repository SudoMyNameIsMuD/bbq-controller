#pragma once

#include "catch2/catch.hpp"

#include <controllercore.h>
#include <persistedstate.h>
#include <probechannel.h>

namespace {

struct FixedAlgorithm : public ControlAlgorithm {
    float duty = 50.0f;
    int resets = 0;
    float requestedDuty(float, float, uint32_t) override {
        return duty;
    }
    void reset() override {
        resets++;
    }
};

ProbeChannelConfig testProbeConfig() {
    ProbeChannelConfig cfg;
    cfg.staleTimeoutMs = 5000;
    cfg.recoveryHealthyCount = 3;
    cfg.recoveryIntervalMs = 2000;
    return cfg;
}

ControllerConfig testControllerConfig() {
    ControllerConfig cfg;
    cfg.minTargetCelsius = 50.0f;
    cfg.maxTargetCelsius = 300.0f;
    cfg.overTempTripCelsius = 350.0f;
    cfg.overTempClearMarginCelsius = 25.0f;
    cfg.maxDuty = 100.0f;
    cfg.minRunDuty = 10.0f;
    return cfg;
}

// Feed enough healthy pit samples to satisfy the recovery debounce.
void feedHealthyPit(ProbeChannel& pit, ControllerCore& core, ControlAlgorithm& algo,
                    float celsius, uint32_t startMs, uint32_t count = 4) {
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t now = startMs + i * 1000;
        pit.submitReading(celsius, now);
        core.update(now, algo);
    }
}

} // namespace

TEST_CASE("ControllerCore startup and basic modes", "[controllercore]") {
    ProbeChannel pit(testProbeConfig());
    ControllerCore core(testControllerConfig(), pit);
    FixedAlgorithm algo;

    SECTION("no output before begin") {
        core.update(1000, algo);
        REQUIRE(core.effectiveDuty() == 0.0f);
        REQUIRE_FALSE(core.requestAutomatic());
    }

    SECTION("starting keeps output off until pit is stably healthy") {
        core.begin(false, RequestedMode::Automatic, 120.0f);
        pit.submitReading(100.0f, 1000);
        core.update(1000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::Starting);
        REQUIRE(core.effectiveDuty() == 0.0f);

        feedHealthyPit(pit, core, algo, 100.0f, 2000);
        REQUIRE(core.effectiveState() == EffectiveState::Automatic);
        REQUIRE(core.effectiveDuty() == Approx(50.0f));
    }

    SECTION("restored manual mode downgrades to off") {
        core.begin(false, RequestedMode::Manual, 120.0f);
        feedHealthyPit(pit, core, algo, 100.0f, 1000);
        REQUIRE(core.requestedMode() == RequestedMode::Off);
        REQUIRE(core.effectiveDuty() == 0.0f);
    }

    SECTION("invalid restored target cancels restored cook") {
        core.begin(false, RequestedMode::Automatic, 10000.0f);
        REQUIRE(core.requestedMode() == RequestedMode::Off);
    }

    SECTION("target validation") {
        core.begin(false, RequestedMode::Off, 120.0f);
        REQUIRE_FALSE(core.setTarget(49.0f));
        REQUIRE_FALSE(core.setTarget(301.0f));
        REQUIRE_FALSE(core.setTarget(NAN));
        REQUIRE(core.setTarget(110.0f));
        REQUIRE(core.targetCelsius() == Approx(110.0f));
    }

    SECTION("manual duty validation and clamping to min run duty") {
        core.begin(false, RequestedMode::Off, 120.0f);
        feedHealthyPit(pit, core, algo, 100.0f, 1000);
        REQUIRE_FALSE(core.requestManual(-1.0f));
        REQUIRE_FALSE(core.requestManual(101.0f));
        REQUIRE(core.requestManual(5.0f));
        core.update(6000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::Manual);
        REQUIRE(core.effectiveDuty() == Approx(10.0f)); // raised to minRunDuty
        REQUIRE(core.requestManual(0.0f));
        core.update(7000, algo);
        REQUIRE(core.effectiveDuty() == 0.0f); // zero stays zero
    }
}

TEST_CASE("ControllerCore pit fault inhibition and recovery", "[controllercore]") {
    ProbeChannel pit(testProbeConfig());
    ControllerCore core(testControllerConfig(), pit);
    FixedAlgorithm algo;

    core.begin(false, RequestedMode::Automatic, 120.0f);
    feedHealthyPit(pit, core, algo, 100.0f, 1000);
    REQUIRE(core.effectiveState() == EffectiveState::Automatic);

    SECTION("pit fault turns blower off immediately, cook retained") {
        pit.submitElectricalFault(ProbeFault::Open, 5000);
        core.update(5000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::PitInhibited);
        REQUIRE(core.effectiveDuty() == 0.0f);
        REQUIRE(core.requestedMode() == RequestedMode::Automatic);
        REQUIRE(core.targetCelsius() == Approx(120.0f));

        // One good sample is not enough to resume.
        pit.submitReading(100.0f, 6000);
        core.update(6000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::PitInhibited);
        REQUIRE(core.effectiveDuty() == 0.0f);

        // Consecutive fresh readings over the recovery interval resume automatically.
        feedHealthyPit(pit, core, algo, 100.0f, 7000);
        REQUIRE(core.effectiveState() == EffectiveState::Automatic);
        REQUIRE(core.effectiveDuty() == Approx(50.0f));
    }

    SECTION("stale pit inhibits output") {
        core.update(20000, algo); // way past staleTimeout
        REQUIRE(core.effectiveState() == EffectiveState::PitInhibited);
        REQUIRE(core.effectiveDuty() == 0.0f);
    }

    SECTION("pit interlock applies to manual override too") {
        REQUIRE(core.requestManual(80.0f));
        pit.submitElectricalFault(ProbeFault::Open, 5000);
        core.update(5000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::PitInhibited);
        REQUIRE(core.effectiveDuty() == 0.0f);
    }
}

TEST_CASE("ControllerCore over-temperature latch", "[controllercore]") {
    ProbeChannel pit(testProbeConfig());
    ControllerCore core(testControllerConfig(), pit);
    FixedAlgorithm algo;

    core.begin(false, RequestedMode::Automatic, 120.0f);
    feedHealthyPit(pit, core, algo, 100.0f, 1000);

    pit.submitReading(360.0f, 5000); // above trip
    core.update(5000, algo);
    REQUIRE(core.latchedFault());
    REQUIRE(core.effectiveState() == EffectiveState::LatchedFault);
    REQUIRE(core.effectiveDuty() == 0.0f);

    SECTION("latched fault blocks mode requests and persists across begin") {
        REQUIRE_FALSE(core.requestAutomatic());
        REQUIRE_FALSE(core.requestManual(20.0f));

        // Simulated reboot with persisted fault.
        ProbeChannel pit2(testProbeConfig());
        ControllerCore core2(testControllerConfig(), pit2);
        core2.begin(true, RequestedMode::Automatic, 120.0f);
        feedHealthyPit(pit2, core2, algo, 100.0f, 1000);
        REQUIRE(core2.effectiveState() == EffectiveState::LatchedFault);
        REQUIRE(core2.effectiveDuty() == 0.0f);
    }

    SECTION("clear requires stable healthy pit below trip minus margin") {
        // Still hot: cannot clear.
        feedHealthyPit(pit, core, algo, 340.0f, 6000);
        REQUIRE_FALSE(core.clearLatchedFault());

        // Cooled and stable: clear succeeds, automatic resumes.
        feedHealthyPit(pit, core, algo, 200.0f, 20000);
        REQUIRE(core.clearLatchedFault());
        REQUIRE(core.requestAutomatic());
        core.update(25000, algo);
        pit.submitReading(200.0f, 26000);
        core.update(26000, algo);
        REQUIRE(core.effectiveState() == EffectiveState::Automatic);
    }
}

TEST_CASE("ControllerCore output clamping", "[controllercore]") {
    ProbeChannel pit(testProbeConfig());
    ControllerCore core(testControllerConfig(), pit);
    FixedAlgorithm algo;
    core.begin(false, RequestedMode::Automatic, 120.0f);
    feedHealthyPit(pit, core, algo, 100.0f, 1000);

    algo.duty = 1000.0f;
    pit.submitReading(100.0f, 5000);
    core.update(5000, algo);
    REQUIRE(core.effectiveDuty() == Approx(100.0f));

    algo.duty = -50.0f;
    pit.submitReading(100.0f, 6000);
    core.update(6000, algo);
    REQUIRE(core.effectiveDuty() == 0.0f);

    algo.duty = NAN;
    pit.submitReading(100.0f, 7000);
    core.update(7000, algo);
    REQUIRE(core.effectiveDuty() == 0.0f);
}

TEST_CASE("PersistedCookState versioning and corruption", "[persistedstate]") {
    PersistedCookState state;
    state.requestedMode = static_cast<uint8_t>(RequestedMode::Automatic);
    state.targetCelsius = 120.0f;
    state.seal();
    REQUIRE(state.valid());
    REQUIRE(state.mode() == RequestedMode::Automatic);

    SECTION("corrupted payload is rejected") {
        state.targetCelsius = 130.0f; // not resealed
        REQUIRE_FALSE(state.valid());
    }

    SECTION("wrong version is rejected") {
        state.version = 99;
        state.crc = state.computeCrc();
        REQUIRE_FALSE(state.valid());
    }

    SECTION("out-of-range mode byte maps to Off") {
        state.requestedMode = 42;
        REQUIRE(state.mode() == RequestedMode::Off);
    }
}
