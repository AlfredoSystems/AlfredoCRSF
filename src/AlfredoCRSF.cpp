#include <AlfredoCRSF.h>

AlfredoCRSF::AlfredoCRSF() :
    _deviceAddr(CRSF_ADDRESS_FLIGHT_CONTROLLER), _deviceName(NULL),
    _crc(0xd5),
    _lastReceive(0), _lastChannelsPacket(0), _linkIsUp(false),
    _hasChannelsStatus(false), _channelsStatus(0)
{

}

void AlfredoCRSF::begin(Stream &port, uint8_t deviceAddr)
{
  this->_port = &port;
  this->_deviceAddr = deviceAddr;
}

// Call from main loop to update
void AlfredoCRSF::update()
{
    handleSerialIn();
}

void AlfredoCRSF::handleSerialIn()
{
    while (_port->available())
    {
        uint8_t b = _port->read();
        _lastReceive = millis();

        _rxBuf[_rxBufPos++] = b;
        handleByteReceived();

        if (_rxBufPos >= sizeof(_rxBuf))
        {
            // Packet buffer filled and no valid packet found, dump the whole thing
            _rxBufPos = 0;
        }
    }

    checkPacketTimeout();
    checkLinkDown();
}

void AlfredoCRSF::handleByteReceived()
{
    bool reprocess;
    do
    {
        reprocess = false;
        if (_rxBufPos > 1)
        {
            uint8_t len = _rxBuf[1];
            // Sanity check the declared length, can't be shorter than Type, X, CRC
            if (len < 3 || len > CRSF_MAX_PACKET_LEN)
            {
                shiftRxBuffer(1);
                reprocess = true;
            }

            else if (_rxBufPos >= (len + 2))
            {
                uint8_t inCrc = _rxBuf[2 + len - 1];
                uint8_t crc = _crc.calc(&_rxBuf[2], len - 1);
                if (crc == inCrc)
                {
                    processPacketIn();
                    shiftRxBuffer(len + 2);
                    reprocess = true;
                }
                else
                {
                    shiftRxBuffer(1);
                    reprocess = true;
                }
            }  // if complete packet
        } // if pos > 1
    } while (reprocess);
}

void AlfredoCRSF::checkPacketTimeout()
{
    // If we haven't received data in a long time, flush the buffer a byte at a time (to trigger shiftyByte)
    if (_rxBufPos > 0 && millis() - _lastReceive > CRSF_PACKET_TIMEOUT_MS)
        while (_rxBufPos)
            shiftRxBuffer(1);
}

void AlfredoCRSF::checkLinkDown()
{
    if (_linkIsUp && millis() - _lastChannelsPacket > CRSF_FAILSAFE_STAGE1_MS)
    {
        _linkIsUp = false;
    }
}

// Byte 0 of a CRSF frame is a sync byte, not routing information: standard
// frames (type below 0x28) have meaning purely by their type, and extended
// frames (0x28-0x96) carry their routing in destination/origin header bytes
void AlfredoCRSF::processPacketIn()
{
    const crsf_header_t *hdr = (crsf_header_t *)_rxBuf;
    if (CRSF_IS_EXT_FRAMETYPE(hdr->type))
    {
        processExtendedPacketIn(hdr);
    }
    else if (hdr->type == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        packetChannelsPacked(hdr);
    }
    else
    {
        processTelemetryPacketIn(hdr);
    }
}

void AlfredoCRSF::processTelemetryPacketIn(const crsf_header_t *hdr)
{
    switch (hdr->type)
    {
    case CRSF_FRAMETYPE_GPS:
        packetGps(hdr);
        break;
    case CRSF_FRAMETYPE_GPS_TIME:
        packetGpsTime(hdr);
        break;
    case CRSF_FRAMETYPE_LINK_STATISTICS:
        packetLinkStatistics(hdr);
        break;
    case CRSF_FRAMETYPE_BARO_ALTITUDE:
        packetBaroAltitude(hdr);
        break;
    case CRSF_FRAMETYPE_VARIO:
        packetVario(hdr);
        break;
    case CRSF_FRAMETYPE_ATTITUDE:
        packetAttitude(hdr);
        break;
    case CRSF_FRAMETYPE_AIRSPEED:
        packetAirspeed(hdr);
        break;
    case CRSF_FRAMETYPE_RPM:
        packetRpm(hdr);
        break;
    case CRSF_FRAMETYPE_TEMP:
        packetTemp(hdr);
        break;
    case CRSF_FRAMETYPE_CELLS:
        packetCells(hdr);
        break;
    }
}

// Extended header frames. Status-carrying frames are decoded regardless of
// their destination; frames that require a response are only acted on when
// addressed to this device (or broadcast)
void AlfredoCRSF::processExtendedPacketIn(const crsf_header_t *hdr)
{
    if (hdr->frame_size < CRSF_FRAME_LENGTH_EXT_TYPE_CRC)
        return;
    switch (hdr->type)
    {
    case CRSF_FRAMETYPE_ELRS_STATUS:
        packetElrsStatus(hdr);
        break;
    case CRSF_FRAMETYPE_HANDSET:
        packetHandsetTiming(hdr);
        break;
    case CRSF_FRAMETYPE_DEVICE_PING:
    {
        const crsf_ext_header_t *ext = (const crsf_ext_header_t *)hdr;
        if (_deviceName && (ext->dest_addr == _deviceAddr || ext->dest_addr == CRSF_ADDRESS_BROADCAST))
            sendDeviceInfo(ext->orig_addr);
        break;
    }
    }
}

// Shift the bytes in the RxBuf down by cnt bytes
void AlfredoCRSF::shiftRxBuffer(uint8_t cnt)
{
    // If removing the whole thing, just set pos to 0
    if (cnt >= _rxBufPos)
    {
        _rxBufPos = 0;
        return;
    }

    // Otherwise do the slow shift down
    uint8_t *src = &_rxBuf[cnt];
    uint8_t *dst = &_rxBuf[0];
    _rxBufPos -= cnt;
    uint8_t left = _rxBufPos;
    while (left--)
        *dst++ = *src++;
}

void AlfredoCRSF::packetChannelsPacked(const crsf_header_t *p)
{
    crsf_channels_t *ch = (crsf_channels_t *)&p->data;
    _channels[0] = ch->ch0;
    _channels[1] = ch->ch1;
    _channels[2] = ch->ch2;
    _channels[3] = ch->ch3;
    _channels[4] = ch->ch4;
    _channels[5] = ch->ch5;
    _channels[6] = ch->ch6;
    _channels[7] = ch->ch7;
    _channels[8] = ch->ch8;
    _channels[9] = ch->ch9;
    _channels[10] = ch->ch10;
    _channels[11] = ch->ch11;
    _channels[12] = ch->ch12;
    _channels[13] = ch->ch13;
    _channels[14] = ch->ch14;
    _channels[15] = ch->ch15;

    for (unsigned int i=0; i<CRSF_NUM_CHANNELS; ++i)
        _channels[i] = map(_channels[i], CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_2000, 1000, 2000);

    // ELRS 4.0+ handsets (EdgeTX 2.11+) append an optional status byte after the packed channels
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_TYPE_CRC;
    if (payloadLen > sizeof(crsf_channels_t))
    {
        _channelsStatus = p->data[sizeof(crsf_channels_t)];
        _hasChannelsStatus = true;
    }
    else
    {
        _channelsStatus = 0;
        _hasChannelsStatus = false;
    }

    _linkIsUp = true;
    _lastChannelsPacket = millis();

    memcpy(&_channelsPacked, ch, sizeof(_channelsPacked));
}

bool AlfredoCRSF::isArmed() const
{
    if (!_linkIsUp)
        return false;
    // Status byte present and Arm using Switch selected: use the commanded arm bit.
    // Otherwise (no status byte, or Arm using CH5 selected): use channel 5 position.
    if (_hasChannelsStatus && !(_channelsStatus & CRSF_CHANNELS_STATUS_ARMING_MODE_CH5))
        return _channelsStatus & CRSF_CHANNELS_STATUS_ARMED;
    return getChannel(5) > 1500;
}

void AlfredoCRSF::packetLinkStatistics(const crsf_header_t *p)
{
    const crsfLinkStatistics_t *link = (crsfLinkStatistics_t *)p->data;
    memcpy(&_linkStatistics, link, sizeof(_linkStatistics));
}

void AlfredoCRSF::packetGps(const crsf_header_t *p)
{
    const crsf_sensor_gps_t *gps = (crsf_sensor_gps_t *)p->data;
    _gpsSensor.latitude = be32toh(gps->latitude);
    _gpsSensor.longitude = be32toh(gps->longitude);
    _gpsSensor.groundspeed = be16toh(gps->groundspeed);
    _gpsSensor.heading = be16toh(gps->heading);
    _gpsSensor.altitude = be16toh(gps->altitude);
    _gpsSensor.satellites = gps->satellites;
}

void AlfredoCRSF::packetVario(const crsf_header_t *p)
{
    const crsf_sensor_vario_t *vario = (crsf_sensor_vario_t *)p->data;
    _varioSensor.verticalspd = be16toh(vario->verticalspd);
}

void AlfredoCRSF::packetBaroAltitude(const crsf_header_t *p)
{
    const crsf_sensor_baro_altitude_t *baroAltitude = (crsf_sensor_baro_altitude_t *)p->data;
    _baroAltitudeSensor.altitude = be16toh(baroAltitude->altitude);
    _baroAltitudeSensor.verticalspd = be16toh(baroAltitude->verticalspd);
}

void AlfredoCRSF::packetAttitude(const crsf_header_t *p)
{
    const crsf_sensor_attitude_t *attitude = (crsf_sensor_attitude_t *)p->data;
    _attitudeSensor.pitch = be16toh(attitude->pitch);
    _attitudeSensor.roll = be16toh(attitude->roll);
    _attitudeSensor.yaw = be16toh(attitude->yaw);
}

void AlfredoCRSF::packetGpsTime(const crsf_header_t *p)
{
    const crsf_sensor_gps_time_t *gpsTime = (crsf_sensor_gps_time_t *)p->data;
    _gpsTimeSensor.year = be16toh(gpsTime->year);
    _gpsTimeSensor.month = gpsTime->month;
    _gpsTimeSensor.day = gpsTime->day;
    _gpsTimeSensor.hour = gpsTime->hour;
    _gpsTimeSensor.minute = gpsTime->minute;
    _gpsTimeSensor.second = gpsTime->second;
    _gpsTimeSensor.millisecond = be16toh(gpsTime->millisecond);
}

void AlfredoCRSF::packetAirspeed(const crsf_header_t *p)
{
    const crsf_sensor_airspeed_t *airspeed = (crsf_sensor_airspeed_t *)p->data;
    _airspeedSensor.speed = be16toh(airspeed->speed);
}

// RPM, TEMP and CELLS frames are variable length: source_id followed by
// 1-N values, where the count comes from the frame length
void AlfredoCRSF::packetRpm(const crsf_header_t *p)
{
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_TYPE_CRC;
    if (payloadLen < 1 + 3)
        return;
    _rpmSensor.source_id = p->data[0];
    uint8_t count = (payloadLen - 1) / 3;
    if (count > CRSF_MAX_RPM_VALUES)
        count = CRSF_MAX_RPM_VALUES;
    _rpmSensor.rpm_count = count;
    for (uint8_t i = 0; i < count; ++i)
    {
        const uint8_t *v = &p->data[1 + i * 3];
        // Signed 24-bit big endian
        int32_t rpm = ((int32_t)v[0] << 16) | ((int32_t)v[1] << 8) | v[2];
        if (rpm & 0x800000)
            rpm |= 0xFF000000;
        _rpmSensor.rpm[i] = rpm;
    }
}

void AlfredoCRSF::packetTemp(const crsf_header_t *p)
{
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_TYPE_CRC;
    if (payloadLen < 1 + 2)
        return;
    _tempSensor.source_id = p->data[0];
    uint8_t count = (payloadLen - 1) / 2;
    if (count > CRSF_MAX_TEMP_VALUES)
        count = CRSF_MAX_TEMP_VALUES;
    _tempSensor.temp_count = count;
    for (uint8_t i = 0; i < count; ++i)
    {
        const uint8_t *v = &p->data[1 + i * 2];
        _tempSensor.temperature[i] = (int16_t)(((uint16_t)v[0] << 8) | v[1]);
    }
}

// ELRS_STATUS is an extended header frame: two extended routing bytes
// (destination, origin) precede the payload
void AlfredoCRSF::packetElrsStatus(const crsf_header_t *p)
{
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_EXT_TYPE_CRC;
    if (payloadLen < 4)
        return;
    const uint8_t *payload = &p->data[2]; // skip extended dest/origin
    _elrsStatus.pktsBad = payload[0];
    _elrsStatus.pktsGood = ((uint16_t)payload[1] << 8) | payload[2];
    _elrsStatus.flags = payload[3];

    uint8_t msgLen = payloadLen - 4;
    if (msgLen > CRSF_ELRS_STATUS_MSG_LEN)
        msgLen = CRSF_ELRS_STATUS_MSG_LEN;
    memcpy(_elrsStatus.msg, &payload[4], msgLen);
    _elrsStatus.msg[msgLen] = '\0';
}

// HANDSET is an extended header frame; the payload is a subcommand byte
// followed by subcommand-specific data
void AlfredoCRSF::packetHandsetTiming(const crsf_header_t *p)
{
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_EXT_TYPE_CRC;
    if (payloadLen < 9)
        return;
    const uint8_t *payload = &p->data[2]; // skip extended dest/origin
    if (payload[0] != CRSF_HANDSET_SUBCMD_TIMING)
        return;
    _handsetTiming.rate = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
                          ((uint32_t)payload[3] << 8) | payload[4];
    _handsetTiming.offset = (int32_t)(((uint32_t)payload[5] << 24) | ((uint32_t)payload[6] << 16) |
                                      ((uint32_t)payload[7] << 8) | payload[8]);
}

void AlfredoCRSF::packetCells(const crsf_header_t *p)
{
    uint8_t payloadLen = p->frame_size - CRSF_FRAME_LENGTH_TYPE_CRC;
    if (payloadLen < 1 + 2)
        return;
    _cellsSensor.source_id = p->data[0];
    uint8_t count = (payloadLen - 1) / 2;
    if (count > CRSF_MAX_CELL_VALUES)
        count = CRSF_MAX_CELL_VALUES;
    _cellsSensor.cell_count = count;
    for (uint8_t i = 0; i < count; ++i)
    {
        const uint8_t *v = &p->data[1 + i * 2];
        _cellsSensor.cell[i] = ((uint16_t)v[0] << 8) | v[1];
    }
}

void AlfredoCRSF::write(uint8_t b)
{
    _port->write(b);
}

void AlfredoCRSF::write(const uint8_t *buf, size_t len)
{
    _port->write(buf, len);
}

void AlfredoCRSF::queuePacket(uint8_t addr, uint8_t type, const void *payload, uint8_t len)
{
    if (!_linkIsUp)
        return;
    if (len > CRSF_MAX_PACKET_LEN)
        return;
   
    uint8_t buf[CRSF_MAX_PACKET_LEN+4];
    buf[0] = addr;
    buf[1] = len + 2; // type + payload + crc
    buf[2] = type;
    memcpy(&buf[3], payload, len);
    buf[len+3] = _crc.calc(&buf[2], len + 1);
    write(buf, len + 4);
}

void AlfredoCRSF::writePacket(uint8_t addr, uint8_t type, const void *payload, uint8_t len)
{
    uint8_t buf[CRSF_MAX_PACKET_LEN+4];
    buf[0] = addr;
    buf[1] = len + 2; // type + payload + crc
    buf[2] = type;
    memcpy(&buf[3], payload, len);
    buf[len+3] = _crc.calc(&buf[2], len + 1);
    write(buf, len + 4);
}

void AlfredoCRSF::writeChannels(uint8_t addr, const crsf_channels_t *channels)
{
    writePacket(addr, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, channels, sizeof(crsf_channels_t));
}

void AlfredoCRSF::writeChannels(uint8_t addr, const crsf_channels_t *channels, uint8_t status)
{
    // ELRS 4.0 extended channels frame: the packed channels followed by one
    // status byte (CRSF_CHANNELS_STATUS_* bits)
    uint8_t payload[sizeof(crsf_channels_t) + 1];
    memcpy(payload, channels, sizeof(crsf_channels_t));
    payload[sizeof(crsf_channels_t)] = status;
    writePacket(addr, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, payload, sizeof(payload));
}

void AlfredoCRSF::writeExtPacket(uint8_t type, uint8_t destAddr, const void *payload, uint8_t len)
{
    if (len > CRSF_MAX_PACKET_LEN - 2)
        return;
    uint8_t buf[CRSF_MAX_PACKET_LEN];
    buf[0] = destAddr;
    buf[1] = _deviceAddr;
    memcpy(&buf[2], payload, len);
    writePacket(CRSF_SYNC_BYTE, type, buf, len + 2);
}

// Model select is a COMMAND frame, which is an extended header frame with an
// extra payload CRC before the frame CRC:
// [sync][len][type][dest][origin][command][subcommand][model id][crcBA][crc]
void AlfredoCRSF::sendModelId(uint8_t modelId)
{
    uint8_t buf[10];
    buf[0] = CRSF_SYNC_BYTE;
    buf[1] = 8; // type, dest, origin, command, subcommand, model id, both CRCs
    buf[2] = CRSF_FRAMETYPE_COMMAND;
    buf[3] = CRSF_ADDRESS_CRSF_TRANSMITTER; // to the transmitter module
    buf[4] = _deviceAddr;                   // from us, acting as the handset
    buf[5] = CRSF_COMMAND_SUBCMD_RX;
    buf[6] = CRSF_COMMAND_MODEL_SELECT_ID;
    buf[7] = modelId;
    // Command frames carry an extra CRC over the payload before the frame CRC
    buf[8] = Crc8::calcPoly(&buf[2], 6, CRSF_COMMAND_CRC_POLY);
    buf[9] = _crc.calc(&buf[2], 7);
    write(buf, sizeof(buf));
}

void AlfredoCRSF::sendHeartbeat()
{
    // Payload is the origin device address as a big endian int16
    uint8_t payload[2] = { 0, _deviceAddr };
    writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_HEARTBEAT, payload, sizeof(payload));
}

void AlfredoCRSF::setDeviceName(const char *name)
{
    _deviceName = name;
}

// DEVICE_INFO payload: null-terminated device name, then serial number,
// hardware and software version (uint32 big endian), field count and
// parameter version. We report no configuration fields.
void AlfredoCRSF::sendDeviceInfo(uint8_t destAddr)
{
    uint8_t payload[CRSF_DEVICE_NAME_MAX + 1 + 14];
    uint8_t nameLen = 0;
    while (_deviceName[nameLen] && nameLen < CRSF_DEVICE_NAME_MAX)
    {
        payload[nameLen] = _deviceName[nameLen];
        nameLen++;
    }
    payload[nameLen++] = '\0';
    memset(&payload[nameLen], 0, 14);
    writeExtPacket(CRSF_FRAMETYPE_DEVICE_INFO, destAddr, payload, nameLen + 14);
}
