# Changelog

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
