#include "sen0658/modbus_rtu.hpp"

#include "sen0658/crc16.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace sen0658 {

std::vector<std::uint8_t> make_read_holding_registers_request(
    std::uint8_t slave_address,
    std::uint16_t start_register,
    std::uint16_t register_count
) {
    std::vector<std::uint8_t> req;
    req.reserve(8);
    req.push_back(slave_address);
    req.push_back(0x03);
    req.push_back(static_cast<std::uint8_t>(start_register >> 8));
    req.push_back(static_cast<std::uint8_t>(start_register & 0xFF));
    req.push_back(static_cast<std::uint8_t>(register_count >> 8));
    req.push_back(static_cast<std::uint8_t>(register_count & 0xFF));

    const std::uint16_t crc = modbus_crc16(req.data(), req.size());
    req.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    req.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xFF));
    return req;
}

std::string hex_dump(const std::vector<std::uint8_t>& data) {
    std::ostringstream os;
    for (std::uint8_t b : data) {
        os << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(b) << ' ';
    }
    return os.str();
}

std::optional<std::vector<std::uint16_t>> parse_read_holding_registers_response_anywhere(
    const std::vector<std::uint8_t>& rx,
    std::uint8_t slave_address,
    std::uint16_t expected_register_count
) {
    const std::size_t expected_data_bytes = static_cast<std::size_t>(expected_register_count) * 2u;
    const std::size_t frame_len = 3u + expected_data_bytes + 2u;

    if (rx.size() < frame_len) {
        return std::nullopt;
    }

    for (std::size_t off = 0; off + frame_len <= rx.size(); ++off) {
        if (rx[off + 0] != slave_address) continue;
        if (rx[off + 1] != 0x03) continue;
        if (rx[off + 2] != expected_data_bytes) continue;

        const std::uint16_t got_crc =
            static_cast<std::uint16_t>(rx[off + frame_len - 2]) |
            (static_cast<std::uint16_t>(rx[off + frame_len - 1]) << 8);

        const std::uint16_t calc_crc = modbus_crc16(rx.data() + off, frame_len - 2);
        if (got_crc != calc_crc) {
            continue;
        }

        std::vector<std::uint16_t> regs;
        regs.reserve(expected_register_count);
        for (std::size_t i = 0; i < expected_register_count; ++i) {
            const std::size_t p = off + 3u + i * 2u;
            regs.push_back(
                (static_cast<std::uint16_t>(rx[p]) << 8) |
                static_cast<std::uint16_t>(rx[p + 1])
            );
        }
        return regs;
    }

    return std::nullopt;
}

std::optional<ModbusReadResult> read_holding_registers(
    SerialPort& port,
    std::uint8_t slave_address,
    std::uint16_t start_register,
    std::uint16_t register_count,
    int timeout_ms,
    bool verbose
) {
    ModbusReadResult result;
    result.request_bytes = make_read_holding_registers_request(
        slave_address,
        start_register,
        register_count
    );

    if (verbose) {
        std::cout << "TX: " << hex_dump(result.request_bytes) << '\n';
    }

    if (!port.write_all(result.request_bytes)) {
        return std::nullopt;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        std::uint8_t tmp[128];
        const int n = port.read_some(tmp, sizeof(tmp));
        if (n < 0) {
            return std::nullopt;
        }

        if (n > 0) {
            result.response_bytes.insert(result.response_bytes.end(), tmp, tmp + n);

            if (auto regs = parse_read_holding_registers_response_anywhere(
                    result.response_bytes,
                    slave_address,
                    register_count)) {
                result.registers = *regs;
                if (verbose) {
                    std::cout << "RX: " << hex_dump(result.response_bytes) << '\n';
                }
                return result;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (verbose) {
        std::cout << "RX timeout/raw: " << hex_dump(result.response_bytes) << '\n';
    }

    return std::nullopt;
}

} // namespace sen0658
