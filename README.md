# SEN0658 PC Modbus Reader

C++17 PC-side Modbus-RTU test program for the DFRobot SEN0658 RS485 9-in-1 weather sensor.

It is meant for direct debugging with a USB serial adapter. It prints raw TX/RX bytes so you can see whether the FTDI adapter is actually transmitting and whether the sensor replies.

## Sensor defaults

Factory/default settings used by this program:

- Modbus RTU slave address: `1`
- Baud: `4800`
- Data format: `8N1`
- Read function: Modbus function `0x03`

## Wiring

For a true 2-wire USB-RS485 adapter:

```text
SEN0658 brown  -> +10..30 V DC
SEN0658 black  -> power supply negative AND adapter GND
SEN0658 yellow -> RS485 A / D+
SEN0658 blue   -> RS485 B / D-
```

For an RS422 stick with `TX+ TX- RX+ RX-`, this sometimes works:

```text
SEN0658 yellow -> TX+ and RX+ tied together
SEN0658 blue   -> TX- and RX- tied together
SEN0658 black  -> adapter GND and supply negative
SEN0658 brown  -> +10..30 V DC
```

If TX blinks but there is no RX, try swapping yellow/blue. If it still does not work, use a real 2-wire half-duplex USB-RS485 adapter.

## Build locally

### Linux

```bash
make
./build/sen0658_poll --port /dev/ttyUSB0 --once
```

Or without `make`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sen0658_poll --port /dev/ttyUSB0 --once
```

### Windows

With Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
.\build\Release\sen0658_poll.exe --port COM9 --once
```

With MSYS2/MinGW:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sen0658_poll.exe --port COM9 --once
```

## Usage

```text
sen0658_poll [options]

Options:
  --port <name>         Serial port, e.g. COM9 or /dev/ttyUSB0
  --baud <baud>         Baud rate, default 4800
  --slave <id>          Modbus slave address, default 1
  --interval-ms <ms>    Poll interval, default 2000
  --timeout-ms <ms>     Per-request timeout, default 1200
  --once                Poll once and exit
  --quiet               Do not print raw TX/RX bytes
  --help                Show help
```

Examples:

```bash
sen0658_poll --port COM9 --once
sen0658_poll --port /dev/ttyUSB0 --baud 4800 --slave 1
```

## GitHub Actions artifacts

The workflow in `.github/workflows/build.yml` builds and uploads:

- `sen0658-linux-amd64`
- `sen0658-linux-arm64`
- `sen0658-windows-amd64`

The Linux ARM64 job uses the native GitHub-hosted `ubuntu-24.04-arm` runner.
