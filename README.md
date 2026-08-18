# UltraHawk Basestation

Ground-station software for the UltraHawk drone. It connects to the ESP32
ground-station firmware over USB serial, displays vehicle telemetry, and sends
control commands back to the drone over ESP-NOW.

There are currently two clients in this repository:

- `app/` is the original native Qt desktop application.
- `web/` is the newer browser-based ground station. It is easier to try out
  and includes a simulator, so it does not need hardware for every UI change.

![UltraHawk ground station](images/view.png)

## Qt Desktop App

The Qt app is a C++17 application built with Qt 6 Widgets. It shows a primary
flight display, motor outputs, telemetry values, and a short attitude trend
graph. It currently opens `/dev/ttyACM0` at `115200` baud, so the device path
is Linux-specific and is set in `app/src/main.cpp`.

Build and run it with CMake:

```sh
cmake -S app -B app/build
cmake --build app/build
./app/build/UltraHawk
```

Qt 6 with the Widgets module and a C++17 compiler are required.

## Web Ground Station

The web client is a React and TypeScript app served by Vite. It can use either
the built-in simulator or Web Serial. The simulator is useful for checking the
flight display, keyboard controls, charts, and stale-telemetry behavior before
connecting a vehicle.

```sh
cd web
npm install
npm run dev
```

Open the URL printed by Vite in Chrome or Edge. Choose `SIMULATOR` to run
without hardware. Choose `SERIAL` to select a USB serial device and connect to
the firmware. Web Serial requires a Chromium-based desktop browser and a
secure context such as `localhost` or HTTPS.

To create a production build or run the tests:

```sh
cd web
npm run build
npm test
```

The web client currently reads the legacy `DP` telemetry lines and sends the
matching text command format. GPS, IMU fields, command acknowledgements, and
firmware failsafe behavior are not represented by that legacy protocol yet.

## Firmware

The firmware is an Arduino sketch for an ESP32 Nano (`arduino:esp32:nano_nora`).
It receives telemetry from the drone with ESP-NOW, prints the latest telemetry
over USB serial, and forwards commands from the ground station to the drone.

The sketch uses `115200` baud on USB serial. The helper scripts default to
`/dev/ttyACM0`; change `PORT` in `firmware/build.sh` or `firmware/serial.sh`
when the board appears at a different path.

The monitor commands in those scripts still default to `9600`; change their
`BAUD` value to `115200` before using the serial monitor.

With Arduino CLI installed and the ESP32 board package configured:

```sh
cd firmware
./build.sh
```

The drone MAC address is currently set directly in `firmware.ino` as
`droneAddress` and needs to match the vehicle before ESP-NOW communication will
work.

## Repository Layout

```text
app/       Native Qt ground station
firmware/  ESP32 Arduino sketch and upload scripts
web/       React/Web Serial ground station
images/    Screenshots used in this README
```
