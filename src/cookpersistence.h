#pragma once

// NVS-backed storage of the persisted cook state, rate limited to protect
// flash. Latched-fault changes are persisted immediately.

#include <Preferences.h>
#include <persistedstate.h>

class CookPersistence {
public:
    void begin() {
        m_prefs.begin("bbq", false);
    }

    PersistedCookState load() {
        PersistedCookState state;

        if (m_prefs.getBytes("cook", &state, sizeof(state)) != sizeof(state) || !state.valid()) {
            state = PersistedCookState(); // defaults: Off, no fault
        }

        return state;
    }

    void maybeSave(const ControllerSnapshot& snap, uint32_t nowMs) {
        PersistedCookState state;
        state.requestedMode = static_cast<uint8_t>(snap.requestedMode);
        state.latchedFault = snap.latchedFault ? 1 : 0;
        state.targetCelsius = snap.targetCelsius;
        state.seal();

        const bool changed = state.crc != m_lastSavedCrc;
        const bool faultChanged = (state.latchedFault != m_lastSavedFault);

        if (!changed) {
            return;
        }

        if (!faultChanged && m_hasSaved && (nowMs - m_lastSaveMs) < MIN_SAVE_INTERVAL_MS) {
            return;
        }

        m_prefs.putBytes("cook", &state, sizeof(state));
        m_lastSavedCrc = state.crc;
        m_lastSavedFault = state.latchedFault;
        m_lastSaveMs = nowMs;
        m_hasSaved = true;
    }

private:
    static constexpr uint32_t MIN_SAVE_INTERVAL_MS = 30000;

    Preferences m_prefs;
    uint32_t m_lastSavedCrc = 0;
    uint8_t m_lastSavedFault = 0;
    uint32_t m_lastSaveMs = 0;
    bool m_hasSaved = false;
};
