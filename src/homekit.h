#pragma once

// HomeKit adapter built on HomeSpan.
//
// Apple Home limitations (see plan): the Home app's displayed/selectable
// temperature ranges are not guaranteed to cover real grill temperatures
// even though the characteristic ranges below are widened. Verify behavior
// in a real Home app; BLE remains the full-range interface. Values are never
// scaled or clamped to fake a supported reading - when a probe is unhealthy
// its sensor is marked inactive/faulted instead of publishing a number.

#include <HomeSpan.h>

#include "appcontext.h"

namespace homekit {

// Pit thermostat: current pit temperature, writable cook target, and
// Heat/Off mapping to automatic/off cook modes.
struct PitThermostat : Service::Thermostat {
    AppContext& ctx;
    Characteristic::CurrentHeatingCoolingState currentState{0};
    Characteristic::TargetHeatingCoolingState targetState{0};
    Characteristic::CurrentTemperature currentTemp{20};
    Characteristic::TargetTemperature targetTemp{110};
    Characteristic::TemperatureDisplayUnits units{0};

    explicit PitThermostat(AppContext& context) : ctx(context) {
        std::lock_guard<std::mutex> lock(ctx.mutex);
        // Only Off (0) and Heat (1) are meaningful for a grill.
        targetState.setValidValues(2, 0, 1);
        // Widened ranges; actual Home app support must be verified on-device.
        currentTemp.setRange(-50, 400);
        targetTemp.setRange(50, 300, 1);
        targetTemp.setVal(ctx.core.targetCelsius());
        targetState.setVal(ctx.core.requestedMode() == RequestedMode::Automatic ? 1 : 0);
    }

    boolean update() override {
        std::lock_guard<std::mutex> lock(ctx.mutex);
        boolean ok = true;

        if (targetTemp.updated()) {
            ok = ctx.core.setTarget(targetTemp.getNewVal<float>()) && ok;
        }

        if (targetState.updated()) {
            ok = (targetState.getNewVal() == 1
                  ? ctx.core.requestAutomatic()
                  : ctx.core.requestOff()) && ok;
        }

        return ok;
    }

    void loop() override {
        std::lock_guard<std::mutex> lock(ctx.mutex);

        if (ctx.pit.healthy() && currentTemp.timeVal() > 2000) {
            currentTemp.setVal(ctx.pit.celsius());
        }

        const bool heating = ctx.core.effectiveState() == EffectiveState::Automatic ||
                             ctx.core.effectiveState() == EffectiveState::Manual;

        if (currentState.getVal() != (heating ? 1 : 0)) {
            currentState.setVal(heating ? 1 : 0);
        }

        const int wantTarget = ctx.core.requestedMode() == RequestedMode::Automatic ? 1 : 0;

        if (targetState.getVal() != wantTarget) {
            targetState.setVal(wantTarget);
        }

        const float coreTarget = ctx.core.targetCelsius();

        if (targetTemp.getVal<float>() != coreTarget) {
            targetTemp.setVal(coreTarget);
        }
    }
};

// Independent food probe sensor with explicit fault presentation.
struct FoodSensor : Service::TemperatureSensor {
    ProbeChannel& probe;
    std::mutex& mutex;
    Characteristic::CurrentTemperature temp{20};
    Characteristic::StatusActive active{false};
    Characteristic::StatusFault fault{1};

    FoodSensor(ProbeChannel& p, std::mutex& m) : probe(p), mutex(m) {
        temp.setRange(-50, 400);
    }

    void loop() override {
        std::lock_guard<std::mutex> lock(mutex);
        const bool healthy = probe.healthy();

        if (active.getVal() != (healthy ? 1 : 0)) {
            active.setVal(healthy);
            fault.setVal(healthy ? 0 : 1);
        }

        // Never publish an invented value: temperature only updates while healthy.
        if (healthy && temp.timeVal() > 2000) {
            temp.setVal(probe.celsius());
        }
    }
};

// Blower as a fan: writing a speed engages manual override through the same
// validated core commands; turning the fan off returns to the requested cook
// mode (automatic if a cook is active, otherwise off).
struct BlowerFan : Service::Fan {
    AppContext& ctx;
    Characteristic::Active active{0};
    Characteristic::RotationSpeed speed{0};

    explicit BlowerFan(AppContext& context) : ctx(context) {
    }

    boolean update() override {
        std::lock_guard<std::mutex> lock(ctx.mutex);

        if (active.updated() && active.getNewVal() == 0) {
            // Leave manual override; resume automatic if a cook was active.
            if (ctx.core.requestedMode() == RequestedMode::Manual) {
                return ctx.core.requestOff();
            }

            return true;
        }

        float duty = speed.updated() ? speed.getNewVal<float>() : speed.getVal<float>();
        return ctx.core.requestManual(duty);
    }

    void loop() override {
        std::lock_guard<std::mutex> lock(ctx.mutex);
        const float duty = ctx.core.effectiveDuty();
        const int isActive = duty > 0.0f ? 1 : 0;

        if (active.getVal() != isActive) {
            active.setVal(isActive);
        }

        if (speed.timeVal() > 2000 && speed.getVal<float>() != duty) {
            speed.setVal(duty);
        }
    }
};

inline void setup(AppContext& ctx, uint8_t statusLedPin, bool statusIsPixel,
                  uint8_t controlButtonPin) {
    if (statusIsPixel) {
        homeSpan.setStatusPixel(statusLedPin);
    } else {
        homeSpan.setStatusPin(statusLedPin);
    }

    homeSpan.setControlPin(controlButtonPin);
    homeSpan.begin(Category::Thermostats, "BBQ Controller");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("BBQ Pit");
    new PitThermostat(ctx);
    new BlowerFan(ctx);

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("Food Probe 1");
    new FoodSensor(ctx.food1, ctx.mutex);

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("Food Probe 2");
    new FoodSensor(ctx.food2, ctx.mutex);
}

} // namespace homekit
