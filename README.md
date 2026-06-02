# SEN0658 PC Modbus Reader

C++17 PC-side Modbus-RTU test program for the DFRobot SEN0658 RS485 9-in-1 weather sensor.

It is meant for direct debugging with a USB serial adapter. It prints raw TX/RX bytes so you can see whether the FTDI adapter is actually transmitting and whether the sensor replies.

## Hardware

Ultrasonic 9-in-1 RS485 Weather Sensor for Arduino (Wind, Temp, Light, Noise)

from DFRobot.com
SKU: SEN0658

Product:
https://www.dfrobot.com/product-2942.html

Wiki:
https://wiki.dfrobot.com/sen0658/

Example Sources: 
https://wiki.dfrobot.com/sen0658/docs/21685

To input data to computers via USB serial you can use FTDI RS-422 to USB stick (alternative chipsets should work too)



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
  --interval-ms <ms>    Poll interval, default 2000 (500 with NMEA output)
  --rate-hz <hz>        Poll rate, alternative to --interval-ms; NMEA default 2 Hz
  --timeout-ms <ms>     Per-request timeout, default 1200
  --nmea                Emit NMEA 0183 standard/XDR sensor sentences at 2 Hz by default
  --nmea-tcp-port <p>   Listen on TCP port p and stream NMEA 0183 sentences
  --nmea-tcp-bind <a>   Bind TCP NMEA server to address a, default all interfaces
  --once                Poll once and exit (ignored by TCP NMEA server)
  --quiet               Do not print raw TX/RX bytes
  --help                Show help
```

Examples:

```bash
sen0658_poll --port COM9 --once
sen0658_poll --port /dev/ttyUSB0 --baud 4800 --slave 1
sen0658_poll --port /dev/ttyUSB0 --nmea
sen0658_poll --port /dev/ttyUSB0 --nmea-tcp-port 10110 --nmea-tcp-bind 127.0.0.1 --rate-hz 2
```

## NMEA 0183 output

Use `--nmea` to print NMEA 0183 standard meteorological and XDR transducer sentences instead of the human-readable report. In this mode the program suppresses raw Modbus TX/RX logging and polls every 500 ms by default, producing 2 Hz output unless `--interval-ms` is supplied.

Use `--nmea-tcp-port <p>` to run a TCP NMEA 0183 server instead of printing sentences to stdout. The server listens on all interfaces by default; add `--nmea-tcp-bind <address>` to restrict the bind address, for example `127.0.0.1`. TCP NMEA mode also suppresses raw Modbus TX/RX logging, polls continuously, and uses the same default 500 ms / 2 Hz rate unless `--interval-ms` or `--rate-hz` is supplied.

Each sensor sample emits checksummed standard sentences for the common weather channels, then XDR sentences for the remaining transducers:

- `$WIMWV`: wind direction as a true angle, wind speed in m/s, and status (`A` = valid, `V` = invalid).
- `$WIMDA`: barometric pressure in inches of mercury and bars, plus outside air temperature in Celsius.
- `$WIXDR`: wind sector, relative humidity, noise, particulate matter, and light transducer measurements when their Modbus blocks are valid.

Example:

```text
$WIMWV,110.0,T,0.14,M,A*13
$WIMDA,30.0921,I,1.0190,B,24.6,C,,C,,,,,,,,,,,,,,*73
$WIXDR,A,2,N,WIND_SECTOR*3A
$WIXDR,H,45.1,P,REL_HUMIDITY,G,51.8,D,NOISE_DB*5D
$WIXDR,G,5,UGM3,PM2_5,G,15,UGM3,PM10*38
$WIXDR,G,50,LX,LIGHT*58
```

## GitHub Actions artifacts

The workflow in `.github/workflows/build.yml` builds and uploads:

- `sen0658-linux-amd64`: versioned Linux tarball and Debian `.deb` package
- `sen0658-linux-arm64`: versioned Linux tarball and Debian `.deb` package
- `sen0658-windows-amd64`: versioned Windows zip package with `run.bat`

Tags named `v*` also publish the archives to a GitHub Release, generate a flat APT repository from the Debian packages, and update the stable `apt` release with the latest APT metadata. The Linux ARM64 job uses the native GitHub-hosted `ubuntu-24.04-arm` runner.

## Sample output (windows)

```
sen0658_poll --port COM3 --once
Opening COM3 at 4800 8N1
Polling Modbus slave 1
If the adapter TX LED never blinks, check COM port/driver first.
TX: 01 03 01 F4 00 04 04 07
RX: 01 03 01 F4 00 04 04 07 01 03 08 00 0E 00 01 00 02 00 6E 67 FB
TX: 01 03 01 F8 00 03 85 C6
RX: 01 03 01 F8 00 03 85 C6 01 03 06 01 C3 00 F6 02 06 05 E5
TX: 01 03 01 FB 00 03 75 C6
RX: 01 03 01 FB 00 03 75 C6 01 03 06 00 05 00 0F 03 FB 9C 05
TX: 01 03 01 FE 00 02 A4 07
RX: 01 03 01 FE 00 02 A4 07 01 03 04 00 00 00 32 7B E6

=== DFRobot SEN0658 OK ===
Wind speed:       0.14 m/s
Wind sector:      2
Wind direction:   110 deg
Temperature:      24.6 C
Humidity:         45.1 %RH
Noise:            51.8 dB
Pressure:         101.9 kPa
PM2.5:            5 ug/m3
PM10:             15 ug/m3
Light:            50 lux
============================
```
