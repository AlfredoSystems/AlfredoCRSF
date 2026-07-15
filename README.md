# AlfredoCRSF - CSRF serial protocol Arduino library

This library is based on CapnBry's CRSF code, it has been modified to match the format of standard Arduino Library. Keywords and example files included. It has also now been extended to support more telemetry packet types. Check out the example files to learn more.

This library was designed for ELRS but should be compatible with any CRSF receiver.

TODO:
* For now callbacks have been removed. May add them back or replace with a flag system to alert when packets come in.
* Improve battery telemetry example by using all 24 capacity bits. (currently just 16 bits are used)
* Lib supports BaroAltitude packets but EdgeTX seems to not be able to parse them if Altitude is included.
* GPS heading seems to have some overflow issues in EdgeTX.

# Hardware requirements

This library is designed for ESP32. CRSF works best when you can access Serial Hardware peripherals that can achieve high baudrates (up to 420000). At least two serial peripherals are preferred, it is best to leave an MCUs default serial peripherals (the Serial object) for printing and debugging, and a second high speed peripheral for CRSF.

This library should work on other MCUS like ATmega32U4/RP2040/STM32 but these are untested, attempt at your own risk. Avoid weak MCUs like atmega328p.

# CRSF protocol specification

The CRSF protocol is not documented or maintained by one single entity. The following specification has been cobbled together from the ELRS, EdgeTX and OpenTX project codebases.

## Packet Format
`[dest] [len] [type] [payload] [crc8]`

### DEST - Destination address or "sync" byte
* CRSF_ADDRESS_CRSF_TRANSMITTER = (0xEE)   //Going to the transmitter module
* CRSF_ADDRESS_RADIO_TRANSMITTER  = (0xEA) //Going to the handset
* CRSF_ADDRESS_FLIGHT_CONTROLLER = (0xC8)  //Going to the flight controller
* CRSF_ADDRESS_CRSF_RECEIVER = (0xEC)      //Going to the receiver (from FC)

### LEN - Length of bytes that follow
Overall packet length is PayloadLength+4 (dest, len, type, crc), or LEN+2 (dest, len).

### TYPE - CRSF_FRAMETYPE
* CRSF_FRAMETYPE_GPS = 0x02,
* CRSF_FRAMETYPE_GPS_TIME = 0x03,
* CRSF_FRAMETYPE_VARIO = 0x07,
* CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
* CRSF_FRAMETYPE_BARO_ALTITUDE = 0x09,
* CRSF_FRAMETYPE_AIRSPEED = 0x0A,
* CRSF_FRAMETYPE_HEARTBEAT = 0x0B,
* CRSF_FRAMETYPE_RPM = 0x0C,
* CRSF_FRAMETYPE_TEMP = 0x0D,
* CRSF_FRAMETYPE_CELLS = 0x0E,
* CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
* CRSF_FRAMETYPE_OPENTX_SYNC = 0x10,
* CRSF_FRAMETYPE_HANDSET = 0x3A, // named RADIO_ID in older firmwares
* CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,
* CRSF_FRAMETYPE_LINK_RX_ID = 0x1C,
* CRSF_FRAMETYPE_LINK_TX_ID = 0x1D,
* CRSF_FRAMETYPE_ATTITUDE = 0x1E,
* CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
// Extended Header Frames, range: 0x28 to 0x96
* CRSF_FRAMETYPE_DEVICE_PING = 0x28,
* CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
* CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
* CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
* CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
* CRSF_FRAMETYPE_ELRS_STATUS = 0x2E,
* CRSF_FRAMETYPE_COMMAND = 0x32,
// KISS frames
* CRSF_FRAMETYPE_KISS_REQ  = 0x78,
* CRSF_FRAMETYPE_KISS_RESP = 0x79,
// MSP commands
* CRSF_FRAMETYPE_MSP_REQ = 0x7A,
* CRSF_FRAMETYPE_MSP_RESP = 0x7B,
* CRSF_FRAMETYPE_MSP_WRITE = 0x7C,
// Ardupilot frames
* CRSF_FRAMETYPE_ARDUPILOT_RESP = 0x80,

### CRC - CRC8 using poly 0xD5
Includes all bytes from type (buffer[2]) to end of payload.

## Payload of each frametype
### CRSF_FRAMETYPE_GPS = 0x02
* int32_t latitude;      // degree / 10,000,000 big endian
* int32_t longitude;     // degree / 10,000,000 big endian
* uint16_t groundspeed;  // km/h / 10 big endian
* uint16_t heading;      // GPS heading, degree/100 big endian
* uint16_t altitude;     // meters, +1000m big endian
* uint8_t satellites;    // satellites
### CRSF_FRAMETYPE_GPS_TIME = 0x03
Used to synchronize the handset clock (sent by e.g. Betaflight 2026.06+, requires ELRS 4.1+ to pass through).
* int16_t year;          // BigEndian
* uint8_t month;
* uint8_t day;
* uint8_t hour;
* uint8_t minute;
* uint8_t second;
* uint16_t millisecond;  // BigEndian
### CRSF_FRAMETYPE_VARIO = 0x07
* int16_t verticalspd;   // Vertical speed in cm/s, BigEndian
### CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08
* unsigned voltage : 16;  // V * 10 big endian
* unsigned current : 16;  // A * 10 big endian
* unsigned capacity : 24; // mah big endian
* unsigned remaining : 8; // %
### CRSF_FRAMETYPE_BARO_ALTITUDE = 0x09
* uint16_t altitude;     // Altitude in decimeters + 10000dm, or Altitude in meters if high bit is set, BigEndian
* int16_t verticalspd;   // Vertical speed in cm/s, BigEndian
### CRSF_FRAMETYPE_AIRSPEED = 0x0A
* uint16_t speed;        // Airspeed in 0.1 * km/h (hectometers/h), BigEndian
### CRSF_FRAMETYPE_HEARTBEAT = 0x0B
* int16_t Origin Device address; // BigEndian (used for device discovery by the ELRS 4.0 CRSF router)
### CRSF_FRAMETYPE_RPM = 0x0C
Variable length, count of values determined by frame length.
* uint8_t source_id;     // e.g. 0 = Motor 1, 1 = Motor 2, etc.
* int24_t rpm[1-19];     // Signed 24-bit RPM values BigEndian, negative = reverse
### CRSF_FRAMETYPE_TEMP = 0x0D
Variable length, count of values determined by frame length.
* uint8_t source_id;     // e.g. 0 = FC including all ESCs, 1 = Ambient, etc.
* int16_t temperature[1-20]; // Deci-degrees Celsius BigEndian (250 = 25.0C)
### CRSF_FRAMETYPE_CELLS = 0x0E
Variable length, count of values determined by frame length. ELRS 4.0+ receivers with VBAT sensing send this with source_id 128 for millivolt-precision voltage.
* uint8_t source_id;     // e.g. 0 = battery 1, 1 = battery 2, etc.
* uint16_t cell[1-29];   // Cell voltage in millivolts BigEndian (3850 = 3.850V)
### CRSF_FRAMETYPE_VIDEO_TRANSMITTER = 0x0F
* uint8_t Origin address;
* uint8_t Status;
* uint8_t Band_Channel;
* uint16_t User_Frequency;
* uint8_t PitMode_and_Power;
### CRSF_FRAMETYPE_LINK_STATISTICS = 0x14
* uint8_t uplink_RSSI_1;
* uint8_t uplink_RSSI_2;
* uint8_t uplink_Link_quality;
* int8_t uplink_SNR;
* uint8_t active_antenna;
* uint8_t rf_Mode;
* uint8_t uplink_TX_Power;
* uint8_t downlink_RSSI;
* uint8_t downlink_Link_quality;
* int8_t downlink_SNR;
### CRSF_FRAMETYPE_OPENTX_SYNC = 0x10
* ????
### CRSF_FRAMETYPE_HANDSET = 0x3A (named RADIO_ID in older firmwares)
Extended header frame (payload preceded by destination and origin address bytes). Sent by a TX module to the handset. The first payload byte is a subcommand; subcommand 0x10 is timing sync:
* uint8_t subCommand;    // 0x10 = timing sync
* uint32_t rate;         // requested channels packet interval in 0.1us units, BigEndian
* int32_t offset;        // timing offset correction in 0.1us units, BigEndian
### CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16
* unsigned ch0 : 11;
* unsigned ch1 : 11;
* unsigned ch2 : 11;
* unsigned ch3 : 11;
* unsigned ch4 : 11;
* unsigned ch5 : 11;
* unsigned ch6 : 11;
* unsigned ch7 : 11;
* unsigned ch8 : 11;
* unsigned ch9 : 11;
* unsigned ch10 : 11;
* unsigned ch11 : 11;
* unsigned ch12 : 11;
* unsigned ch13 : 11;
* unsigned ch14 : 11;
* unsigned ch15 : 11;

ELRS 4.0+ handsets (EdgeTX 2.11+) may append one status byte after the packed channels:
* bit 0: CRSF_CHANNELS_STATUS_ARMED - commanded armed status in Arm using Switch mode
* bit 1: CRSF_CHANNELS_STATUS_ARMING_MODE_CH5 - arm via CH5 instead of the armed bit
### CRSF_FRAMETYPE_LINK_RX_ID = 0x1C
* uint8_t rxRssiPercent; 
* uint8_t rxRfPower;  //should be signed int?
### CRSF_FRAMETYPE_LINK_TX_ID = 0x1D
* uint8_t txRssiPercent;
* uint8_t txRfPower;  //should be signed int?
* uint8_t txFps;
### CRSF_FRAMETYPE_ATTITUDE = 0x1E
* int16_t pitch;  // pitch in radians * 10000, BigEndian
* int16_t roll;   // roll in radians * 10000, BigEndian
* int16_t yaw;    // yaw in radians * 10000, BigEndian
### CRSF_FRAMETYPE_FLIGHT_MODE = 0x21
* char[]; //Flight mode ( Null-terminated string )
// Extended Header Frames, range: 0x28 to 0x96
### CRSF_FRAMETYPE_DEVICE_PING = 0x28,
Extended header frame (payload preceded by destination and origin address bytes). Device discovery request, usually sent to the broadcast address; each device answers with DEVICE_INFO.
* (no payload)
### CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
Extended header frame. Device discovery response.
* char name[];          // Device name (null-terminated string)
* uint32_t serialNo;    // BigEndian
* uint32_t hardwareVer; // BigEndian
* uint32_t softwareVer; // BigEndian
* uint8_t fieldCnt;     // number of configuration parameters this device has
* uint8_t parameterVersion;
### CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
* ????
### CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
* ????
### CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
* ????
### CRSF_FRAMETYPE_ELRS_STATUS = 0x2E,
Extended header frame (payload preceded by destination and origin address bytes). Sent by an ELRS TX module to the handset.
* uint8_t pktsBad;
* uint16_t pktsGood; // BigEndian
* uint8_t flags;     // bit 0: connected, bit 2: model mismatch warning, bit 3: armed warning, bit 5: error - change blocked while connected, bit 6: error - baud rate too low
* char msg[];        // Warning message (null-terminated string)
### CRSF_FRAMETYPE_COMMAND = 0x32,
* ????
// KISS frames
### CRSF_FRAMETYPE_KISS_REQ  = 0x78,
* ????
### CRSF_FRAMETYPE_KISS_RESP = 0x79,
* ????
// MSP commands
### CRSF_FRAMETYPE_MSP_REQ = 0x7A,
* ????
### CRSF_FRAMETYPE_MSP_RESP = 0x7B,
* ????
### CRSF_FRAMETYPE_MSP_WRITE = 0x7C,
* ????
