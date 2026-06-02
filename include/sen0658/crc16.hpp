#pragma once

#include <cstddef>
#include <cstdint>

namespace sen0658 {

std::uint16_t modbus_crc16(const std::uint8_t* data, std::size_t len);

} // namespace sen0658
