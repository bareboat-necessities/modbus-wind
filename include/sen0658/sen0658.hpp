#pragma once

#include "sen0658/serial_port.hpp"

#include <cstdint>
#include <optional>

namespace sen0658 {

struct Reading {
    double wind_speed_mps = 0.0;
    int wind_direction_sector = 0;
    int wind_direction_degrees = 0;

    double humidity_percent = 0.0;
    double temperature_celsius = 0.0;
    double noise_db = 0.0;

    int pm25_ugm3 = 0;
    int pm10_ugm3 = 0;
    double pressure_kpa = 0.0;

    std::uint32_t light_lux = 0;

    bool wind_ok = false;
    bool temp_humidity_noise_ok = false;
    bool pm_pressure_ok = false;
    bool light_ok = false;

    [[nodiscard]] bool all_ok() const {
        return wind_ok && temp_humidity_noise_ok && pm_pressure_ok && light_ok;
    }
};

class Sensor {
public:
    explicit Sensor(
        std::uint8_t slave_address = 1,
        int timeout_ms = 1200,
        bool verbose = true,
        int request_gap_ms = 250
    );

    std::optional<Reading> read_all(SerialPort& port) const;

private:
    std::uint8_t slave_address_ = 1;
    int timeout_ms_ = 1200;
    bool verbose_ = true;
    int request_gap_ms_ = 250;
};

} // namespace sen0658
