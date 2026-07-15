// Drives a real ELRS TX module the way a handset would: sends paced channels
// frames, follows the module's requested frame rate, and prints the module's
// ELRS status. Demonstrates the ELRS 4.0 "Arm using Switch" status byte.
//
// Wiring: ELRS TX modules use a single half-duplex data line (the S.Port-style
// pin in the module bay), non-inverted for ELRS. For a bench setup connect the
// module's data pin to PIN_RX directly and to PIN_TX through a ~1k resistor.
// Most modules auto-detect the handset baud rate; 400000 is the common default.
//
// Set ARM_WITH_STATUS_BYTE to 1 only with an ELRS 4.0+ module configured for
// "Arm using Switch" - 3.x modules do not understand the longer channels frame.

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 18
#define PIN_TX 19
#define HANDSET_BAUD 400000

#define ARM_WITH_STATUS_BYTE 1

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

uint32_t lastChannelsMicros = 0;
uint32_t lastPrintMs = 0;
uint32_t lastArmToggleMs = 0;
bool armed = false;

void setup()
{
  Serial.begin(115200);
  Serial.println("ELRS handset emulator");

  crsfSerial.begin(HANDSET_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  if (!crsfSerial) while (1) Serial.println("Invalid crsfSerial configuration");

  // We are the handset, so extended frames addressed to the radio are for us
  crsf.begin(crsfSerial, CRSF_ADDRESS_RADIO_TRANSMITTER);
}

void loop()
{
  crsf.update();

  // Toggle the demo arm state every 5 seconds
  if (millis() - lastArmToggleMs > 5000)
  {
    lastArmToggleMs = millis();
    armed = !armed;
    Serial.print("Commanded arm state: ");
    Serial.println(armed ? "ARMED" : "disarmed");
  }

  // Pace channels frames at the rate the module asks for via its timing sync
  // frames (0.1us units); default to 4ms until one has been received
  uint32_t intervalUs = 4000;
  if (crsf.getHandsetTiming()->rate != 0)
    intervalUs = crsf.getHandsetTiming()->rate / 10;
  if (micros() - lastChannelsMicros >= intervalUs)
  {
    lastChannelsMicros = micros();
    sendChannels();
  }

  // Once per second, print what the module reports
  if (millis() - lastPrintMs > 1000)
  {
    lastPrintMs = millis();
    printModuleStatus();
  }
}

void sendChannels()
{
  crsf_channels_t ch = { 0 };
  ch.ch0 = CRSF_CHANNEL_VALUE_MID;  // aileron center
  ch.ch1 = CRSF_CHANNEL_VALUE_MID;  // elevator center
  ch.ch2 = CRSF_CHANNEL_VALUE_1000; // throttle low
  ch.ch3 = CRSF_CHANNEL_VALUE_MID;  // rudder center
  ch.ch4 = armed ? CRSF_CHANNEL_VALUE_2000 : CRSF_CHANNEL_VALUE_1000; // CH5/AUX1
  ch.ch5 = CRSF_CHANNEL_VALUE_1000;
  ch.ch6 = CRSF_CHANNEL_VALUE_1000;
  ch.ch7 = CRSF_CHANNEL_VALUE_1000;

#if ARM_WITH_STATUS_BYTE
  // ELRS 4.0 Arm using Switch: arm state travels in the status byte
  crsf.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch,
                     armed ? CRSF_CHANNELS_STATUS_ARMED : 0);
#else
  // Classic (ELRS 3.x compatible): arm state is just the CH5 value
  crsf.writeChannels(CRSF_ADDRESS_CRSF_TRANSMITTER, &ch);
#endif
}

void printModuleStatus()
{
  const crsf_elrs_status_t *status = crsf.getElrsStatus();
  Serial.print("pktsGood: ");
  Serial.print(status->pktsGood);
  Serial.print("  pktsBad: ");
  Serial.print(status->pktsBad);
  Serial.print("  connected: ");
  Serial.print((status->flags & CRSF_ELRS_FLAG_CONNECTED) ? "yes" : "no");
  Serial.print("  armed flag: ");
  Serial.print((status->flags & CRSF_ELRS_FLAG_ARMED) ? "yes" : "no");
  if (status->msg[0])
  {
    Serial.print("  msg: ");
    Serial.print(status->msg);
  }
  Serial.print("  frame interval: ");
  Serial.print(crsf.getHandsetTiming()->rate / 10);
  Serial.println("us");

  const crsfLinkStatistics_t *link = crsf.getLinkStatistics();
  Serial.print("  downlink LQ: ");
  Serial.print(link->downlink_Link_quality);
  Serial.print("  uplink LQ: ");
  Serial.println(link->uplink_Link_quality);
}
