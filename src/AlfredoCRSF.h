#pragma once

#include <Arduino.h>
#include <crc8.h>
#include <crsf_protocol.h>

enum eFailsafeAction { fsaNoPulses, fsaHold };

class AlfredoCRSF
{
public:
    // Packet timeout where buffer is flushed if no data is received in this time
    static const unsigned int CRSF_PACKET_TIMEOUT_MS = 100;
    static const unsigned int CRSF_FAILSAFE_STAGE1_MS = 300;

    AlfredoCRSF();
    // deviceAddr is this device's own CRSF address, used for extended header
    // frames that are addressed to a specific device (e.g. device discovery
    // pings). Pass CRSF_ADDRESS_RADIO_TRANSMITTER when acting as a handset.
    void begin(Stream& port, uint8_t deviceAddr = CRSF_ADDRESS_FLIGHT_CONTROLLER);
    void update();
    void write(uint8_t b);
    void write(const uint8_t *buf, size_t len);
    void queuePacket(uint8_t addr, uint8_t type, const void *payload, uint8_t len);
    void writePacket(uint8_t addr, uint8_t type, const void *payload, uint8_t len);

    // Send a packed channels frame. addr is the leading byte: use CRSF_SYNC_BYTE
    // when sending to a flight controller, CRSF_ADDRESS_CRSF_TRANSMITTER when
    // sending to a TX module as a handset would.
    void writeChannels(uint8_t addr, const crsf_channels_t *channels);
    // ELRS 4.0+ TX modules only: also appends the channels status byte
    // (CRSF_CHANNELS_STATUS_* bits) carrying the commanded arm state for Arm
    // using Switch mode. ELRS 3.x modules do not understand the longer frame,
    // so only use this against a 4.0+ module with Switch arming selected.
    void writeChannels(uint8_t addr, const crsf_channels_t *channels, uint8_t status);

    // Return current channel value (1-based) in us
    int getChannel(unsigned int ch) const { return _channels[ch - 1]; }
    const crsf_channels_t *getChannelsPacked() const { return &_channelsPacked;}
    const crsfLinkStatistics_t *getLinkStatistics() const { return &_linkStatistics; }
    const crsf_sensor_gps_t *getGpsSensor() const { return &_gpsSensor; }
    const crsf_sensor_gps_time_t *getGpsTimeSensor() const { return &_gpsTimeSensor; }
    const crsf_sensor_vario_t *getVarioSensor() const { return &_varioSensor; }
    const crsf_sensor_baro_altitude_t *getBaroAltitudeSensor() const { return &_baroAltitudeSensor; }
    const crsf_sensor_attitude_t *getAttitudeSensor() const { return &_attitudeSensor; }
    const crsf_sensor_airspeed_t *getAirspeedSensor() const { return &_airspeedSensor; }
    const crsf_sensor_rpm_t *getRpmSensor() const { return &_rpmSensor; }
    const crsf_sensor_temp_t *getTempSensor() const { return &_tempSensor; }
    const crsf_sensor_cells_t *getCellsSensor() const { return &_cellsSensor; }
    const crsf_elrs_status_t *getElrsStatus() const { return &_elrsStatus; }
    bool isLinkUp() const { return _linkIsUp; }

    // ELRS 4.0+ (EdgeTX 2.11+) appends an optional status byte to channels
    // packets from the handset (see CRSF_CHANNELS_STATUS_* bits)
    bool hasChannelsStatus() const { return _hasChannelsStatus; }
    uint8_t getChannelsStatus() const { return _channelsStatus; }
    // Commanded arm state, mirroring ELRS logic: uses the status byte in Arm
    // using Switch mode, otherwise falls back to channel 5 (AUX1) position
    bool isArmed() const;

private:
    Stream* _port;
    uint8_t _deviceAddr;
    uint8_t _rxBuf[CRSF_MAX_PACKET_LEN+3];
    uint8_t _rxBufPos;
    Crc8 _crc;
    crsf_channels_t _channelsPacked;
    crsfLinkStatistics_t _linkStatistics;
    crsf_sensor_gps_t _gpsSensor;
    crsf_sensor_gps_time_t _gpsTimeSensor;
    crsf_sensor_vario_t _varioSensor;
    crsf_sensor_baro_altitude_t _baroAltitudeSensor;
    crsf_sensor_attitude_t _attitudeSensor;
    crsf_sensor_airspeed_t _airspeedSensor;
    crsf_sensor_rpm_t _rpmSensor;
    crsf_sensor_temp_t _tempSensor;
    crsf_sensor_cells_t _cellsSensor;
    crsf_elrs_status_t _elrsStatus;
    uint32_t _baud;
    uint32_t _lastReceive;
    uint32_t _lastChannelsPacket;
    bool _linkIsUp;
    bool _hasChannelsStatus;
    uint8_t _channelsStatus;
    int _channels[CRSF_NUM_CHANNELS];

    void handleSerialIn();
    void handleByteReceived();
    void shiftRxBuffer(uint8_t cnt);
    void processPacketIn();
    void checkPacketTimeout();
    void checkLinkDown();

    // Packet RX Handlers
    void processTelemetryPacketIn(const crsf_header_t *p);
    void processExtendedPacketIn(const crsf_header_t *p);
    void packetChannelsPacked(const crsf_header_t *p);
    void packetLinkStatistics(const crsf_header_t *p);
    void packetGps(const crsf_header_t *p);
    void packetGpsTime(const crsf_header_t *p);
    void packetVario(const crsf_header_t *p);
    void packetBaroAltitude(const crsf_header_t *p);
    void packetAttitude(const crsf_header_t *p);
    void packetAirspeed(const crsf_header_t *p);
    void packetRpm(const crsf_header_t *p);
    void packetTemp(const crsf_header_t *p);
    void packetCells(const crsf_header_t *p);
    void packetElrsStatus(const crsf_header_t *p);
};
