#ifdef _WIN32

#include "sen0658/serial_port.hpp"

#include <iostream>
#include <utility>

namespace sen0658 {

SerialPort::~SerialPort() {
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_HANDLE_VALUE;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool SerialPort::open(const std::string& port_name, int baud) {
    close();

    std::string path = port_name;
    if (path.rfind("\\\\.\\", 0) != 0) {
        path = "\\\\.\\" + path;
    }

    handle_ = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed for " << path << ", GetLastError=" << GetLastError() << '\n';
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        std::cerr << "GetCommState failed, GetLastError=" << GetLastError() << '\n';
        close();
        return false;
    }

    dcb.BaudRate = static_cast<DWORD>(baud);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(handle_, &dcb)) {
        std::cerr << "SetCommState failed, GetLastError=" << GetLastError() << '\n';
        close();
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 250;

    if (!SetCommTimeouts(handle_, &timeouts)) {
        std::cerr << "SetCommTimeouts failed, GetLastError=" << GetLastError() << '\n';
        close();
        return false;
    }

    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool SerialPort::is_open() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

bool SerialPort::write_all(const std::vector<std::uint8_t>& data) {
    if (!is_open()) return false;

    DWORD written = 0;
    if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr)) {
        std::cerr << "WriteFile failed, GetLastError=" << GetLastError() << '\n';
        return false;
    }
    return written == data.size();
}

int SerialPort::read_some(std::uint8_t* buffer, std::size_t max_len) {
    if (!is_open()) return -1;

    DWORD got = 0;
    if (!ReadFile(handle_, buffer, static_cast<DWORD>(max_len), &got, nullptr)) {
        std::cerr << "ReadFile failed, GetLastError=" << GetLastError() << '\n';
        return -1;
    }
    return static_cast<int>(got);
}

} // namespace sen0658

#endif // _WIN32
