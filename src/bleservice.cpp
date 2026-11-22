#include "bleservice.h"

#include <cmath>
#include <cstring>

namespace blesvc {

namespace {

constexpr int16_t TEMP_UNAVAILABLE = 0x7FFF;

NimBLECharacteristic* tempsChar = nullptr;
NimBLECharacteristic* statusChar = nullptr;
NimBLECharacteristic* resultChar = nullptr;

int16_t toCenti(float celsius) {
    const float scaled = celsius * 100.0f;

    if (!std::isfinite(scaled) || scaled > 32000.0f || scaled < -32000.0f) {
        return TEMP_UNAVAILABLE;
    }

    return static_cast<int16_t>(scaled);
}

void encodeProbe(uint8_t* out, const ProbeChannel& probe) {
    const int16_t value = probe.healthy() ? toCenti(probe.celsius()) : TEMP_UNAVAILABLE;
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>(probe.fault());
}

void setResult(uint8_t opcode, uint8_t result) {
    const uint8_t payload[2] = { opcode, result };
    resultChar->setValue(payload, sizeof(payload));
    resultChar->notify();
}

class ControlCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit ControlCallbacks(AppContext& context) : m_ctx(context) {
    }

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        (void)connInfo;
        const NimBLEAttValue value = characteristic->getValue();

        if (value.size() < 1) {
            setResult(0, 2);
            return;
        }

        const uint8_t* data = value.data();
        const uint8_t opcode = data[0];
        std::lock_guard<std::mutex> lock(m_ctx.mutex);
        bool accepted = false;
        bool malformed = false;

        switch (opcode) {
            case 0x01: { // set target, int16 centi-degC
                if (value.size() != 3) {
                    malformed = true;
                    break;
                }

                const int16_t centi = static_cast<int16_t>(data[1] | (data[2] << 8));
                accepted = m_ctx.core.setTarget(centi / 100.0f);
                break;
            }

            case 0x02:
                accepted = (value.size() == 1) ? m_ctx.core.requestAutomatic()
                           : (malformed = true, false);
                break;

            case 0x03:
                accepted = (value.size() == 1) ? m_ctx.core.requestOff()
                           : (malformed = true, false);
                break;

            case 0x04: // manual duty, u8 percent
                if (value.size() != 2) {
                    malformed = true;
                    break;
                }

                accepted = m_ctx.core.requestManual(static_cast<float>(data[1]));
                break;

            case 0x05:
                accepted = (value.size() == 1) ? m_ctx.core.clearLatchedFault()
                           : (malformed = true, false);
                break;

            default:
                malformed = true;
                break;
        }

        setResult(opcode, malformed ? 2 : (accepted ? 0 : 1));
    }

private:
    AppContext& m_ctx;
};

ControlCallbacks* controlCallbacks = nullptr;

} // namespace

void setup(AppContext& ctx) {
    NimBLEDevice::init("BBQ Controller");
    // Bonding + encryption. Headless "Just Works" pairing: see header notes.
    NimBLEDevice::setSecurityAuth(true, false, true);

    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(SERVICE_UUID);

    tempsChar = service->createCharacteristic(
                    TEMPS_UUID,
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY);
    statusChar = service->createCharacteristic(
                     STATUS_UUID,
                     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY);
    resultChar = service->createCharacteristic(
                     RESULT_UUID,
                     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic* controlChar = service->createCharacteristic(
            CONTROL_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
    controlCallbacks = new ControlCallbacks(ctx);
    controlChar->setCallbacks(controlCallbacks);

    service->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();
}

void publish(AppContext& ctx) {
    if (tempsChar == nullptr || statusChar == nullptr) {
        return;
    }

    uint8_t temps[9];
    uint8_t status[8];
    {
        std::lock_guard<std::mutex> lock(ctx.mutex);
        encodeProbe(&temps[0], ctx.pit);
        encodeProbe(&temps[3], ctx.food1);
        encodeProbe(&temps[6], ctx.food2);

        const ControllerSnapshot snap = ctx.core.snapshot();
        const int16_t target = toCenti(snap.targetCelsius);
        status[0] = PROTOCOL_VERSION;
        status[1] = static_cast<uint8_t>(snap.requestedMode);
        status[2] = static_cast<uint8_t>(snap.effectiveState);
        status[3] = snap.latchedFault ? 1 : 0;
        status[4] = static_cast<uint8_t>(target & 0xFF);
        status[5] = static_cast<uint8_t>((target >> 8) & 0xFF);
        const float reqDuty = std::fmin(std::fmax(snap.requestedDuty, 0.0f), 100.0f);
        const float effDuty = std::fmin(std::fmax(snap.effectiveDuty, 0.0f), 100.0f);
        status[6] = static_cast<uint8_t>(reqDuty + 0.5f);
        status[7] = static_cast<uint8_t>(effDuty + 0.5f);
    }

    tempsChar->setValue(temps, sizeof(temps));
    tempsChar->notify();
    statusChar->setValue(status, sizeof(status));
    statusChar->notify();
}

void clearBonds() {
    NimBLEDevice::deleteAllBonds();
}

} // namespace blesvc
