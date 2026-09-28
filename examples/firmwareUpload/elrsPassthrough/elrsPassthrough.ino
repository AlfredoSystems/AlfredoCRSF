/*
Update the firmware on a connected ELRS receiver through this board, the same
way flashing through a Betaflight flight controller works.

Flash this sketch, then in the ExpressLRS Configurator pick your receiver
target, choose the "Betaflight Passthrough" flashing method, select this
board's serial port and hit flash. Nothing else is needed: the Configurator
drives the whole process. Flash your normal sketch again afterwards.

The Configurator starts by talking to what it thinks is a Betaflight CLI, so
this sketch answers the handful of questions it asks. When it asks for
passthrough, the sketch sends the receiver into its bootloader itself and then
copies bytes between the USB port and the receiver. It sends the bootloader
command directly rather than relying on the one the Configurator sends through
the bridge, which some receivers do not act on.

There is also a manual path. Open a serial terminal at 115200, type "bl" and
press enter, and the sketch sends the receiver into its bootloader and starts
passthrough. Close the terminal, then run esptool yourself against this port.
For an ESP8285 receiver (most 2.4GHz ELRS RX):

  esptool --chip esp8266 --port <port> --baud 115200 --before no-reset \
          --after no-reset --no-stub write-flash 0x0 firmware.bin

firmware.bin must be the uncompressed image. On a native USB board the --baud
value does not matter, the sketch sets the receiver side to BL_BAUD. For an ESP32
receiver, set BL_BAUD to RX_BAUD and write the firmware at 0x10000 instead.

WARNING: while passthrough is running this board does nothing else. It stops
parsing CRSF, so channel data and failsafe handling stop with it. Never flash a
receiver on a vehicle that can move. Recover by power cycling the board: a
plain reset deliberately stays in passthrough (see passthroughArmed).
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

// Native USB boards (S3/C3) need "Hardware CDC and JTAG" USB mode: TinyUSB only
// sends to the host while it holds DTR, which esptool does not, so the
// receiver's replies never arrive. Classic ESP32 boards use a USB to serial
// chip and are unaffected (ARDUINO_USB_MODE only exists on native USB parts).
#if defined(ARDUINO_USB_MODE)
  #if ARDUINO_USB_MODE != 1
    #error "Set Tools > USB Mode to 'Hardware CDC and JTAG'. In USB-OTG (TinyUSB) mode the receiver's replies never reach esptool and flashing hangs at Connecting."
  #endif
  #if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
    #error "Set Tools > USB CDC On Boot to 'Enabled' so Serial is the USB port used for passthrough."
  #endif
#endif

//Rotini V3:
//#define PIN_RX 7
//#define PIN_TX 8
//Rotini V4: 
#define PIN_RX 18
#define PIN_TX 17

//#define PIN_RX 4
//#define PIN_TX 5

// Baud rate of the running receiver, used to send it the bootloader command
#define RX_BAUD CRSF_BAUDRATE

// Baud rate of the receiver's bootloader, used for the flash itself. An ESP8285
// receiver (most 2.4GHz ELRS RX) sits at 74880 after the bootloader command;
// any other rate garbles every reply. For an ESP32 receiver, whose bootloader
// keeps the CRSF rate, set this equal to RX_BAUD.
#define BL_BAUD 74880

// Baud rate for talking to the Configurator before passthrough starts
#define CLI_BAUD 115200

// After the bootloader command is sent, wait this long before bridging so the
// receiver finishes rebooting into its bootloader. An ESP8285 receiver (like
// most 2.4GHz ELRS RX) prints its name, waits 100ms, then reboots, and its ROM
// only auto-detects the baud rate once it is in that mode. Bridging too early
// lets the host's baud-training bytes arrive before the ROM is listening, so
// esptool then cannot sync. Raise this if flashing is unreliable.
#define BOOTLOADER_REBOOT_MS 300

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

bool passthroughActive = false;
char cmdBuf[64];
uint8_t cmdLen = 0;

// The Configurator closes the port after the CLI handshake and reopens it for
// esptool, and on a native USB board that reopen resets this board. RTC memory
// survives a reset but not a power cycle, so this flag brings a board that was
// mid-flash straight back into the bridge instead of the CLI.
RTC_NOINIT_ATTR uint32_t passthroughArmed;
static const uint32_t ARMED_MAGIC = 0x50415353; // "PASS"

void setup()
{
  // esptool writes the flash in 2 KB bursts, far more than the default receive
  // buffer holds, so a burst would overflow before it could be relayed
  Serial.setRxBufferSize(4096);
  Serial.begin(CLI_BAUD);
  crsfSerial.begin(RX_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  if (!crsfSerial) while (1) Serial.println("Invalid crsfSerial configuration");

  crsf.begin(crsfSerial);

  // Reset mid-flash: resume the bridge. Do not resend the bootloader command,
  // the receiver is already in its bootloader, which locks its baud rate to the
  // first bytes it sees, and a stray CRSF frame would lock it to the wrong one.
  if (passthroughArmed == ARMED_MAGIC)
  {
    crsfSerial.updateBaudRate(BL_BAUD);
    passthroughActive = true;
  }
}

void loop()
{
  if (passthroughActive)
  {
    bridgeBytes();
    return;
  }

  readCommands();
}

// Copy bytes in both directions as fast as they arrive. Once this starts the
// board is a wire and nothing else.
void bridgeBytes()
{
  uint8_t buf[64];

  int count = Serial.available();
  if (count > 0)
  {
    if (count > (int)sizeof(buf)) count = sizeof(buf);
    Serial.readBytes(buf, count);
    crsfSerial.write(buf, count);
  }

  count = crsfSerial.available();
  if (count > 0)
  {
    if (count > (int)sizeof(buf)) count = sizeof(buf);
    crsfSerial.readBytes(buf, count);
    Serial.write(buf, count);
  }
}

// The Configurator sends a bare '#' with no line ending to open the CLI, then
// sends each command terminated with CRLF.
void readCommands()
{
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '#' && cmdLen == 0)
    {
      // Anything ending in "# " is accepted as a CLI prompt
      Serial.print("\r\nEntering CLI Mode, type 'exit' to return\r\n\r\n# ");
      continue;
    }

    if (c == '\n' || c == '\r')
    {
      if (cmdLen > 0)
      {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
      }
      continue;
    }

    if (cmdLen < sizeof(cmdBuf) - 1)
      cmdBuf[cmdLen++] = c;
  }
}

void handleCommand(const char *cmd)
{
  // The Configurator checks the receiver protocol settings before it will
  // start. Report the configuration it is looking for.
  if (strcmp(cmd, "get serialrx_provider") == 0)
  {
    Serial.print("serialrx_provider = CRSF\r\n# ");
  }
  else if (strcmp(cmd, "get serialrx_inverted") == 0)
  {
    Serial.print("serialrx_inverted = OFF\r\n# ");
  }
  else if (strcmp(cmd, "get serialrx_halfduplex") == 0)
  {
    Serial.print("serialrx_halfduplex = OFF\r\n# ");
  }
  // Next it asks which UART the receiver is on. Function bit 64 marks a
  // serial RX port, and the number that follows "serial" is the port index
  // used in the passthrough command below.
  else if (strcmp(cmd, "serial") == 0)
  {
    Serial.print("serial 1 64 115200 57600 0 115200\r\n");
    Serial.print("# \r\n");
  }
  // Finally it asks for passthrough on that port. From here the board is a
  // transparent bridge and the Configurator flashes the receiver directly.
  else if (strncmp(cmd, "serialpassthrough", 17) == 0)
  {
    startPassthrough(parseBaud(cmd));
  }
  // Not part of the Betaflight protocol: start passthrough by hand
  else if (strcmp(cmd, "bl") == 0)
  {
    startPassthrough(RX_BAUD);
  }
  else
  {
    Serial.print("# ");
  }
}

// "serialpassthrough <port> <baud>"
uint32_t parseBaud(const char *cmd)
{
  const char *p = strchr(cmd, ' ');          // before port index
  if (p) p = strchr(p + 1, ' ');             // before baud
  uint32_t baud = p ? strtoul(p + 1, NULL, 10) : 0;
  return baud ? baud : RX_BAUD;
}

void startPassthrough(uint32_t hostBaud)
{
  passthroughArmed = ARMED_MAGIC;

  // Put the receiver into its bootloader ourselves rather than trusting the
  // command the Configurator sends through the bridge, which some receivers
  // do not act on. Sent natively it is clean and correctly timed. Harmless if
  // the Configurator also sends its own: the receiver is already rebooting.
  crsf.sendBootloaderCommand();
  crsfSerial.flush();

  crsfSerial.updateBaudRate(BL_BAUD);

  // Let the receiver finish rebooting into its bootloader before we start
  // passing the host's bytes through, so its ROM is listening when the baud
  // training arrives. Bytes the host sends during this window buffer and are
  // bridged right after.
  delay(BOOTLOADER_REBOOT_MS);

  Serial.flush();

#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
  // On boards that reach the PC through a USB to serial chip the host really
  // does change the line rate, so follow it. With native USB the rate is
  // virtual and there is nothing to do.
  Serial.updateBaudRate(hostBaud);
#else
  (void)hostBaud;
#endif

  passthroughActive = true;
}
