// Headless ESP32-S3 HomeKit BBQ controller.
//
// One pit probe regulates a blower through a safety state machine; two food
// probes provide independent monitoring. Apple Home pairs directly over
// Wi-Fi (HomeSpan); a versioned BLE GATT service provides nearby full-range
// monitoring and control. Autonomous regulation continues without a network.
//
// Hardware gates still open (see README): probe characterization, blower
// driver selection, pin verification and supervised grill tuning. The
// placeholder conversion coefficients and control parameters are not
// validated for real use.

#include <Arduino.h>
#include <HomeSpan.h>

#include <controllercore.h>
#include <pdalgorithm.h>
#include <probechannel.h>

#include "appcontext.h"
#include "bleservice.h"
#include "bloweroutput.h"
#include "boardconfig.h"
#include "cookpersistence.h"
#include "homekit.h"
#include "probereader.h"

namespace {

ProbeChannel pitChannel;
ProbeChannel food1Channel;
ProbeChannel food2Channel;

ControllerConfig controllerConfig;
ControllerCore core(controllerConfig, pitChannel);
PdControlAlgorithm algorithm;
AppContext ctx(core, pitChannel, food1Channel, food2Channel);

ProbeReader pitReader(ProbeHardwareConfig{PIN_PROBE_PIT}, pitChannel);
ProbeReader food1Reader(ProbeHardwareConfig{PIN_PROBE_FOOD1}, food1Channel);
ProbeReader food2Reader(ProbeHardwareConfig{PIN_PROBE_FOOD2}, food2Channel);

BlowerOutput blower;
CookPersistence persistence;

uint32_t lastSampleMs = 0;

} // namespace

void setup() {
    Serial.begin(115200);

    // Blower hardware off before anything else runs.
    blower.begin();

    pitReader.begin();
    food1Reader.begin();
    food2Reader.begin();

    persistence.begin();
    const PersistedCookState restored = persistence.load();
    core.begin(restored.latchedFault != 0, restored.mode(), restored.targetCelsius);

    homekit::setup(ctx, PIN_STATUS_LED, PIN_CONTROL_BUTTON);

    // Authorized BLE bond reset through the serial CLI.
    new SpanUserCommand('B', "delete all BLE bonds", [](const char*) {
        blesvc::clearBonds();
        Serial.println("All BLE bonds deleted.");
    });

    blesvc::setup(ctx);
}

void loop() {
    homeSpan.poll();

    const uint32_t nowMs = millis();

    if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS) {
        lastSampleMs = nowMs;

        float duty;
        {
            std::lock_guard<std::mutex> lock(ctx.mutex);
            pitReader.sample(nowMs);
            food1Reader.sample(nowMs);
            food2Reader.sample(nowMs);
            food1Channel.update(nowMs);
            food2Channel.update(nowMs);

            core.update(nowMs, algorithm);
            duty = core.effectiveDuty();
            persistence.maybeSave(core.snapshot(), nowMs);
        }

        // The only place that drives the blower output.
        blower.apply(duty);

        blesvc::publish(ctx);
    }
}
