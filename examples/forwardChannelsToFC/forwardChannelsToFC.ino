#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX_IN1 18
#define PIN_TX_IN1 19

#define PIN_RX_IN2 25
#define PIN_TX_IN2 26

#define PIN_RX_OUT 16
#define PIN_TX_OUT 17

// Set up a new Serial object
HardwareSerial crsfSerialIn1(1);
AlfredoCRSF crsfIn1;

HardwareSerial crsfSerialIn2(2);
AlfredoCRSF crsfIn2;

HardwareSerial crsfSerialOut(0);
AlfredoCRSF crsfOut;

void setup() {
  crsfSerialIn1.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX_IN1, PIN_TX_IN1);
  delay(1000);
  crsfSerialIn2.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX_IN2, PIN_TX_IN2);
  delay(1000);
  crsfSerialOut.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX_OUT, PIN_TX_OUT);
  delay(1000);
  crsfIn1.begin(crsfSerialIn1);
  crsfIn2.begin(crsfSerialIn2);
  crsfOut.begin(crsfSerialOut);
}

void loop() {
  crsfIn1.update();
  crsfIn2.update();

  // Check if RX1 is connected
  if (crsfIn1.isLinkUp()) {
    // RX1 is connected
    if (crsfIn2.isLinkUp()) {
      // Both RX1 and RX2 are connected, select based on Link Quality
      int lq1 = getLinkQuality(crsfIn1);
      int lq2 = getLinkQuality(crsfIn2);
      if (lq1 >= lq2) {
        sendChannels(crsfIn1);
      } else {
        sendChannels(crsfIn2);
      }
    } else {
      // Only RX1 is connected
      sendChannels(crsfIn1);
    }
  } else {
    // RX1 is not connected
    if (crsfIn2.isLinkUp()) {
      // Only RX2 is connected
      sendChannels(crsfIn2);
    } else {
      // No RX connected, send fallback channels
      sendFallbackChannels();
    }
  }
}

// M>ethod to get the link quality from CRSF instance
int getLinkQuality(AlfredoCRSF& crsf) {
  const crsfLinkStatistics_t* stat_ptr = crsf.getLinkStatistics();
  return stat_ptr->uplink_Link_quality;
}

// Method to send channels based on CRSF instance
void sendChannels(AlfredoCRSF& crsf) {
  const crsf_channels_t* channels_ptr = crsf.getChannelsPacked();
  crsfOut.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, channels_ptr, sizeof(*channels_ptr));
}

// Fallback method to send default channel values
void sendFallbackChannels() {
  crsf_channels_t crsfChannels = { 0 };
  crsfChannels.ch0 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch1 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch2 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch3 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch4 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch5 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch6 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch7 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch8 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch9 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch10 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch11 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch12 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch13 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch14 = CRSF_CHANNEL_VALUE_1000;
  crsfChannels.ch15 = CRSF_CHANNEL_VALUE_1000;

  crsfOut.writePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, &crsfChannels, sizeof(crsfChannels));
}
