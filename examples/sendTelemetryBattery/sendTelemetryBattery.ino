/*
This example reads the voltage on pin PIN_SNS_VIN and sends it to your radio in a telemetry packet
This code assumes you are using a voltage divider with a "high side" resistance of 15K and "low side" resistance of 2.2K
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 4
#define PIN_TX 5

#define PIN_SNS_VIN 3

#define RESISTOR1 15000.0
#define RESISTOR2 2200.0
#define ADC_RES 8192.0
#define ADC_VLT 3.3

// How often to send telemetry, in milliseconds. Do not send telemetry every
// loop: ELRS only carries it as fast as the Telem Ratio allows, so sending
// faster does not make it arrive sooner, it just backs up the serial buffer
// and slows this loop down. Keep this rate within what your ratio can carry.
#define TELEM_INTERVAL_MS 100

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

float cap = 0;
uint32_t lastTelemMs = 0;
void loop()
{
  // Call crsf.update() every loop to process incoming data and keep link state current
  crsf.update();

  if (millis() - lastTelemMs >= TELEM_INTERVAL_MS)
  {
    lastTelemMs = millis();
    int snsVin = analogRead(PIN_SNS_VIN);
    float batteryVoltage = ((float)snsVin * ADC_VLT / ADC_RES) * ((RESISTOR1 + RESISTOR2) / RESISTOR2);
    sendRxBattery(batteryVoltage, 1.2, cap += 10, 50);
  }
}

static void sendRxBattery(float voltage, float current, float capacity, float remaining)
{
  crsf_sensor_battery_t crsfBatt = { 0 };

  // Values are MSB first (BigEndian)
  crsfBatt.voltage = htobe16((uint16_t)(voltage * 10.0));   //Volts
  crsfBatt.current = htobe16((uint16_t)(current * 10.0));   //Amps
  crsfBatt.capacity = htobe24((uint32_t)(capacity));        //mAh (24 bit field, max 16777215mAh)
  crsfBatt.remaining = (uint8_t)(remaining);                //percent
  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_BATTERY_SENSOR, &crsfBatt, sizeof(crsfBatt));
}