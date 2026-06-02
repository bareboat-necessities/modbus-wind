#pragma once

#include "sen0658/serial_port.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sen0658 {

struct ModbusReadResult {
    std::vector<std::uint16_t> registers;
    std::vector<std::uint8_t> request_bytes;
    std::vector<std::uint8_t> response_bytes;
};

std::vector<std::uint8_t> make_read_holding_registers_request(
    std::uint8_t slave_address,
    std::uint16_t start_register,
    std::uint16_t register_count
);

std::string hex_dump(const std::vector<std::uint8_t>& data);

std::optional<std::vector<std::uint16_t>> parse_read_holding_registers_response_anywhere(
    const std::vector<std::uint8_t>& rx,
    std::uint8_t slave_address,
    std::uint16_t expected_register_count
);

std::optional<ModbusReadResult> read_holding_registers(
    SerialPort& port,
    std::uint8_t slave_address,
    std::uint16_t start_register,
    std::uint16_t register_count,
    int timeout_ms,
    bool verbose
);

} // namespace sen0658
