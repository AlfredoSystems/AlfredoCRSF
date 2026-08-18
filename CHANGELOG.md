# Changelog

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
