#pragma once

// Nearby BLE interface (NimBLE GATT server). This is NOT the Apple Home
// connection; it is the full-range fallback/monitoring interface for a
// generic BLE client app.
//
// Protocol v1 (little-endian):
//   Service  8b5f0001-5c3e-4e3f-9f6b-2bb000000001
//   TEMPS    ...0002  read/notify, encrypted. 9 bytes:
//            3 x { int16 centi-degC (0x7FFF = unavailable), uint8 fault code }
//            in order pit, food1, food2. Fault codes match ProbeFault.
//   STATUS   ...0003  read/notify, encrypted. 8 bytes:
//            u8 protocolVersion, u8 requestedMode, u8 effectiveState,
//            u8 latchedFault, int16 target centi-degC,
//            u8 requestedDuty %, u8 effectiveDuty %.
//   CONTROL  ...0004  write, encrypted (bonded). Commands:
//            0x01 <int16 target centi-degC>  set target
//            0x02                            start automatic cook
//            0x03                            off
//            0x04 <u8 duty %>                manual blower override
//            0x05                            clear latched fault
//   RESULT   ...0005  read/notify, encrypted. 2 bytes:
//            u8 last opcode, u8 result (0 = accepted, 1 = rejected,
//            2 = malformed).
//
// Security: pairing with bonding and encrypted characteristics is required.
// Headless hardware cannot display a passkey, so "Just Works" pairing is
// used; this protects against passive eavesdropping but not an active MITM
// during the initial pairing. Pair in a controlled environment. Unauthorized
// bonds can be cleared with NimBLEDevice::deleteAllBonds() via the HomeSpan
// CLI 'B' command added in main.cpp.

#include <NimBLEDevice.h>

#include "appcontext.h"

namespace blesvc {

constexpr uint8_t PROTOCOL_VERSION = 1;

constexpr const char* SERVICE_UUID = "8b5f0001-5c3e-4e3f-9f6b-2bb000000001";
constexpr const char* TEMPS_UUID   = "8b5f0002-5c3e-4e3f-9f6b-2bb000000001";
constexpr const char* STATUS_UUID  = "8b5f0003-5c3e-4e3f-9f6b-2bb000000001";
constexpr const char* CONTROL_UUID = "8b5f0004-5c3e-4e3f-9f6b-2bb000000001";
constexpr const char* RESULT_UUID  = "8b5f0005-5c3e-4e3f-9f6b-2bb000000001";

void setup(AppContext& ctx);

// Publish fresh TEMPS/STATUS values and notify subscribers. Call at the
// sampling cadence from the main loop.
void publish(AppContext& ctx);

// Remove all stored bonds (authorized recovery/reset path).
void clearBonds();

} // namespace blesvc
