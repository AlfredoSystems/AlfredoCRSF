# CRSF protocol specification

The CRSF (Crossfire) protocol is not documented or maintained by any single
entity. This specification has been assembled from the ExpressLRS, EdgeTX and
OpenTX codebases, cross-checked against the TBS specification. Where firmwares
disagree, the behaviour described here is the one this library implements.

See [References](#references) at the bottom for the upstream sources.

## Packet format

```
[sync] [len] [type] [payload] [crc8]
```

| Field | Size | Description |
| --- | --- | --- |
| sync | 1 | Sync byte, see below |
| len | 1 | Number of bytes that follow, i.e. type + payload + crc |
| type | 1 | Frame type, see [Frame types](#frame-types) |
| payload | len - 2 | Frame specific, see [Payloads](#payloads) |
| crc8 | 1 | CRC over type and payload |

Total packet length is `len + 2`, or payload length + 4.

### Sync byte

The first byte is a **sync marker, not routing information**. On a serial link
it is always `0xC8`, which collides with `CRSF_ADDRESS_FLIGHT_CONTROLLER`
because that address doubles as the sync value. On handset links you will also
see `0xEE` and `0xEA`.

Do not infer the destination of a frame from this byte. Standard frames are
identified purely by their type, and frames that genuinely need addressing use
[extended header frames](#extended-header-frames).

### CRC

CRC8 with polynomial `0xD5`, initial value 0, covering all bytes from the type
byte through the end of the payload. It does not include the sync or length
bytes.

### Addresses

| Address | Value | Device |
| --- | --- | --- |
| `CRSF_ADDRESS_BROADCAST` | 0x00 | All devices |
| `CRSF_ADDRESS_USB` | 0x10 | USB |
| `CRSF_ADDRESS_BLUETOOTH_WIFI` | 0x12 | Bluetooth or WiFi link |
| `CRSF_ADDRESS_TBS_CORE_PNP_PRO` | 0x80 | TBS Core PNP Pro |
| `CRSF_ADDRESS_CURRENT_SENSOR` | 0xC0 | Current sensor |
| `CRSF_ADDRESS_GPS` | 0xC2 | GPS |
| `CRSF_ADDRESS_TBS_BLACKBOX` | 0xC4 | TBS Blackbox |
| `CRSF_ADDRESS_FLIGHT_CONTROLLER` | 0xC8 | Flight controller |
| `CRSF_ADDRESS_RACE_TAG` | 0xCC | Race tag |
| `CRSF_ADDRESS_RADIO_TRANSMITTER` | 0xEA | Handset |
| `CRSF_ADDRESS_CRSF_RECEIVER` | 0xEC | Receiver |
| `CRSF_ADDRESS_CRSF_TRANSMITTER` | 0xEE | Transmitter module |

### Extended header frames

Frame types in the range **0x28 to 0x96** carry two extra bytes at the start of
the payload:

```
[sync] [len] [type] [dest] [origin] [payload] [crc8]
```

`dest` is the address the frame is for and `origin` is the sender. A device
should act on a frame when `dest` is its own address or the broadcast address
`0x00`. This is how device discovery, parameter access and ELRS status frames
are routed between the handset, transmitter module, receiver and flight
controller.

## Frame types

| Type | ID | Direction | Notes |
| --- | --- | --- | --- |
| `GPS` | 0x02 | telemetry | |
| `GPS_TIME` | 0x03 | telemetry | Handset clock sync, ELRS 4.1+ |
| `VARIO` | 0x07 | telemetry | |
| `BATTERY_SENSOR` | 0x08 | telemetry | |
| `BARO_ALTITUDE` | 0x09 | telemetry | Optionally includes vertical speed |
| `AIRSPEED` | 0x0A | telemetry | |
| `HEARTBEAT` | 0x0B | any | Device discovery, ELRS 4.0+ |
| `RPM` | 0x0C | telemetry | Variable length |
| `TEMP` | 0x0D | telemetry | Variable length |
| `CELLS` | 0x0E | telemetry | Variable length |
| `VIDEO_TRANSMITTER` | 0x0F | to VTX | |
| `OPENTX_SYNC` | 0x10 | to handset | Legacy, superseded by `HANDSET` |
| `LINK_STATISTICS` | 0x14 | to FC | |
| `RC_CHANNELS_PACKED` | 0x16 | to FC / to TX | |
| `LINK_RX_ID` | 0x1C | telemetry | |
| `LINK_TX_ID` | 0x1D | telemetry | |
| `ATTITUDE` | 0x1E | telemetry | |
| `FLIGHT_MODE` | 0x21 | telemetry | |
| `DEVICE_PING` | 0x28 | extended | |
| `DEVICE_INFO` | 0x29 | extended | |
| `PARAMETER_SETTINGS_ENTRY` | 0x2B | extended | |
| `PARAMETER_READ` | 0x2C | extended | |
| `PARAMETER_WRITE` | 0x2D | extended | |
| `ELRS_STATUS` | 0x2E | extended | ELRS specific |
| `COMMAND` | 0x32 | extended | |
| `HANDSET` | 0x3A | extended | Named `RADIO_ID` in older firmwares |
| `KISS_REQ` | 0x78 | extended | |
| `KISS_RESP` | 0x79 | extended | |
| `MSP_REQ` | 0x7A | extended | |
| `MSP_RESP` | 0x7B | extended | |
| `MSP_WRITE` | 0x7C | extended | |
| `ARDUPILOT_RESP` | 0x80 | extended | |

All multi-byte values are big endian unless stated otherwise.

## Payloads

### GPS (0x02)

| Field | Type | Units |
| --- | --- | --- |
| latitude | int32 | degrees / 10,000,000 |
| longitude | int32 | degrees / 10,000,000 |
| groundspeed | uint16 | km/h / 10 |
| heading | uint16 | degrees / 100 |
| altitude | uint16 | metres + 1000 |
| satellites | uint8 | count |

Heading is degrees times 100, so a full 0-360 degrees maps to 0-36000. Sending
a larger scale factor overflows the field.

### GPS_TIME (0x03)

Synchronises the handset clock. Sent by a flight controller such as Betaflight
2026.06+, and requires ELRS 4.1+ to pass through to the handset.

| Field | Type | Units |
| --- | --- | --- |
| year | int16 | |
| month | uint8 | 1-12 |
| day | uint8 | 1-31 |
| hour | uint8 | 0-23 |
| minute | uint8 | 0-59 |
| second | uint8 | 0-59 |
| millisecond | uint16 | 0-999 |

### VARIO (0x07)

| Field | Type | Units |
| --- | --- | --- |
| verticalspd | int16 | cm/s |

### BATTERY_SENSOR (0x08)

| Field | Type | Units |
| --- | --- | --- |
| voltage | uint16 | volts * 10 |
| current | uint16 | amps * 10 |
| capacity | uint24 | mAh |
| remaining | uint8 | percent |

Capacity is a 24 bit field, so the maximum is 16,777,215 mAh.

### BARO_ALTITUDE (0x09)

| Field | Type | Units |
| --- | --- | --- |
| altitude | uint16 | decimetres + 10000, or metres if the high bit is set |
| verticalspd | int16 | cm/s, optional |

The receiving side decides what the frame contains from its declared length:
a 2 byte payload is altitude only, 3 bytes adds a TBS style single byte
vertical speed, and 4 bytes adds the ELRS style int16 vertical speed above.

### AIRSPEED (0x0A)

| Field | Type | Units |
| --- | --- | --- |
| speed | uint16 | km/h * 10 |

### HEARTBEAT (0x0B)

Announces a device so the CRSF router can discover it.

| Field | Type | Units |
| --- | --- | --- |
| origin | int16 | address of the sending device |

### RPM (0x0C)

Variable length: the number of values is derived from the frame length.

| Field | Type | Units |
| --- | --- | --- |
| source_id | uint8 | 0 = motor 1, 1 = motor 2, etc. |
| rpm | int24 x 1-19 | RPM, negative means reverse |

### TEMP (0x0D)

Variable length.

| Field | Type | Units |
| --- | --- | --- |
| source_id | uint8 | 0 = FC including ESCs, 1 = ambient, etc. |
| temperature | int16 x 1-20 | tenths of a degree Celsius |

### CELLS (0x0E)

Variable length. ELRS 4.0+ receivers with battery voltage sensing send this
with `source_id` 128 to report millivolt precision voltage.

| Field | Type | Units |
| --- | --- | --- |
| source_id | uint8 | 0 = battery 1, 1 = battery 2, etc. |
| cell | uint16 x 1-29 | millivolts |

### VIDEO_TRANSMITTER (0x0F)

| Field | Type |
| --- | --- |
| origin | uint8 |
| status | uint8 |
| band_channel | uint8 |
| user_frequency | uint16 |
| pitmode_and_power | uint8 |

### LINK_STATISTICS (0x14)

| Field | Type | Units |
| --- | --- | --- |
| uplink_RSSI_1 | uint8 | dBm * -1 |
| uplink_RSSI_2 | uint8 | dBm * -1 |
| uplink_Link_quality | uint8 | percent |
| uplink_SNR | int8 | dB |
| active_antenna | uint8 | 0 or 1 |
| rf_Mode | uint8 | packet rate index |
| uplink_TX_Power | uint8 | power index |
| downlink_RSSI | uint8 | dBm * -1 |
| downlink_Link_quality | uint8 | percent |
| downlink_SNR | int8 | dB |

### RC_CHANNELS_PACKED (0x16)

Sixteen channels packed into 11 bits each, 22 bytes total. Values are 172 to
1811 for -100% to +100%, with 992 as centre. With extended limits enabled the
usable range widens to 0 to 1984.

ELRS 4.0+ handsets running EdgeTX 2.11+ may append **one extra status byte**
after the channel data, making the payload 23 bytes:

| Bit | Name | Meaning |
| --- | --- | --- |
| 0 | `CRSF_CHANNELS_STATUS_ARMED` | Commanded arm state in "Arm using Switch" mode |
| 1 | `CRSF_CHANNELS_STATUS_ARMING_MODE_CH5` | Arm from the channel 5 value instead of bit 0 |

Receivers that predate this simply see a longer frame than they expect, so only
send the status byte to an ELRS 4.0+ transmitter module.

### LINK_RX_ID (0x1C)

| Field | Type | Units |
| --- | --- | --- |
| rxRssiPercent | uint8 | percent |
| rxRfPower | uint8 | power index |

### LINK_TX_ID (0x1D)

| Field | Type | Units |
| --- | --- | --- |
| txRssiPercent | uint8 | percent |
| txRfPower | uint8 | power index |
| txFps | uint8 | frames per second / 10 |

### ATTITUDE (0x1E)

| Field | Type | Units |
| --- | --- | --- |
| pitch | int16 | radians * 10000 |
| roll | int16 | radians * 10000 |
| yaw | int16 | radians * 10000 |

These are signed: negative angles are normal and must not be treated as
unsigned.

### FLIGHT_MODE (0x21)

| Field | Type |
| --- | --- |
| mode | null terminated string |

### DEVICE_PING (0x28)

Extended header frame with no payload. Device discovery request, usually sent
to the broadcast address. Every device answers with `DEVICE_INFO`.

Only ELRS 4.0+ receivers answer pings over the flight controller serial port.

### DEVICE_INFO (0x29)

Extended header frame. The response to a ping.

| Field | Type |
| --- | --- |
| name | null terminated string |
| serialNo | uint32 |
| hardwareVer | uint32 |
| softwareVer | uint32 |
| fieldCnt | uint8, number of configuration parameters |
| parameterVersion | uint8 |

### ELRS_STATUS (0x2E)

Extended header frame sent by an ELRS transmitter module to the handset.

| Field | Type |
| --- | --- |
| pktsBad | uint8 |
| pktsGood | uint16 |
| flags | uint8, see below |
| msg | null terminated warning string |

| Bit | Meaning |
| --- | --- |
| 0 | Connected |
| 2 | Model mismatch warning |
| 3 | Armed warning |
| 5 | Error: change blocked while connected |
| 6 | Error: baud rate too low |

### HANDSET (0x3A)

Extended header frame, named `RADIO_ID` in older firmwares. The first payload
byte is a subcommand. Subcommand `0x10` is timing sync, sent by a transmitter
module to tell the handset how fast to send channel frames:

| Field | Type | Units |
| --- | --- | --- |
| subCommand | uint8 | 0x10 for timing sync |
| rate | uint32 | requested packet interval, 0.1 us units |
| offset | int32 | phase correction, 0.1 us units |

### Undocumented frames

The payloads of `PARAMETER_SETTINGS_ENTRY` (0x2B), `PARAMETER_READ` (0x2C),
`PARAMETER_WRITE` (0x2D), `COMMAND` (0x32), the KISS frames (0x78, 0x79), the
MSP frames (0x7A to 0x7C) and `ARDUPILOT_RESP` (0x80) are not documented here.
This library does not decode them.

## References

- [ExpressLRS `crsf_protocol.h`](https://github.com/ExpressLRS/ExpressLRS/blob/master/src/include/crsf_protocol.h) - authoritative for ELRS frame definitions
- [EdgeTX `crossfire.cpp`](https://github.com/EdgeTX/edgetx/blob/main/radio/src/telemetry/crossfire.cpp) - authoritative for how telemetry is decoded and displayed
- [TBS CRSF specification](https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md) - the vendor specification
