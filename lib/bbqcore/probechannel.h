#pragma once

#include <cstdint>

// Fault classification for one temperature probe channel.
enum class ProbeFault : uint8_t {
    None = 0,       // healthy, fresh reading available
    Open = 1,       // electrically open (probe unplugged / broken)
    Short = 2,      // electrically shorted
    OutOfRange = 3, // converted temperature outside the plausible range
    Stale = 4,      // no fresh reading within the configured timeout
    Invalid = 5     // non-finite or otherwise invalid data
};

struct ProbeChannelConfig {
    float minValidCelsius = -20.0f;   // below -> OutOfRange
    float maxValidCelsius = 400.0f;   // above -> OutOfRange
    uint32_t staleTimeoutMs = 10000;  // no reading for this long -> Stale
    // Recovery debounce: this many consecutive healthy readings spanning at
    // least this interval before the channel reports stable health again.
    uint16_t recoveryHealthyCount = 5;
    uint32_t recoveryIntervalMs = 5000;
};

// One independent probe measurement channel.
//
// The hardware acquisition layer converts its raw measurement either into a
// calibrated temperature (submitReading) or an electrical fault
// (submitElectricalFault). This class classifies plausibility and freshness
// and tracks the stable-recovery debounce used to gate control decisions.
class ProbeChannel {
public:
    explicit ProbeChannel(const ProbeChannelConfig& config = ProbeChannelConfig());

    // Feed one calibrated reading. Classifies Invalid / OutOfRange itself.
    void submitReading(float celsius, uint32_t nowMs);

    // Report an electrical fault detected by the acquisition circuitry.
    void submitElectricalFault(ProbeFault fault, uint32_t nowMs);

    // Call periodically; detects staleness even when no readings arrive.
    void update(uint32_t nowMs);

    // True when the last reading is healthy and fresh.
    bool healthy() const;

    // True when healthy() and the recovery debounce criteria are satisfied:
    // recoveryHealthyCount consecutive healthy readings spanning at least
    // recoveryIntervalMs. Used by the controller to gate (re)enabling output.
    bool stableHealthy() const;

    // Last known temperature. Only meaningful when healthy().
    float celsius() const;

    ProbeFault fault() const;

    // Age of the last accepted healthy reading, or UINT32_MAX if none.
    uint32_t ageMs(uint32_t nowMs) const;

    bool hasReading() const;

private:
    void markFault(ProbeFault fault);

    ProbeChannelConfig m_config;
    float m_celsius = 0.0f;
    bool m_hasReading = false;
    uint32_t m_lastHealthyMs = 0;
    ProbeFault m_fault = ProbeFault::Stale; // no data yet
    uint16_t m_consecutiveHealthy = 0;
    uint32_t m_recoveryStartMs = 0;
};
