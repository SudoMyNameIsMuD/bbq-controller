#include "persistedstate.h"

namespace {

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }

    return ~crc;
}

} // namespace

uint32_t PersistedCookState::computeCrc() const {
    PersistedCookState copy = *this;
    copy.crc = 0;
    return crc32(reinterpret_cast<const uint8_t*>(&copy), sizeof(copy) - sizeof(copy.crc));
}
