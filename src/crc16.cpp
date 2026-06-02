#include "sen0658/crc16.hpp"

namespace sen0658 {

std::uint16_t modbus_crc16(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFF;

    for (std::size_t pos = 0; pos < len; ++pos) {
        crc ^= data[pos];
        for (int i = 0; i < 8; ++i) {
            if ((crc & 0x0001u) != 0u) {
                crc >>= 1u;
                crc ^= 0xA001u;
            } else {
                crc >>= 1u;
            }
        }
    }

    return crc;
}

} // namespace sen0658
