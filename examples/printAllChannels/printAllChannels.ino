#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

//Rotini V3:
//#define PIN_RX 7
//#define PIN_TX 8
//Rotini V4:
//#define PIN_RX 18 
//#define PIN_TX 17

#define PIN_RX 4       // this board's RX, wired to the target's TX
#define PIN_TX 5       // this board's TX, wired to the target's RX

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
    printChannels();
}

//Use crsf.getChannel(x) to get us channel values (1-16).
void printChannels()
{
  for (int ChannelNum = 1; ChannelNum <= 16; ChannelNum++)
  {
    Serial.print(crsf.getChannel(ChannelNum));
    Serial.print(", ");
  }
  Serial.println(" ");
}