# Headless ESP32-S3 HomeKit BBQ Controller

A headless, Wi-Fi + Bluetooth kamado/BBQ controller: one pit probe regulates a
blower through a safety state machine; two independent food probes provide
monitoring. Pairs directly with Apple Home over Wi-Fi (no bridge) via
[HomeSpan](https://github.com/HomeSpan/HomeSpan), and exposes a versioned BLE
GATT service for nearby full-range monitoring and control with a generic BLE
client. Regulation continues autonomously without any network connection.

Derived from [rvt/bbq-controller](https://github.com/rvt/bbq-controller)
(MIT); the legacy display/MQTT firmware has been removed. The original fuzzy
controller is retained in `lib/bbq/` for algorithm comparison in simulation.

## Status — read before use

Firmware builds and the controller core is host-tested, but **the project is
not validated on hardware**. Open gates (see the state machine and placeholder
warnings in the source):

- **Probe characterization** — `src/probereader.h` uses *placeholder* NTC
  beta-model coefficients and divider values. Characterize the real probes
  (resistance vs. verified reference temperatures) and replace them before
  trusting any reading.
- **Pin verification** — all pins in `src/boardconfig.h` are provisional for
  an ESP32-S3 DevKitC-1-class board; verify against your module's schematic.
- **Blower driver** — never connect a blower directly to a GPIO. Select a
  driver that is electrically default-off, plus a rated supply and a physical
  blower power disconnect.
- **Control/safety parameters** — target range, over-temperature trip,
  timeouts and PD gains in `lib/bbqcore/` are placeholders to validate on the
  bench and in supervised grill trials.
- **Apple Home range behavior** — characteristic ranges are widened, but the
  Home app's actual display/selection limits must be verified on a real
  iPhone. BLE is the full-range interface; values are never scaled or faked.

This firmware is not a fire-suppression system and does not make unattended
cooking safe. Charcoal keeps burning through natural draft with the blower
off.

## Architecture

```
src/                     ESP32-S3 firmware
  main.cpp               setup/loop; the only place that drives the blower
  boardconfig.h          provisional pins and cadence
  probereader.*          ADC acquisition, open/short detection, conversion
  bloweroutput.h         LEDC PWM output (Arduino-ESP32 3.x API)
  cookpersistence.h      NVS persistence, rate-limited
  homekit.h              HomeSpan accessories (pit thermostat, 2 food sensors, fan)
  bleservice.*           NimBLE GATT service (protocol v1, documented in header)
  appcontext.h           shared mutex-guarded context for the adapters

lib/bbqcore/             platform-independent controller core (host-tested)
  probechannel.*         per-probe fault model: open/short/out-of-range/stale/
                         invalid, freshness, stable-recovery debounce
  controllercore.*       single owner of mode/target/faults; safety arbitration
  pdalgorithm.*          initial PD control algorithm
  persistedstate.*       versioned, CRC-checked persisted cook state

lib/bbq/                 legacy fuzzy controller, kept for simulation comparison
libtest/                 Catch2 host tests (coretests)
```

### Safety behavior

- Requested mode (off/automatic/manual) is separate from effective state
  (starting/off/automatic/manual/pit-inhibited/latched-fault).
- Precedence: latched fault → pit-probe health → requested mode. Interlocks
  apply to manual override too; no remote interface can bypass them.
- A pit probe fault turns the blower off immediately but retains the cook;
  control resumes automatically only after consecutive fresh healthy readings
  over the recovery interval.
- Over-temperature latches the blower off, persists across reboots, and can
  only be cleared when the pit is stably healthy and cooled below the trip
  margin.
- Food probe faults affect only that reading; no substitute value is ever
  published.
- After reboot the blower starts off; an automatic cook resumes only after
  valid persisted configuration and fresh pit readings pass startup checks.
  Manual override never survives a reboot.

## Building

```sh
pio run -e esp32s3          # firmware (pinned pioarduino platform, Arduino-ESP32 3.3.x)
```

The pinned platform provides the Arduino-ESP32 3.3.x core HomeSpan requires;
the 8 MB `default_8MB` partition table satisfies HomeSpan's ≥1.9 MB app
partition requirement.

### Host tests

```sh
cd libtest
cmake -B build && cmake --build build && ./build/coretests
```

(Or compile directly: `clang++ -std=c++17 -I. -I../lib/bbqcore coretests.cpp
../lib/bbqcore/*.cpp -o coretests`.) Legacy fuzzy tests build only when
`-DLEGACY_LIBDEPS=<dir with eFLL and opt-parser>` is passed to CMake.

## Setup and pairing

- **Wi-Fi / HomeKit:** use HomeSpan's headless workflows (serial CLI, control
  button + status LED) to provision Wi-Fi and the pairing code, then add the
  accessory in Apple Home. A HomePod/Apple TV home hub is required for access
  away from home.
- **BLE:** the GATT protocol (service/characteristic UUIDs, byte layouts,
  command opcodes, security notes) is documented in `src/bleservice.h`.
  Pairing uses bonding + encryption ("Just Works" — pair in a controlled
  environment). Delete all bonds with the `B` command on the serial CLI.

## License

MIT, see [LICENSE](LICENSE). Based on work by R. van Twisk.
