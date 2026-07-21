// AlfredoCRSF 2.0 / ELRS 4.0 feature self-test. No radio hardware needed:
// two UARTs on one ESP32 are cross-wired and two AlfredoCRSF instances talk
// to each other, one playing the handset and one playing a TX module.
//
// Wiring (two jumpers):
//   PIN_TX_HANDSET (19) -> PIN_RX_MODULE (25)
//   PIN_TX_MODULE  (26) -> PIN_RX_HANDSET (18)
//
// Open the serial monitor at 115200; each check prints PASS or FAIL.

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX_HANDSET 18
#define PIN_TX_HANDSET 19
#define PIN_RX_MODULE 25
#define PIN_TX_MODULE 26

HardwareSerial serialHandset(1);
HardwareSerial serialModule(2);
AlfredoCRSF crsfHandset;
AlfredoCRSF crsfModule;

int passCount = 0;
int failCount = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("AlfredoCRSF ELRS 4.0 self-test");

  serialHandset.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX_HANDSET, PIN_TX_HANDSET);
  serialModule.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX_MODULE, PIN_TX_MODULE);

  crsfHandset.begin(serialHandset, CRSF_ADDRESS_RADIO_TRANSMITTER);
  crsfModule.begin(serialModule, CRSF_ADDRESS_CRSF_TRANSMITTER);
  crsfModule.setDeviceName("SelfTestModule");

  runTests();

  Serial.println("");
  Serial.print("Result: ");
  Serial.print(passCount);
  Serial.print(" passed, ");
  Serial.print(failCount);
  Serial.println(" failed");
}

void loop()
{
}

void runTests()
{
  crsf_channels_t ch = { 0 };
  ch.ch0 = CRSF_CHANNEL_VALUE_1000; // getChannel(1) -> 1000
  ch.ch1 = CRSF_CHANNEL_VALUE_MID;  // getChannel(2) -> 1500
  ch.ch2 = CRSF_CHANNEL_VALUE_2000; // getChannel(3) -> 2000
  ch.ch4 = CRSF_CHANNEL_VALUE_1000; // CH5 (arm channel) low

  // --- Plain channels frame (ELRS 3.x compatible, no status byte) ---
  crsfHandset.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch);
  pump(20);
  check("channels: ch1=1000", crsfModule.getChannel(1) == 1000);
  check("channels: ch2=1500", crsfModule.getChannel(2) == 1500);
  check("channels: ch3=2000", crsfModule.getChannel(3) == 2000);
  check("channels: link up", crsfModule.isLinkUp());
  check("channels: no status byte", !crsfModule.hasChannelsStatus());
  check("channels: not armed (CH5 low)", !crsfModule.isArmed());

  // --- ELRS 4.0 status byte, Arm using Switch mode ---
  crsfHandset.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch, CRSF_CHANNELS_STATUS_ARMED);
  pump(20);
  check("status byte: detected", crsfModule.hasChannelsStatus());
  check("status byte: armed via switch", crsfModule.isArmed());

  // --- Status byte with the CH5-mode bit: CH5 value wins over the armed bit ---
  crsfHandset.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch,
                            CRSF_CHANNELS_STATUS_ARMED | CRSF_CHANNELS_STATUS_ARMING_MODE_CH5);
  pump(20);
  check("CH5 mode: not armed while CH5 low", !crsfModule.isArmed());
  ch.ch4 = CRSF_CHANNEL_VALUE_2000;
  crsfHandset.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch,
                            CRSF_CHANNELS_STATUS_ARMING_MODE_CH5);
  pump(20);
  check("CH5 mode: armed while CH5 high", crsfModule.isArmed());

  // --- Back to a plain frame: status byte state must clear ---
  crsfHandset.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch);
  pump(20);
  check("status byte: cleared by plain frame", !crsfModule.hasChannelsStatus());

  // --- GPS time telemetry ---
  crsf_sensor_gps_time_t gpsTime = { 0 };
  gpsTime.year = htobe16(2026);
  gpsTime.month = 7;
  gpsTime.day = 14;
  gpsTime.hour = 12;
  gpsTime.minute = 34;
  gpsTime.second = 56;
  gpsTime.millisecond = htobe16(789);
  crsfHandset.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_GPS_TIME, &gpsTime, sizeof(gpsTime));
  pump(20);
  check("gps time: year", crsfModule.getGpsTimeSensor()->year == 2026);
  check("gps time: millisecond", crsfModule.getGpsTimeSensor()->millisecond == 789);

  // --- Cells telemetry (variable length, millivolt cell voltages) ---
  uint8_t cellsPayload[] = { 128, 0x0F, 0x0A, 0x0F, 0x14 }; // source 128, 3850mV, 3860mV
  crsfHandset.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_CELLS, cellsPayload, sizeof(cellsPayload));
  pump(20);
  check("cells: source id", crsfModule.getCellsSensor()->source_id == 128);
  check("cells: count", crsfModule.getCellsSensor()->cell_count == 2);
  check("cells: values", crsfModule.getCellsSensor()->cell[0] == 3850 &&
                         crsfModule.getCellsSensor()->cell[1] == 3860);

  // --- RPM telemetry (24-bit signed values, test sign extension) ---
  uint8_t rpmPayload[] = { 3, 0xFF, 0xFE, 0x0C }; // source 3, one value: -500
  crsfHandset.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_RPM, rpmPayload, sizeof(rpmPayload));
  pump(20);
  check("rpm: count", crsfModule.getRpmSensor()->rpm_count == 1);
  check("rpm: negative value", crsfModule.getRpmSensor()->rpm[0] == -500);

  // --- Temperature telemetry ---
  uint8_t tempPayload[] = { 1, 0x00, 0xFA, 0xFF, 0xCE }; // source 1, 25.0C, -5.0C
  crsfHandset.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_TEMP, tempPayload, sizeof(tempPayload));
  pump(20);
  check("temp: values", crsfModule.getTempSensor()->temperature[0] == 250 &&
                        crsfModule.getTempSensor()->temperature[1] == -50);

  // --- ELRS_STATUS from the module to the handset (extended header frame) ---
  uint8_t statusPayload[] = { 1, 0x01, 0x02, CRSF_ELRS_FLAG_CONNECTED, 'O', 'K', '\0' };
  crsfModule.writeExtPacket(CRSF_FRAMETYPE_ELRS_STATUS, CRSF_ADDRESS_RADIO_TRANSMITTER,
                            statusPayload, sizeof(statusPayload));
  pump(20);
  check("elrs status: packet counts", crsfHandset.getElrsStatus()->pktsBad == 1 &&
                                      crsfHandset.getElrsStatus()->pktsGood == 258);
  check("elrs status: flags", crsfHandset.getElrsStatus()->flags == CRSF_ELRS_FLAG_CONNECTED);
  check("elrs status: message", strcmp(crsfHandset.getElrsStatus()->msg, "OK") == 0);

  // --- HANDSET timing sync from the module (extended header frame) ---
  uint8_t timingPayload[] = { CRSF_HANDSET_SUBCMD_TIMING,
                              0x00, 0x00, 0x9C, 0x40,   // rate: 40000 (4ms in 0.1us units)
                              0xFF, 0xFF, 0xFF, 0x9C }; // offset: -100
  crsfModule.writeExtPacket(CRSF_FRAMETYPE_HANDSET, CRSF_ADDRESS_RADIO_TRANSMITTER,
                            timingPayload, sizeof(timingPayload));
  pump(20);
  check("handset timing: rate", crsfHandset.getHandsetTiming()->rate == 40000);
  check("handset timing: offset", crsfHandset.getHandsetTiming()->offset == -100);

  // --- Device discovery: ping the module, expect a DEVICE_INFO response.
  // The response is checked as raw bytes since the handset side has no
  // DEVICE_INFO parser (yet) ---
  drain(serialHandset);
  uint8_t none = 0;
  crsfHandset.writeExtPacket(CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_BROADCAST, &none, 0);
  pumpOnly(crsfModule, 20);
  check("device ping: DEVICE_INFO response", sawFrameType(serialHandset, CRSF_FRAMETYPE_DEVICE_INFO));

  // --- Heartbeat (raw check on the wire) ---
  drain(serialHandset);
  crsfModule.sendHeartbeat();
  delay(20);
  check("heartbeat: frame on the wire", sawFrameType(serialHandset, CRSF_FRAMETYPE_HEARTBEAT));
}

// Run both parsers for a while so frames propagate
void pump(uint32_t ms)
{
  uint32_t start = millis();
  while (millis() - start < ms)
  {
    crsfHandset.update();
    crsfModule.update();
    delay(1);
  }
}

// Run only one parser (so the other side's RX bytes stay in the buffer for raw checks)
void pumpOnly(AlfredoCRSF &crsf, uint32_t ms)
{
  uint32_t start = millis();
  while (millis() - start < ms)
  {
    crsf.update();
    delay(1);
  }
}

void drain(Stream &port)
{
  while (port.available())
    port.read();
}

// Scan raw bytes on a port for a frame of the given type (sync byte two
// bytes before the type byte)
bool sawFrameType(Stream &port, uint8_t type)
{
  uint8_t buf[128];
  size_t n = 0;
  while (port.available() && n < sizeof(buf))
    buf[n++] = port.read();
  for (size_t i = 2; i < n; i++)
  {
    if (buf[i] == type && buf[i - 2] == CRSF_SYNC_BYTE)
      return true;
  }
  return false;
}

void check(const char *name, bool ok)
{
  Serial.print(ok ? "PASS: " : "FAIL: ");
  Serial.println(name);
  if (ok) passCount++;
  else failCount++;
}
