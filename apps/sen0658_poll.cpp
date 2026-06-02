#include "sen0658/sen0658.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalid_socket_handle = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle invalid_socket_handle = -1;
#endif

void close_socket(SocketHandle s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

bool set_nonblocking(SocketHandle s) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

class TcpRuntime {
public:
    TcpRuntime() {
#ifdef _WIN32
        WSADATA data{};
        ok_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
        ok_ = true;
#endif
    }

    TcpRuntime(const TcpRuntime&) = delete;
    TcpRuntime& operator=(const TcpRuntime&) = delete;

    ~TcpRuntime() {
#ifdef _WIN32
        if (ok_) WSACleanup();
#endif
    }

    bool ok() const { return ok_; }

private:
    bool ok_ = false;
};

class NmeaTcpServer {
public:
    NmeaTcpServer() = default;
    NmeaTcpServer(const NmeaTcpServer&) = delete;
    NmeaTcpServer& operator=(const NmeaTcpServer&) = delete;

    ~NmeaTcpServer() {
        for (const auto client : clients_) {
            close_socket(client);
        }
        if (listen_socket_ != invalid_socket_handle) {
            close_socket(listen_socket_);
        }
    }

    bool open(const std::string& bind_address, int tcp_port) {
        if (tcp_port < 1 || tcp_port > 65535) {
            std::cerr << "TCP NMEA port must be 1..65535\n";
            return false;
        }

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = bind_address.empty() ? AI_PASSIVE : 0;

        const std::string service = std::to_string(tcp_port);
        addrinfo* raw = nullptr;
        const char* node = bind_address.empty() ? nullptr : bind_address.c_str();
        const int rc = getaddrinfo(node, service.c_str(), &hints, &raw);
        if (rc != 0) {
            std::cerr << "Failed to resolve TCP NMEA bind address: " << gai_strerror(rc) << '\n';
            return false;
        }

        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
        for (addrinfo* ai = addresses.get(); ai != nullptr; ai = ai->ai_next) {
            SocketHandle candidate = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (candidate == invalid_socket_handle) continue;

            int yes = 1;
            setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

            if (bind(candidate, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0 &&
                listen(candidate, 8) == 0 &&
                set_nonblocking(candidate)) {
                listen_socket_ = candidate;
                return true;
            }

            close_socket(candidate);
        }

        std::cerr << "Failed to open TCP NMEA server socket\n";
        return false;
    }

    void accept_pending() {
        while (true) {
            sockaddr_storage peer{};
            socklen_t peer_len = sizeof(peer);
            const SocketHandle client = accept(
                listen_socket_,
                reinterpret_cast<sockaddr*>(&peer),
                &peer_len
            );
            if (client == invalid_socket_handle) return;

            if (set_nonblocking(client)) {
                clients_.push_back(client);
                std::cerr << "TCP NMEA client connected (" << clients_.size() << " active)\n";
            } else {
                close_socket(client);
            }
        }
    }

    void broadcast(const std::string& payload) {
        accept_pending();
        auto end = std::remove_if(clients_.begin(), clients_.end(), [&](SocketHandle client) {
#ifdef MSG_NOSIGNAL
            const int flags = MSG_NOSIGNAL;
#else
            const int flags = 0;
#endif
            const int sent = send(client, payload.data(), static_cast<int>(payload.size()), flags);
            if (sent == static_cast<int>(payload.size())) {
                return false;
            }
            close_socket(client);
            std::cerr << "TCP NMEA client disconnected\n";
            return true;
        });
        clients_.erase(end, clients_.end());
    }

private:
    SocketHandle listen_socket_ = invalid_socket_handle;
    std::vector<SocketHandle> clients_;
};

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
    int request_gap_ms = 250;
    int nmea_tcp_port = 0;
    bool interval_ms_set = false;
    bool request_gap_ms_set = false;
    bool nmea = false;
    bool once = false;
    bool verbose = true;
    std::string nmea_tcp_bind;
};

void usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --port <name>         Serial port, e.g. COM9 or /dev/ttyUSB0\n"
        << "  --baud <baud>         Baud rate, default 4800\n"
        << "  --slave <id>          Modbus slave address, default 1\n"
        << "  --interval-ms <ms>    Poll interval, default 2000 (500 with NMEA output)\n"
        << "  --rate-hz <hz>        Poll rate, alternative to --interval-ms; NMEA default 2 Hz\n"
        << "  --timeout-ms <ms>     Per-request timeout, default 1200\n"
        << "  --request-gap-ms <ms> Delay between Modbus requests, default 250 (10 with NMEA output)\n"
        << "  --nmea                Emit NMEA 0183 standard/XDR sensor sentences at 2 Hz by default\n"
        << "  --nmea-tcp-port <p>   Listen on TCP port p and stream NMEA 0183 sentences\n"
        << "  --nmea-tcp-bind <a>   Bind TCP NMEA server to address a, default all interfaces\n"
        << "  --once                Poll once and exit (ignored by TCP NMEA server)\n"
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

bool parse_rate_hz(const char* s, int& interval_ms) {
    char* end = nullptr;
    const double hz = std::strtod(s, &end);
    if (end == s || *end != '\0' || hz <= 0.0) return false;
    interval_ms = std::max(1, static_cast<int>((1000.0 / hz) + 0.5));
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
        } else if (arg == "--rate-hz") {
            if (!parse_rate_hz(need_value("--rate-hz"), a.interval_ms)) {
                std::cerr << "Poll rate must be a positive number of Hz\n";
                std::exit(2);
            }
            a.interval_ms_set = true;
        } else if (arg == "--timeout-ms") {
            if (!parse_int(need_value("--timeout-ms"), a.timeout_ms)) std::exit(2);
        } else if (arg == "--request-gap-ms") {
            if (!parse_int(need_value("--request-gap-ms"), a.request_gap_ms)) std::exit(2);
            a.request_gap_ms_set = true;
        } else if (arg == "--nmea-tcp-port") {
            if (!parse_int(need_value("--nmea-tcp-port"), a.nmea_tcp_port)) std::exit(2);
            a.nmea = true;
            a.verbose = false;
        } else if (arg == "--nmea-tcp-bind") {
            a.nmea_tcp_bind = need_value("--nmea-tcp-bind");
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
    if (a.nmea && !a.request_gap_ms_set) {
        a.request_gap_ms = 10;
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

std::string nmea_sentence(const std::string& payload) {
    return "$" + payload + "*" + nmea_checksum(payload) + "\r\n";
}

std::string format_double(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string format_if(bool ok, double value, int precision) {
    return ok ? format_double(value, precision) : std::string();
}

void append_xdr_measurement(
    std::vector<std::string>& fields,
    const std::string& transducer_type,
    const std::string& value,
    const std::string& units,
    const std::string& name
) {
    fields.push_back(transducer_type);
    fields.push_back(value);
    fields.push_back(units);
    fields.push_back(name);
}

std::string xdr_sentence(const std::vector<std::string>& fields) {
    std::string payload = "WIXDR";
    for (const auto& field : fields) {
        payload += "," + field;
    }
    return nmea_sentence(payload);
}

std::string nmea_reading(const sen0658::Reading& d) {
    // Use standard NMEA 0183 meteorological sentences for the channels that
    // have well-known sentences, and XDR transducer sentences for the SEN0658's
    // remaining environmental and air-quality channels.
    std::string out;

    // MWV: apparent wind angle, speed in metres per second, and status.
    out += nmea_sentence(
        "WIMWV," + format_double(d.wind_direction_degrees, 1) + ",R," +
        format_double(d.wind_speed_mps, 2) + ",M," +
        (d.wind_ok ? "A" : "V")
    );

    // MDA: barometric pressure and outside air temperature. Empty fields are
    // retained for the other MDA measurements so humidity and similar channels
    // can be represented by XDR below without duplicating them here.
    const bool pressure_ok = d.pm_pressure_ok;
    const bool temperature_ok = d.temp_humidity_noise_ok;
    const double pressure_bar = d.pressure_kpa / 100.0;
    const double pressure_inhg = d.pressure_kpa * 0.295299830714;
    out += nmea_sentence(
        "WIMDA," + format_if(pressure_ok, pressure_inhg, 4) + ",I," +
        format_if(pressure_ok, pressure_bar, 4) + ",B," +
        format_if(temperature_ok, d.temperature_celsius, 1) + ",C," +
        ",C,,,,,,,,,,,,,,"
    );

    if (d.wind_ok) {
        std::vector<std::string> fields;
        append_xdr_measurement(fields, "A", std::to_string(d.wind_direction_sector), "N", "WIND_SECTOR");
        out += xdr_sentence(fields);
    }

    if (d.temp_humidity_noise_ok) {
        std::vector<std::string> fields;
        append_xdr_measurement(fields, "H", format_double(d.humidity_percent, 1), "P", "REL_HUMIDITY");
        append_xdr_measurement(fields, "G", format_double(d.noise_db, 1), "D", "NOISE_DB");
        out += xdr_sentence(fields);
    }

    if (d.pm_pressure_ok) {
        std::vector<std::string> fields;
        append_xdr_measurement(fields, "G", std::to_string(d.pm25_ugm3), "UGM3", "PM2_5");
        append_xdr_measurement(fields, "G", std::to_string(d.pm10_ugm3), "UGM3", "PM10");
        out += xdr_sentence(fields);
    }

    if (d.light_ok) {
        std::vector<std::string> fields;
        append_xdr_measurement(fields, "G", std::to_string(d.light_lux), "LX", "LIGHT");
        out += xdr_sentence(fields);
    }

    return out;
}

void print_nmea_reading(const sen0658::Reading& d) {
    std::cout << nmea_reading(d);
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

    if (args.interval_ms < 1) {
        std::cerr << "Poll interval must be at least 1 ms\n";
        return 2;
    }

    if (args.request_gap_ms < 0) {
        std::cerr << "Modbus request gap must be at least 0 ms\n";
        return 2;
    }

    if (args.nmea_tcp_port < 0 || args.nmea_tcp_port > 65535) {
        std::cerr << "TCP NMEA port must be 1..65535\n";
        return 2;
    }

    if (!args.nmea) {
        std::cout << "Opening " << args.port << " at " << args.baud << " 8N1\n";
        std::cout << "Polling Modbus slave " << args.slave << "\n";
        std::cout << "If the adapter TX LED never blinks, check COM port/driver first.\n";
    }

    TcpRuntime tcp_runtime;
    std::optional<NmeaTcpServer> nmea_tcp_server;
    if (args.nmea_tcp_port > 0) {
        if (!tcp_runtime.ok()) {
            std::cerr << "Failed to initialize TCP sockets\n";
            return 1;
        }
        nmea_tcp_server.emplace();
        if (!nmea_tcp_server->open(args.nmea_tcp_bind, args.nmea_tcp_port)) {
            return 1;
        }
        std::cerr << "TCP NMEA 0183 server listening on "
                  << (args.nmea_tcp_bind.empty() ? std::string("all interfaces") : args.nmea_tcp_bind)
                  << ':' << args.nmea_tcp_port << " at "
                  << format_double(1000.0 / args.interval_ms, 2) << " Hz\n";
    }

    sen0658::SerialPort port;
    if (!port.open(args.port, args.baud)) {
        return 1;
    }

    const sen0658::Sensor sensor(
        static_cast<std::uint8_t>(args.slave),
        args.timeout_ms,
        args.verbose,
        args.request_gap_ms
    );

    do {
        const auto poll_started = std::chrono::steady_clock::now();
        if (nmea_tcp_server) {
            nmea_tcp_server->accept_pending();
        }

        if (auto reading = sensor.read_all(port)) {
            if (nmea_tcp_server) {
                nmea_tcp_server->broadcast(nmea_reading(*reading));
            } else if (args.nmea) {
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

        if (!args.once || nmea_tcp_server) {
            const auto next_poll = poll_started + std::chrono::milliseconds(args.interval_ms);
            std::this_thread::sleep_until(next_poll);
        }
    } while (!args.once || nmea_tcp_server);

    return 0;
}
