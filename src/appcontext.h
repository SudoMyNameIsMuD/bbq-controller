#pragma once

// Shared application context handed to the HomeKit and BLE adapters. Both
// adapters translate external actions into validated ControllerCore commands
// under the mutex; neither may touch the blower GPIO directly.

#include <mutex>

#include <controllercore.h>
#include <probechannel.h>

struct AppContext {
    std::mutex mutex;
    ControllerCore& core;
    ProbeChannel& pit;
    ProbeChannel& food1;
    ProbeChannel& food2;

    AppContext(ControllerCore& c, ProbeChannel& p, ProbeChannel& f1, ProbeChannel& f2)
        : core(c), pit(p), food1(f1), food2(f2) {
    }
};
