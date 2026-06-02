#include "sen0658/sen0658.hpp"

#include "sen0658/modbus_rtu.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace sen0658 {
namespace {

std::int16_t as_i16(std::uint16_t x) {
    return static_cast<std::int16_t>(x);
}

std::uint16_t reg_at(const std::vector<std::uint16_t>& regs, std::size_t i) {
    return i < regs.size() ? regs[i] : 0u;
}

} // namespace

Sensor::Sensor(std::uint8_t slave_address, int timeout_ms, bool verbose)
    : slave_address_(slave_address), timeout_ms_(timeout_ms), verbose_(verbose) {}

std::optional<Reading> Sensor::read_all(SerialPort& port) const {
    Reading out;

    // DFRobot example pattern:
    // 0x01F4 count 4: wind speed, reserved/unknown, direction sector, direction degrees.
    if (auto r = read_holding_registers(port, slave_address_, 0x01F4, 4, timeout_ms_, verbose_)) {
        // DFRobot example divides wind speed by 100.0; some documentation tables imply /10.
        // Keep /100 here to match the official example code.
        out.wind_speed_mps = reg_at(r->registers, 0) / 100.0;
        out.wind_direction_sector = static_cast<int>(reg_at(r->registers, 2));
        out.wind_direction_degrees = static_cast<int>(reg_at(r->registers, 3));
        out.wind_ok = true;
    } else if (verbose_) {
        std::cerr << "Failed to read wind block 0x01F4\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 0x01F8 count 3: humidity, temperature, noise.
    if (auto r = read_holding_registers(port, slave_address_, 0x01F8, 3, timeout_ms_, verbose_)) {
        out.humidity_percent = reg_at(r->registers, 0) / 10.0;
        out.temperature_celsius = as_i16(reg_at(r->registers, 1)) / 10.0;
        out.noise_db = reg_at(r->registers, 2) / 10.0;
        out.temp_humidity_noise_ok = true;
    } else if (verbose_) {
        std::cerr << "Failed to read temp/humidity/noise block 0x01F8\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 0x01FB count 3: PM2.5, PM10, pressure.
    if (auto r = read_holding_registers(port, slave_address_, 0x01FB, 3, timeout_ms_, verbose_)) {
        out.pm25_ugm3 = static_cast<int>(reg_at(r->registers, 0));
        out.pm10_ugm3 = static_cast<int>(reg_at(r->registers, 1));
        out.pressure_kpa = reg_at(r->registers, 2) / 10.0;
        out.pm_pressure_ok = true;
    } else if (verbose_) {
        std::cerr << "Failed to read PM/pressure block 0x01FB\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 0x01FE count 2: light high word + low word.
    if (auto r = read_holding_registers(port, slave_address_, 0x01FE, 2, timeout_ms_, verbose_)) {
        out.light_lux =
            (static_cast<std::uint32_t>(reg_at(r->registers, 0)) << 16) |
            static_cast<std::uint32_t>(reg_at(r->registers, 1));
        out.light_ok = true;
    } else if (verbose_) {
        std::cerr << "Failed to read light block 0x01FE\n";
    }

    if (!out.wind_ok && !out.temp_humidity_noise_ok && !out.pm_pressure_ok && !out.light_ok) {
        return std::nullopt;
    }

    return out;
}

} // namespace sen0658
