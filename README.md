# AlfredoCRSF

An Arduino library for the CRSF (Crossfire) serial protocol. Talk to an
ExpressLRS or TBS Crossfire receiver from a microcontroller: read stick and
switch positions, monitor link quality, and send telemetry back to the handset.

Originally based on CapnBry's CRSF code, restructured as a standard Arduino
library and extended with support for many more packet types.

## Features

The same sketch runs on both ELRS generations. Features that need a newer
firmware are simply inert on an older link, never broken: frames that do not
exist on 3.x never arrive, and their getters stay zeroed.

| Feature | ELRS 3.x | ELRS 4.x |
| --- | :---: | :---: |
| Receive RC channels | ✅ | ✅ |
| Link statistics (RSSI, LQ, SNR, TX power) | ✅ | ✅ |
| Link state tracking with failsafe timeout | ✅ | ✅ |
| Send telemetry: battery, GPS, vario, barometric altitude, attitude | ✅ | ✅ |
| Arm state from the channel 5 position | ✅ | ✅ |
| Handset timing sync (frame rate and phase) | ✅ | ✅ |
| Airspeed, RPM, temperature and cell voltage telemetry | ✅ | ✅ |
| Millivolt battery voltage reported by the receiver | ❌ | ✅ |
| Arm state from the channels status byte | ❌ | ✅ |
| ELRS status: packet counts, warning flags, messages | ❌ | ✅ |
| CRSF router participation: heartbeat and device discovery | ❌ | ✅ |
| GPS time forwarded to the handset clock | ❌ | ✅ |


There are no packet callbacks. Call `update()` in your loop and read the
latest values from the getters whenever you need them.

## Hardware requirements

Designed for the ESP32. CRSF runs at up to 420000 baud, so it needs a hardware
serial peripheral; software serial will not keep up.

Two serial peripherals are strongly preferred: leave the default `Serial` for
printing and debugging, and use a second high speed peripheral for CRSF.

Other MCUs such as the ATmega32U4, RP2040 and STM32 should work but are
untested. Avoid weak MCUs like the ATmega328P.

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

## Examples

| Example | ELRS 3.x | ELRS 4.x | What it does |
| --- | :---: | :---: | --- |
| `printAllChannels` | ✅ | ✅ | Print all 16 channels. Start here |
| `linkStatusLed` | ✅ | ✅ | Drive an LED from link state |
| `sendTelemetryBattery` | ✅ | ✅ | Measure a voltage divider and report battery telemetry |
| `sendTelemetryGpsBaroVarioAttitude` | ✅ | ✅ | Send GPS, altitude, vario and attitude. The GPS time packet needs 4.1+ |
| `forwardChannelsToFC` | ✅ | ✅ | Two receivers with failover, forwarding channels onward |
| `forwardPacketsFromHandset` | ✅ | ✅ | Read packets on the handset side of the link |
| `elrs4SelfTest` | ❌ | ✅ | Functional self test needing no radio at all: cross wire two UARTs and check every parser and sender |
| `elrs4ReceiverTest` | ❌ | ✅ | Bench test a receiver. On 3.x the newer telemetry sections stay silent, which is the compatibility check |
| `handsetEmulator` | ✅ | ✅ | Drive a TX module the way a handset does. Set `ARM_WITH_STATUS_BYTE` to 0 for 3.x |

## Compatibility notes

- **Model match.** If a receiver binds and shows connected but sends nothing at
  all over serial, check the model match setting on your handset. A mismatched
  model ID makes the receiver suppress its entire serial output, which looks
  exactly like a wiring fault.
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
