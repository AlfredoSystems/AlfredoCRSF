/*
Sends the variable length telemetry sensors: RPM, temperature and cell
voltages. Requires ELRS 3.5.5 or newer at both ends; older firmware does not
know these frame types and ignores them.

Unlike GPS or battery telemetry, these three carry a variable number of values
and the receiver works out how many from the frame length. Their structs in
crsf_protocol.h are decode helpers rather than wire layouts, so the payload has
to be packed into a byte buffer by hand. Each one starts with a source ID that
identifies which motor, sensor or battery the values belong to, and RPM uses
24 bit values that no htobe helper covers.
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 4
#define PIN_TX 5

// Set up a new Serial object
HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

void setup()
{
  Serial.begin(115200);
  Serial.println("COM Serial initialized");

  crsfSerial.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX, PIN_TX);
  if (!crsfSerial) while (1) Serial.println("Invalid crsfSerial configuration");

  crsf.begin(crsfSerial);
}

void loop()
{
  // Must call crsf.update() in loop() to process data
  crsf.update();

  // Four motors, the last one spinning in reverse
  int32_t motorRpm[4] = { 12500, 12480, 12510, -12495 };
  sendRpm(0, motorRpm, 4);

  // Flight controller and ambient temperature, in tenths of a degree C
  int16_t temperatures[2] = { 415, 226 }; // 41.5C and 22.6C
  sendTemperature(0, temperatures, 2);

  // A 4S pack, in millivolts
  uint16_t cells[4] = { 4150, 4148, 4152, 4149 };
  sendCells(0, cells, 4);

  delay(100);
}

// RPM values are signed 24 bit, so each one is packed as three bytes.
// Negative values mean the motor is spinning in reverse.
// sourceId identifies the group of motors, e.g. 0 for the first four ESCs.
void sendRpm(uint8_t sourceId, const int32_t *values, uint8_t count)
{
  if (count > CRSF_MAX_RPM_VALUES)
    count = CRSF_MAX_RPM_VALUES;

  uint8_t payload[1 + CRSF_MAX_RPM_VALUES * 3];
  payload[0] = sourceId;

  for (uint8_t i = 0; i < count; i++)
  {
    // Values are MSB first (BigEndian)
    payload[1 + i*3]     = (values[i] >> 16) & 0xFF;
    payload[1 + i*3 + 1] = (values[i] >> 8) & 0xFF;
    payload[1 + i*3 + 2] = values[i] & 0xFF;
  }

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_RPM, payload, 1 + count*3);
}

// Temperatures are int16 in tenths of a degree Celsius, so 250 is 25.0C and
// -50 is -5.0C. sourceId 0 is conventionally the flight controller.
void sendTemperature(uint8_t sourceId, const int16_t *values, uint8_t count)
{
  if (count > CRSF_MAX_TEMP_VALUES)
    count = CRSF_MAX_TEMP_VALUES;

  uint8_t payload[1 + CRSF_MAX_TEMP_VALUES * 2];
  payload[0] = sourceId;

  for (uint8_t i = 0; i < count; i++)
  {
    // Values are MSB first (BigEndian)
    payload[1 + i*2]     = (values[i] >> 8) & 0xFF;
    payload[1 + i*2 + 1] = values[i] & 0xFF;
  }

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_TEMP, payload, 1 + count*2);
}

// Cell voltages in millivolts, so 3850 is 3.850V. sourceId 0 is the first
// battery. ELRS 4.0 receivers use source ID 128 to report their own measured
// pack voltage this way, which is why it has millivolt precision.
void sendCells(uint8_t sourceId, const uint16_t *millivolts, uint8_t count)
{
  if (count > CRSF_MAX_CELL_VALUES)
    count = CRSF_MAX_CELL_VALUES;

  uint8_t payload[1 + CRSF_MAX_CELL_VALUES * 2];
  payload[0] = sourceId;

  for (uint8_t i = 0; i < count; i++)
  {
    // Values are MSB first (BigEndian)
    payload[1 + i*2]     = (millivolts[i] >> 8) & 0xFF;
    payload[1 + i*2 + 1] = millivolts[i] & 0xFF;
  }

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_CELLS, payload, 1 + count*2);
}
