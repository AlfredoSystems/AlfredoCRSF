# Changelog

## 2.5 - 2026-10-09

### Added

- **`CrsfSwitch`**: a channel the transmitter drives to 1000/1500/2000 us, read
  as `DOWN` / `MIDDLE` / `UP` with `is()`, `movedTo()` and `movedFrom()`.
  Covers three- and two-position switches and pairs of momentary buttons on
  one channel. Included by `AlfredoCRSF.h`.
- **`getAxis(ch, deadzone)`**: a stick channel as -1..1 with a dead zone.
- **`switchesAndButtons` example.**

## 2.4 - 2026-09-28

### Changed

- **Firmware upload examples moved to `examples/firmwareUpload/`.**
  `elrsPassthrough` and `serialBridge` now live there. The examples also have
  Rotini V3 and V4 pin presets.
- **`serialBridge` now needs Tools > USB Mode set to USB-OTG (TinyUSB).** In
  Hardware CDC mode the ESP32-S3 reboots itself when esptool resets its target,
  and that cannot be turned off. A compile error says so if the mode is wrong.

### Fixed

- **`elrsPassthrough` works on native USB boards such as the ESP32-S3.** Once an
  ESP8285 receiver is in its bootloader the relay switches to 74880 (new
  `BL_BAUD`), and the board goes straight back into the bridge after the
  Configurator reopens the port, which resets it. Needs Tools > USB Mode set to
  Hardware CDC and JTAG, checked at compile time.
- **`serialBridge` works with the ExpressLRS Configurator's UART method**, on
  the ESP32-S3 and now the S2. It follows the host's baud rate through esptool's
  switch to 460800 and no longer loses the target's replies. Optional
  `PIN_TARGET_EN` and `PIN_TARGET_BOOT` pins can put the target into download
  mode automatically.
- **`serialBridgeESP.md` rewritten** to match what actually works.

## 2.3 - 2026-09-26

### Added

- **ELRS parameter read and write (`enableModelMatch` example).** Read and
  change a device's settings over CRSF the way the ExpressLRS Lua menu does, for
  example to turn Model Match on from a DIY transmitter that has no Lua radio.
  Adds `pingDevices()`, `readParameter()`, `writeParameter()`, and the
  `onDeviceInfo()` and `onParameter()` callbacks. Parameter entries that arrive
  split across several chunks are reassembled automatically.
- **`serialBridge` companion tool.** Turns a USB native ESP32 (S2 or S3) into a
  USB to serial adapter for flashing a blank ESP8285 or ESP8266 through its ROM
  bootloader, which Betaflight passthrough and the CRSF `bl` command cannot
  reach because the chip has no firmware yet. `serialBridgeESP.md` writes up the
  setup that actually works.

## 2.2 — 2026-08-18

### Fixed

- **Telemetry examples no longer send every loop.** Sending a frame each loop
  pushes data faster than the ELRS link can carry it — that rate is set by the
  Telem Ratio — which backs up the serial buffer and inflates loop time. The
  telemetry examples now send on a `millis()` timer while still calling
  `update()` every loop.
- **Examples use non-flash GPIO pins.** GPIO 7 and 8 are wired to the internal
  SPI flash on the classic ESP32, so those examples received no serial data on
  that board. They now use GPIO 4 and 5, matching the other examples.

## 2.1 — 2026-07-21

### Added

- **Receiver firmware flashing (`elrsPassthrough` example).** Update the
  firmware on a connected ELRS receiver through the board using the ExpressLRS
  Configurator's Betaflight Passthrough method, or a manual `bl` command over a
  serial terminal. Adds `sendBootloaderCommand()` to put the receiver into its
  bootloader.
- **Standalone voltage telemetry (`sendVoltage()` in the
  `sendTelemetryRpmTempCells` example).** Report a millivolt-precision voltage
  as its own sensor, the way an ELRS 4.0 receiver reports its VBatt. Multiple
  indexes give independent voltage sensors, e.g. a receiver battery and an
  ignition battery.

## Earlier versions

2.0 added ExpressLRS 4.0 support alongside 3.x. See the git history for the
full record before this changelog was started.
