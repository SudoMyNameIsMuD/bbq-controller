#pragma once

#include <cstdint>
#include <cstring>

#include "controllercore.h"

// Versioned, checksummed cook state persisted across reboots (NVS on the
// ESP32-S3, a plain buffer in host tests). Kept deliberately small to limit
// flash wear; the firmware must additionally rate-limit writes.
struct PersistedCookState {
    static constexpr uint32_t MAGIC = 0x42425131; // "BBQ1"
    static constexpr uint16_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint16_t version = VERSION;
    uint8_t requestedMode = static_cast<uint8_t>(RequestedMode::Off);
    uint8_t latchedFault = 0;
    float targetCelsius = 0.0f;
    uint32_t crc = 0;

    // CRC-32 over all fields except crc itself.
    uint32_t computeCrc() const;

    void seal() {
        magic = MAGIC;
        version = VERSION;
        crc = computeCrc();
    }

    // True when magic, version and CRC are all valid.
    bool valid() const {
        return magic == MAGIC && version == VERSION && crc == computeCrc();
    }

    RequestedMode mode() const {
        return requestedMode <= static_cast<uint8_t>(RequestedMode::Manual)
               ? static_cast<RequestedMode>(requestedMode) : RequestedMode::Off;
    }
};
