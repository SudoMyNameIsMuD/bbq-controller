#pragma once

#include "catch2/catch.hpp"

#include <probechannel.h>

TEST_CASE("ProbeChannel classifies readings and faults", "[probechannel]") {
    ProbeChannelConfig cfg;
    cfg.minValidCelsius = -20.0f;
    cfg.maxValidCelsius = 400.0f;
    cfg.staleTimeoutMs = 10000;
    cfg.recoveryHealthyCount = 3;
    cfg.recoveryIntervalMs = 2000;
    ProbeChannel probe(cfg);

    SECTION("starts stale with no reading") {
        REQUIRE_FALSE(probe.healthy());
        REQUIRE(probe.fault() == ProbeFault::Stale);
        REQUIRE_FALSE(probe.hasReading());
        REQUIRE(probe.ageMs(1000) == UINT32_MAX);
    }

    SECTION("healthy reading") {
        probe.submitReading(120.0f, 1000);
        REQUIRE(probe.healthy());
        REQUIRE(probe.celsius() == Approx(120.0f));
        REQUIRE(probe.ageMs(1500) == 500);
    }

    SECTION("non-finite reading -> Invalid") {
        probe.submitReading(NAN, 1000);
        REQUIRE(probe.fault() == ProbeFault::Invalid);
        probe.submitReading(INFINITY, 2000);
        REQUIRE(probe.fault() == ProbeFault::Invalid);
    }

    SECTION("out-of-range reading") {
        probe.submitReading(500.0f, 1000);
        REQUIRE(probe.fault() == ProbeFault::OutOfRange);
        probe.submitReading(-40.0f, 2000);
        REQUIRE(probe.fault() == ProbeFault::OutOfRange);
    }

    SECTION("electrical faults") {
        probe.submitElectricalFault(ProbeFault::Open, 1000);
        REQUIRE(probe.fault() == ProbeFault::Open);
        probe.submitElectricalFault(ProbeFault::Short, 2000);
        REQUIRE(probe.fault() == ProbeFault::Short);
    }

    SECTION("staleness detected by update") {
        probe.submitReading(100.0f, 1000);
        probe.update(11000);
        REQUIRE(probe.healthy());
        probe.update(11001);
        REQUIRE(probe.fault() == ProbeFault::Stale);
    }

    SECTION("stable recovery needs count and interval") {
        probe.submitReading(100.0f, 1000);
        probe.submitReading(100.0f, 2000);
        probe.submitReading(100.0f, 2500);
        // 3 readings but only 1500 ms span
        REQUIRE(probe.healthy());
        REQUIRE_FALSE(probe.stableHealthy());
        probe.submitReading(100.0f, 3100);
        REQUIRE(probe.stableHealthy());

        // A fault resets the debounce
        probe.submitElectricalFault(ProbeFault::Open, 3200);
        probe.submitReading(100.0f, 4000);
        REQUIRE_FALSE(probe.stableHealthy());
    }
}
