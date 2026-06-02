#include "sen0658/sen0658.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

struct Args {
#ifdef _WIN32
    std::string port = "COM9";
#else
    std::string port = "/dev/ttyUSB0";
#endif
    int baud = 4800;
    int slave = 1;
    int interval_ms = 2000;
    int timeout_ms = 1200;
    bool interval_ms_set = false;
    bool nmea = false;
    bool once = false;
    bool verbose = true;
};

void usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --port <name>         Serial port, e.g. COM9 or /dev/ttyUSB0\n"
        << "  --baud <baud>         Baud rate, default 4800\n"
        << "  --slave <id>          Modbus slave address, default 1\n"
        << "  --interval-ms <ms>    Poll interval, default 2000 (500 with --nmea)\n"
        << "  --timeout-ms <ms>     Per-request timeout, default 1200\n"
        << "  --nmea                Emit NMEA 0183 proprietary sensor sentences at 2 Hz by default\n"
        << "  --once                Poll once and exit\n"
        << "  --quiet               Do not print raw TX/RX bytes\n"
        << "  --help                Show this help\n\n"
#ifdef _WIN32
        << "Example: " << argv0 << " --port COM9 --once\n";
#else
        << "Example: " << argv0 << " --port /dev/ttyUSB0 --once\n";
#endif
}

bool parse_int(const char* s, int& out) {
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << '\n';
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--port") {
            a.port = need_value("--port");
        } else if (arg == "--baud") {
            if (!parse_int(need_value("--baud"), a.baud)) std::exit(2);
        } else if (arg == "--slave") {
            if (!parse_int(need_value("--slave"), a.slave)) std::exit(2);
        } else if (arg == "--interval-ms") {
            if (!parse_int(need_value("--interval-ms"), a.interval_ms)) std::exit(2);
            a.interval_ms_set = true;
        } else if (arg == "--timeout-ms") {
            if (!parse_int(need_value("--timeout-ms"), a.timeout_ms)) std::exit(2);
        } else if (arg == "--nmea") {
            a.nmea = true;
            a.verbose = false;
        } else if (arg == "--once") {
            a.once = true;
        } else if (arg == "--quiet") {
            a.verbose = false;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "Unknown option: " << arg << '\n';
            usage(argv[0]);
            std::exit(2);
        } else {
            // Convenience: allow first positional arg to be the port.
            a.port = arg;
        }
    }
    if (a.nmea && !a.interval_ms_set) {
        a.interval_ms = 500;
    }
    return a;
}

std::string nmea_checksum(const std::string& payload) {
    unsigned char checksum = 0;
    for (const char c : payload) {
        checksum ^= static_cast<unsigned char>(c);
    }

    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(checksum);
    return out.str();
}

void print_nmea_sentence(const std::string& payload) {
    std::cout << '$' << payload << '*' << nmea_checksum(payload) << "\r\n";
}

std::string format_double(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

void print_nmea_reading(const sen0658::Reading& d) {
    // Proprietary NMEA 0183 sentences keep every SEN0658 channel available even
    // when no standard sentence exists for a sensor, such as PM, noise, or lux.
    print_nmea_sentence(
        "PSENWND," + format_double(d.wind_speed_mps, 2) + ",M," +
        std::to_string(d.wind_direction_degrees) + ",T," +
        std::to_string(d.wind_direction_sector) + "," +
        (d.wind_ok ? "A" : "V")
    );

    print_nmea_sentence(
        "PSENENV," + format_double(d.temperature_celsius, 1) + ",C," +
        format_double(d.humidity_percent, 1) + ",P," +
        format_double(d.noise_db, 1) + ",D," +
        format_double(d.pressure_kpa, 1) + ",K," +
        (d.temp_humidity_noise_ok && d.pm_pressure_ok ? "A" : "V")
    );

    print_nmea_sentence(
        "PSENAIR," + std::to_string(d.pm25_ugm3) + ",UGM3," +
        std::to_string(d.pm10_ugm3) + ",UGM3," +
        (d.pm_pressure_ok ? "A" : "V")
    );

    print_nmea_sentence(
        "PSENLUX," + std::to_string(d.light_lux) + ",LX," +
        (d.light_ok ? "A" : "V")
    );
}

void print_reading(const sen0658::Reading& d) {
    std::cout << "\n=== DFRobot SEN0658 " << (d.all_ok() ? "OK" : "PARTIAL") << " ===\n";
    std::cout << "Wind speed:       " << d.wind_speed_mps << " m/s" << (d.wind_ok ? "" : "  (no data)") << '\n';
    std::cout << "Wind sector:      " << d.wind_direction_sector << '\n';
    std::cout << "Wind direction:   " << d.wind_direction_degrees << " deg\n";
    std::cout << "Temperature:      " << d.temperature_celsius << " C" << (d.temp_humidity_noise_ok ? "" : "  (no data)") << '\n';
    std::cout << "Humidity:         " << d.humidity_percent << " %RH\n";
    std::cout << "Noise:            " << d.noise_db << " dB\n";
    std::cout << "Pressure:         " << d.pressure_kpa << " kPa" << (d.pm_pressure_ok ? "" : "  (no data)") << '\n';
    std::cout << "PM2.5:            " << d.pm25_ugm3 << " ug/m3\n";
    std::cout << "PM10:             " << d.pm10_ugm3 << " ug/m3\n";
    std::cout << "Light:            " << d.light_lux << " lux" << (d.light_ok ? "" : "  (no data)") << '\n';
    std::cout << "============================\n";
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    if (args.slave < 1 || args.slave > 247) {
        std::cerr << "Modbus slave address must be 1..247\n";
        return 2;
    }

    if (!args.nmea) {
        std::cout << "Opening " << args.port << " at " << args.baud << " 8N1\n";
        std::cout << "Polling Modbus slave " << args.slave << "\n";
        std::cout << "If the adapter TX LED never blinks, check COM port/driver first.\n";
    }

    sen0658::SerialPort port;
    if (!port.open(args.port, args.baud)) {
        return 1;
    }

    const sen0658::Sensor sensor(
        static_cast<std::uint8_t>(args.slave),
        args.timeout_ms,
        args.verbose
    );

    do {
        if (auto reading = sensor.read_all(port)) {
            if (args.nmea) {
                print_nmea_reading(*reading);
            } else {
                print_reading(*reading);
            }
        } else if (args.nmea) {
            std::cerr << "No valid Modbus replies decoded. Check power, common ground, A/B polarity, and RS485 wiring.\n";
        } else {
            std::cout << "\nNo valid Modbus replies decoded.\n";
            std::cout << "Check power, common ground, A/B polarity, and whether RS422 adapter can do 2-wire RS485.\n";
        }

        if (!args.once) {
            std::this_thread::sleep_for(std::chrono::milliseconds(args.interval_ms));
        }
    } while (!args.once);

    return 0;
}
