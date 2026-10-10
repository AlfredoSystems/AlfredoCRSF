/*
  switchesAndButtons - read the sticks as axes and the switches as positions.

  CrsfSwitch turns a channel the transmitter drives to 1000/1500/2000 us into
  DOWN / MIDDLE / UP, and tells you the loop it changes. That covers three-
  position switches, two-position switches, and a pair of momentary buttons
  that share one channel (one pulls it to 1000, the other to 2000).
  getAxis() turns a stick channel into -1..1 with a dead zone.

  The channel numbers below are a RadioMaster Zorro on ExpressLRS: aux
  channels 5 and up carry the switches, 11 the two bumpers.
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

//Rotini V3:
//#define PIN_RX 7
//#define PIN_TX 8
//Rotini V4:
//#define PIN_RX 18
//#define PIN_TX 17

#define PIN_RX 4       // this board's RX, wired to the receiver's TX
#define PIN_TX 5       // this board's TX, wired to the receiver's RX

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

CrsfSwitch armSwitch(5);    // two positions: DOWN or UP
CrsfSwitch modeSwitch(6);   // three positions
CrsfSwitch bumpers(11);     // DOWN = left bumper held, UP = right bumper held

const char *name(CrsfSwitch::Position p)
{
  return p == CrsfSwitch::DOWN ? "down" : p == CrsfSwitch::UP ? "up" : "middle";
}

void setup()
{
  Serial.begin(115200);
  crsfSerial.begin(CRSF_BAUDRATE, SERIAL_8N1, PIN_RX, PIN_TX);
  crsf.begin(crsfSerial);
}

void loop()
{
  crsf.update();
  armSwitch.update(crsf);     // after crsf.update(), once per loop
  modeSwitch.update(crsf);
  bumpers.update(crsf);

  if (!crsf.isLinkUp()) return;

  // Switches: react the loop they move
  if (armSwitch.movedTo(CrsfSwitch::UP)) Serial.println("armed");
  if (armSwitch.movedTo(CrsfSwitch::DOWN)) Serial.println("disarmed");
  if (modeSwitch.moved())
  {
    Serial.print("mode switch: ");
    Serial.println(name(modeSwitch.position()));
  }

  // Buttons: movedTo is a press, movedFrom a release, is() while held
  if (bumpers.movedTo(CrsfSwitch::DOWN)) Serial.println("left bumper pressed");
  if (bumpers.movedTo(CrsfSwitch::UP)) Serial.println("right bumper pressed");
  if (bumpers.movedFrom(CrsfSwitch::UP)) Serial.println("right bumper released");

  // Sticks, ten times a second
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 100)
  {
    lastPrint = millis();
    Serial.print("throttle ");
    Serial.print(crsf.getAxis(3));   // -1..1, 0 near the center
    Serial.print("  turn ");
    Serial.println(crsf.getAxis(1));
  }
}
