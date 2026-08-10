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
passthrough. You can then run esptool yourself against this port:

  esptool --passthrough --chip esp32 --port <port> --baud 420000 \
          --before no_reset --after hard_reset write_flash 0x10000 firmware.bin

WARNING: while passthrough is running this board does nothing else. It stops
parsing CRSF, so channel data and failsafe handling stop with it. Never flash a
receiver on a vehicle that can move. Recover by resetting the board.
*/

#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

#define PIN_RX 4
#define PIN_TX 5

// Baud rate of the link to the receiver. Leave this at the receiver's CRSF
// baud rate: an ESP32 receiver keeps using it while being flashed and has no
// way to detect a different one.
#define RX_BAUD CRSF_BAUDRATE

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

void setup()
{
  Serial.begin(CLI_BAUD);
  crsfSerial.begin(RX_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  if (!crsfSerial) while (1) Serial.println("Invalid crsfSerial configuration");

  crsf.begin(crsfSerial);
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
  // Put the receiver into its bootloader ourselves rather than trusting the
  // command the Configurator sends through the bridge, which some receivers
  // do not act on. Sent natively it is clean and correctly timed. Harmless if
  // the Configurator also sends its own: the receiver is already rebooting.
  crsf.sendBootloaderCommand();
  crsfSerial.flush();

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
