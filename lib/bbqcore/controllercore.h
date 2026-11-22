#pragma once

#include <cstdint>

#include "probechannel.h"

// What the user asked for.
enum class RequestedMode : uint8_t {
    Off = 0,
    Automatic = 1,
    Manual = 2
};

// What the controller is actually doing after safety arbitration.
enum class EffectiveState : uint8_t {
    Starting = 0,     // startup checks in progress, output off
    Off = 1,          // user-requested off
    Automatic = 2,    // algorithm drives the blower
    Manual = 3,       // user-requested duty drives the blower
    PitInhibited = 4, // pit probe unhealthy, output off, cook retained
    LatchedFault = 5  // over-temperature (or persisted) latch, output off
};

struct ControllerConfig {
    // Safe pit target range; targets outside are rejected.
    // PLACEHOLDER values - validate against the actual probes and grill
    // before real use (see plan "Open gates").
    float minTargetCelsius = 50.0f;
    float maxTargetCelsius = 300.0f;

    // Absolute over-temperature trip. Blower off + latched fault.
    float overTempTripCelsius = 350.0f;

    // Latched fault may only be cleared when the pit reads below
    // overTempTripCelsius - overTempClearMarginCelsius and is healthy.
    float overTempClearMarginCelsius = 25.0f;

    // Output limits, percent duty.
    float maxDuty = 100.0f;
    // Duties in (0, minRunDuty) are raised to minRunDuty so the blower
    // actually spins; exact value depends on the selected blower.
    float minRunDuty = 0.0f;
};

// Interface for the control algorithm. It only produces a *requested* duty;
// the core applies safety arbitration and output limits afterwards.
class ControlAlgorithm {
public:
    virtual ~ControlAlgorithm() = default;
    virtual float requestedDuty(float pitCelsius, float targetCelsius, uint32_t nowMs) = 0;
    // Called when automatic control (re)starts so stale internal state is dropped.
    virtual void reset() {
    }
};

// Snapshot published to the HomeKit and BLE adapters.
struct ControllerSnapshot {
    RequestedMode requestedMode;
    EffectiveState effectiveState;
    float targetCelsius;
    float requestedDuty; // what mode/algorithm asked for
    float effectiveDuty; // what is actually applied after arbitration
    bool latchedFault;
};

// Single owner of mode, target, faults and output arbitration.
//
// All command entry points (HomeKit, BLE, future UIs) must go through the
// bool-returning methods below; none of them can bypass the safety
// arbitration performed in update().
class ControllerCore {
public:
    ControllerCore(const ControllerConfig& config, ProbeChannel& pitProbe);

    // Startup. A persisted latched fault is never bypassed. A restored
    // Manual mode is explicitly downgraded to Off (decision: manual override
    // does not survive a reboot); a restored Automatic cook resumes only
    // after startup checks pass in update().
    void begin(bool persistedLatchedFault, RequestedMode restoredMode, float restoredTargetCelsius);

    // Commands. Return false when rejected (invalid value or interlock).
    bool setTarget(float celsius);
    bool requestAutomatic();
    bool requestManual(float duty);
    bool requestOff();

    // Clear a latched fault. Succeeds only when the pit probe is healthy and
    // reads below the trip threshold minus the clear margin.
    bool clearLatchedFault();

    // Periodic arbitration; call at the sampling cadence. Reads the pit
    // channel, runs the algorithm when in automatic mode and computes the
    // effective output. The caller applies effectiveDuty() to the hardware.
    void update(uint32_t nowMs, ControlAlgorithm& algorithm);

    RequestedMode requestedMode() const {
        return m_requestedMode;
    }
    EffectiveState effectiveState() const {
        return m_effectiveState;
    }
    float targetCelsius() const {
        return m_targetCelsius;
    }
    float effectiveDuty() const {
        return m_effectiveDuty;
    }
    bool latchedFault() const {
        return m_latchedFault;
    }
    ControllerSnapshot snapshot() const;

private:
    float clampDuty(float duty) const;

    ControllerConfig m_config;
    ProbeChannel& m_pitProbe;

    RequestedMode m_requestedMode = RequestedMode::Off;
    EffectiveState m_effectiveState = EffectiveState::Starting;
    float m_targetCelsius;
    float m_manualDuty = 0.0f;
    float m_requestedDuty = 0.0f;
    float m_effectiveDuty = 0.0f;
    bool m_latchedFault = false;
    bool m_started = false;
    bool m_algorithmNeedsReset = true;
};
