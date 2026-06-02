#ifndef _WIN32

#include "sen0658/serial_port.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

namespace sen0658 {
namespace {

speed_t to_speed(int baud) {
    switch (baud) {
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return 0;
    }
}

} // namespace

SerialPort::~SerialPort() {
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool SerialPort::open(const std::string& port_name, int baud) {
    close();

    fd_ = ::open(port_name.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "open failed for " << port_name << ": " << std::strerror(errno) << '\n';
        return false;
    }

    const speed_t speed = to_speed(baud);
    if (speed == 0) {
        std::cerr << "Unsupported baud: " << baud << '\n';
        close();
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "tcgetattr failed: " << std::strerror(errno) << '\n';
        close();
        return false;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CLOCAL | CREAD;
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_lflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 100 ms chunks; Modbus timeout handled above.

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "tcsetattr failed: " << std::strerror(errno) << '\n';
        close();
        return false;
    }

    tcflush(fd_, TCIOFLUSH);
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::is_open() const {
    return fd_ >= 0;
}

bool SerialPort::write_all(const std::vector<std::uint8_t>& data) {
    if (!is_open()) return false;

    std::size_t done = 0;
    while (done < data.size()) {
        const ssize_t n = ::write(fd_, data.data() + done, data.size() - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "write failed: " << std::strerror(errno) << '\n';
            return false;
        }
        done += static_cast<std::size_t>(n);
    }

    tcdrain(fd_);
    return true;
}

int SerialPort::read_some(std::uint8_t* buffer, std::size_t max_len) {
    if (!is_open()) return -1;

    const ssize_t n = ::read(fd_, buffer, max_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) return 0;
        std::cerr << "read failed: " << std::strerror(errno) << '\n';
        return -1;
    }
    return static_cast<int>(n);
}

} // namespace sen0658

#endif // !_WIN32
