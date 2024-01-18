# AlfredoCRSF - CSRF serial protocol Arduino library

This repo is based on CapnBry's CRSF code, it has been modified to match the format of standard Arduino Library. Keywords and example files included. It has also now been extended to support more telemetry packet types. Check out the example files to learn more.

TODO:
* For now callbacks have been removed. May add them back or replace with a flag system to alert when packets come in.
* Improve battery telemetry example by using all 24 capacity bits. (currently just 16 bits are used)
* Lib supports BaroAltitude packets but EdgeTX seems to not be able to parse them if Altitude is included.
* GPS heading seems to have some overflow issues in EdgeTX.

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
* CRSF_FRAMETYPE_VARIO = 0x07,
* CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
* CRSF_FRAMETYPE_BARO_ALTITUDE = 0x09,
* CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
* CRSF_FRAMETYPE_OPENTX_SYNC = 0x10,
* CRSF_FRAMETYPE_RADIO_ID = 0x3A,
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
### CRSF_FRAMETYPE_HEARTBEAT = 0x0B
* uint8_t Origin Device address;
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
### CRSF_FRAMETYPE_RADIO_ID = 0x3A
* uint16_t radioAddress;         //should be 0xEA00?
* uint8_t timingCorrectionFrame; //should be 0x10?
* uint32_t update_interval;      //what is this?
* int32_t offset;                //what is this?
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
### CRSF_FRAMETYPE_LINK_RX_ID = 0x1C
* uint8_t rxRssiPercent; 
* uint8_t rxRfPower;  //should be signed int?
### CRSF_FRAMETYPE_LINK_TX_ID = 0x1D
* uint8_t txRssiPercent;
* uint8_t txRfPower;  //should be signed int?
* uint8_t txFps;
### CRSF_FRAMETYPE_ATTITUDE = 0x1E
* uint16_t pitch;  // pitch in radians, BigEndian
* uint16_t roll;   // roll in radians, BigEndian
* uint16_t yaw;    // yaw in radians, BigEndian
### CRSF_FRAMETYPE_FLIGHT_MODE = 0x21
* char[]; //Flight mode ( Null-terminated string )
// Extended Header Frames, range: 0x28 to 0x96
### CRSF_FRAMETYPE_DEVICE_PING = 0x28,
* ????
### CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
* ????
### CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
* ????
### CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
* ????
### CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
* ????
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
