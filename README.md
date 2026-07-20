# AlfredoCRSF

An Arduino library for the CRSF (Crossfire) serial protocol. Talk to an
ExpressLRS or TBS Crossfire receiver from a microcontroller: read stick and
switch positions, monitor link quality, and send telemetry back to the handset.

Originally based on CapnBry's CRSF code, restructured as a standard Arduino
library and extended with support for many more packet types.

## Features

- **Receive** RC channels, link statistics, and telemetry
- **Send** telemetry: battery, GPS, GPS time, vario, barometric altitude,
  attitude, airspeed
- **Link state** tracking with a failsafe timeout, plus commanded arm state
- **ELRS 4.0 support**: millivolt cell voltages, RPM, temperature, airspeed,
  GPS time, ELRS status frames, the channels arming status byte, and CRSF
  router participation (heartbeat and device discovery)
- **Works with both ELRS 3.x and 4.x.** Newer frame types are decoded when
  they arrive and simply never appear on an older link, so the same sketch
  runs on either.

There are no packet callbacks. Call `update()` in your loop and read the
latest values from the getters whenever you need them.

## Hardware requirements

Designed for the ESP32. CRSF runs at up to 420000 baud, so it needs a hardware
serial peripheral; software serial will not keep up.

Two serial peripherals are strongly preferred: leave the default `Serial` for
printing and debugging, and use a second high speed peripheral for CRSF.

Other MCUs such as the ATmega32U4, RP2040 and STM32 should work but are
untested. Avoid weak MCUs like the ATmega328P.

### Wiring

Connect the receiver's TX pad to your MCU's RX pin and the receiver's RX pad to
your MCU's TX pin, and give them a common ground. Pick pins that are actually
free on your board: on the ESP32-S3 avoid GPIO 19 and 20 (native USB), 26-32
(flash) and 33-37 (octal PSRAM); on the classic ESP32 avoid GPIO 6-11 (flash),
and note that 34-39 are input only.

## Installation

Search for "AlfredoCRSF" in the Arduino IDE Library Manager, or clone this
repository into your Arduino `libraries` folder.

## Quick start

```cpp
#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 18
#define PIN_TX 17

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

void setup()
{
  Serial.begin(115200);
  crsfSerial.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX, PIN_TX);
  crsf.begin(crsfSerial);
}

void loop()
{
  crsf.update();   // must be called regularly

  if (crsf.isLinkUp())
  {
    Serial.print("throttle: ");
    Serial.println(crsf.getChannel(3));   // channels are 1 based, value in us
  }
}
```

## API

### Setup

| Method | Description |
| --- | --- |
| `begin(port, deviceAddr)` | Start on a stream. `deviceAddr` defaults to `CRSF_ADDRESS_FLIGHT_CONTROLLER`; pass `CRSF_ADDRESS_RADIO_TRANSMITTER` when acting as a handset |
| `update()` | Process incoming bytes. Call this often from `loop()` |

### Channels and link

| Method | Description |
| --- | --- |
| `getChannel(ch)` | Channel value in microseconds, 1 based |
| `getChannelsPacked()` | The raw packed channels struct, for forwarding |
| `isLinkUp()` | False once no channels packet has arrived for 300 ms |
| `getLinkStatistics()` | RSSI, link quality, SNR, TX power |
| `isArmed()` | Commanded arm state. Uses the ELRS 4.0 status byte when present, otherwise the channel 5 position |
| `hasChannelsStatus()` / `getChannelsStatus()` | Whether the ELRS 4.0 status byte was present, and its raw bits |

### Telemetry getters

Each returns a pointer to the most recently received value. Sensors that never
arrive stay zeroed.

| Method | Frame |
| --- | --- |
| `getGpsSensor()` | GPS position, speed, heading, altitude |
| `getGpsTimeSensor()` | GPS date and time |
| `getVarioSensor()` | Vertical speed |
| `getBaroAltitudeSensor()` | Barometric altitude |
| `getAttitudeSensor()` | Pitch, roll, yaw |
| `getAirspeedSensor()` | Airspeed |
| `getRpmSensor()` | Up to 19 RPM values |
| `getTempSensor()` | Up to 20 temperatures |
| `getCellsSensor()` | Up to 29 cell voltages in millivolts |
| `getElrsStatus()` | ELRS packet counts, warning flags and message |
| `getHandsetTiming()` | Frame rate a TX module is asking a handset for |

### Sending

| Method | Description |
| --- | --- |
| `queuePacket(addr, type, payload, len)` | Send a frame, dropped if the link is down |
| `writePacket(addr, type, payload, len)` | Send a frame unconditionally |
| `writeChannels(addr, channels)` | Send packed channels |
| `writeChannels(addr, channels, status)` | Send packed channels with the ELRS 4.0 arming status byte. **ELRS 4.0+ modules only** |
| `writeExtPacket(type, destAddr, payload, len)` | Send an extended header frame from this device |
| `sendHeartbeat()` | Announce this device for CRSF router discovery |
| `setDeviceName(name)` | Answer device discovery pings with this name |

Build telemetry payloads with the structs in
[`crsf_protocol.h`](src/crsf_protocol.h) and the `htobe16` / `htobe24` /
`htobe32` helpers, since all multi-byte fields are big endian. See the
telemetry examples for the pattern.

## Examples

| Example | What it does |
| --- | --- |
| `printAllChannels` | Print all 16 channels. Start here |
| `linkStatusLed` | Drive an LED from link state |
| `sendTelemetryBattery` | Measure a voltage divider and report battery telemetry |
| `sendTelemetryGpsBaroVarioAttitude` | Send GPS, GPS time, altitude, vario and attitude |
| `forwardChannelsToFC` | Two receivers with failover, forwarding channels onward |
| `forwardPacketsFromHandset` | Read packets on the handset side of the link |
| `elrs4SelfTest` | Functional self test needing no radio: cross wire two UARTs and check every parser and sender |
| `elrs4ReceiverTest` | Bench test a receiver: channels, link stats and all telemetry types |
| `handsetEmulator` | Drive a TX module the way a handset does, including arm state and frame pacing |

## Compatibility notes

- **Model match.** If a receiver binds and shows connected but sends nothing at
  all over serial, check the model match setting on your handset. A mismatched
  model ID makes the receiver suppress its entire serial output, which looks
  exactly like a wiring fault.
- **ELRS 3.x receivers stay silent until they have a radio link**, so bench
  testing needs the transmitter powered and bound. ELRS 4.0 receivers answer
  device discovery pings without a link.
- **The arming status byte** on channel frames is ELRS 4.0 with EdgeTX 2.11 or
  newer. Sending it to a 3.x transmitter module produces a frame it does not
  understand.
- **GPS time** forwarding to the handset needs ELRS 4.1+.

## Protocol specification

The wire format, frame types and payload layouts are documented in
[CRSF_PROTOCOL.md](CRSF_PROTOCOL.md).

## References

- [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS) and its
  [documentation](https://www.expresslrs.org/)
- [EdgeTX](https://github.com/EdgeTX/edgetx)
- [TBS CRSF specification](https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md)
- [CapnBry's CRSF work](https://github.com/CapnBry/CRServoF), the origin of this library

## License

GPL-3.0. See [LICENSE](LICENSE).
