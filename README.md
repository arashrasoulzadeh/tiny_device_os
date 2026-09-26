# ArdubotOS

Host SDL2 emulator first, then the same launcher on a board (NodeMCU / ESP8266).

Wi-Fi name and password live in `device_secrets.yaml` (gitignored). Copy the example and edit both values before flashing:

```bash
cp device_secrets.yaml.example device_secrets.yaml
```

```yaml
wifi:
  ssid: "your-network"
  password: "your-password"
```

The status bar shows a Wi-Fi glyph, signal bars, and battery. Without that file the icons show Wi-Fi off. On the board, the bars follow the live signal. The emulator shows the radio as associated when the file is filled in.

## Emulator

```bash
make run
```

That configures a Debug sim build, then opens the SDL window (`build/sim/Debug/sim/ardubot-sim`).

Headless tests:

```bash
make test
```

The same binary from a CMake build directory:

```bash
cmake -B build -DARDUBOT_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/sim/ardubot-sim --flash-image=flash.img --sd-image=sd.img
```

Panel controls match the board: Up moves, Select launches, hold Select to go back.

## On device

`device_config.yaml` is the board profile (serial port, LCD, buttons). `device_secrets.yaml` is required for `make usb` on hardware.

```bash
make usb-ports
make usb DEVICE=nodemcu
```

`make usb` with no `DEVICE` asks which target to compile. Override the serial port with `PORT=/dev/cu.usbserial-0001` (or leave `device.port: auto`).

```bash
make device-config
make usb DEVICE=nodemcu PORT=/dev/cu.wchusbserial1410
```

Apple Silicon needs Rosetta for the ESP8266 toolchain (`Bad CPU type in executable`):

```bash
softwareupdate --install-rosetta --agree-to-license
```
