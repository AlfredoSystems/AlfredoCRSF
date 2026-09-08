/*
Enable Model Match on an ELRS TX module from an Arduino, without a Lua radio.

Model Match on the TX module is a config setting, separate from the model ID.
sendModelId() only sets the ID number. The on/off switch is normally reached
through the ExpressLRS Lua menu, which a DIY transmitter does not have. This
sketch sets it the same way the Lua menu does, by talking to the module's
parameter system over CRSF: discover the module, walk its parameter list to
find "Model Match", then write it on.

Wire the module the same way as the handsetEmulator example. This sketch acts
as the handset, so it uses the radio transmitter address, and runs the
parameter exchange over the serial link. If your module only answers while it
sees a live handset, add a neutral channel stream like the handsetEmulator
example does.

NOTE: this is new and has not been tested against hardware yet. The parameter
protocol is reassembled from the ExpressLRS source. If it does not work,
please report what the serial monitor prints.
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 18
#define PIN_TX 19
#define HANDSET_BAUD 400000

// The model ID to select before enabling Model Match. Model Match is stored per
// model on the module, so select the one you intend to use (0 to 63), then this
// sketch turns Model Match on for it. Must match what your receiver was bound to.
#define MODEL_ID 2

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

// Shared state, written by the callbacks and read by the state machine
bool gotDeviceInfo = false;
uint8_t paramCount = 0;
bool gotThisParam = false;
int modelMatchField = -1;

void onDeviceInfo(uint8_t deviceAddr, const char *name, uint8_t count)
{
  (void)deviceAddr;
  Serial.print("Found device: ");
  Serial.print(name);
  Serial.print(" with ");
  Serial.print(count);
  Serial.println(" parameters");
  paramCount = count;
  gotDeviceInfo = true;
}

void onParameter(uint8_t deviceAddr, const crsf_param_t *param)
{
  (void)deviceAddr;
  Serial.print("  param ");
  Serial.print(param->fieldId);
  Serial.print(": ");
  Serial.println(param->name);
  gotThisParam = true;
  if (strstr(param->name, "Model Match") != NULL)
    modelMatchField = param->fieldId;
}

enum State { PING, WAIT_INFO, READ, WAIT_PARAM, WRITE, DONE, NOT_FOUND };
State state = PING;
uint8_t field = 1;
uint32_t tAction = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Enable Model Match");

  crsfSerial.begin(HANDSET_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  if (!crsfSerial) while (1) Serial.println("Invalid crsfSerial configuration");

  crsf.begin(crsfSerial, CRSF_ADDRESS_RADIO_TRANSMITTER);
  crsf.onDeviceInfo(onDeviceInfo);
  crsf.onParameter(onParameter);

  // Select the model first, so Model Match is enabled for the right one
  crsf.sendModelId(MODEL_ID);
}

void loop()
{
  crsf.update();

  switch (state)
  {
  case PING:
    Serial.println("Pinging for devices...");
    crsf.pingDevices();
    tAction = millis();
    state = WAIT_INFO;
    break;

  case WAIT_INFO:
    if (gotDeviceInfo)
    {
      field = 1;
      state = READ;
    }
    else if (millis() - tAction > 1000)
    {
      state = PING; // retry
    }
    break;

  case READ:
    gotThisParam = false;
    crsf.readParameter(CRSF_ADDRESS_CRSF_TRANSMITTER, field);
    tAction = millis();
    state = WAIT_PARAM;
    break;

  case WAIT_PARAM:
    if (modelMatchField >= 0)
    {
      state = WRITE;
    }
    else if (gotThisParam)
    {
      field++;
      state = (field > paramCount) ? NOT_FOUND : READ;
    }
    else if (millis() - tAction > 500)
    {
      state = READ; // no reply, retry this field
    }
    break;

  case WRITE:
    Serial.print("Model Match is field ");
    Serial.print(modelMatchField);
    Serial.println(", turning it on");
    crsf.writeParameter(CRSF_ADDRESS_CRSF_TRANSMITTER, modelMatchField, 1); // 1 = on
    Serial.println("Done. Model Match should now be enabled on the module.");
    state = DONE;
    break;

  case NOT_FOUND:
    Serial.println("Did not find a Model Match parameter. Is this an ELRS TX module?");
    state = DONE;
    break;

  case DONE:
    break;
  }
}
