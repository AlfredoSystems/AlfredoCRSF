/*
Transparent USB to serial bridge, for flashing a blank ESP8285 or ESP8266
(such as a bare ELRS receiver chip) that has no firmware yet.

A blank chip cannot be flashed with Betaflight passthrough or the CRSF "bl"
command. Those talk to ELRS firmware that is not on the chip yet. A blank chip
is flashed through its ROM bootloader instead, and this sketch turns a USB
native ESP32 (S2 or S3) into the USB to serial adapter for that job.

This sketch does not use the AlfredoCRSF library. It is a companion tool: use
it once to get ELRS onto the chip, after which the library and the other
examples work over the normal CRSF link.

Steps:
  1. Flash this to an ESP32-S2 or S3 with "USB CDC On Boot" set to Enabled.
  2. Wire it to the target chip:
       this board TX (PIN_TX) -> target RX
       this board RX (PIN_RX) -> target TX
       common GND, and 3.3V to the target
  3. Put the target into ROM download mode by hand, right before flashing: hold
     its GPIO0 (BOOT) pad to GND, tap reset or power-cycle it, then release
     GPIO0. It now waits in the bootloader. This is manual because the bridge
     does not wire the boot and reset lines, so esptool cannot do it for you.
  4. In the ExpressLRS Configurator, pick your target, choose the UART flashing
     method, select this board's port, and hit Flash. No Configurator setting or
     command line is needed: esptool's reset toggle does nothing to the target
     (harmless), and the target is already sitting in the bootloader from step 3.

If the Configurator ever refuses, you can flash by hand instead:
  esptool --chip esp8266 --port <this board's port> --baud 115200 \
          --before no_reset --after soft_reset write_flash 0x0 firmware.bin

The bridge runs at a fixed 115200 both ways (DEFAULT_BAUD). Set the Configurator
or esptool to the same 115200 so nothing tries to renegotiate the rate.
*/

#include <HardwareSerial.h>

#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "This sketch needs a USB native ESP32 (S2 or S3) with 'USB CDC On Boot' enabled."
#endif

#define PIN_RX 4       // this board's RX, wired to the target's TX
#define PIN_TX 5       // this board's TX, wired to the target's RX
// Fixed bridge baud, host side and wire side. Flash with esptool or the
// Configurator at this same rate. A blank ESP8266/8285 ROM auto-bauds to
// whatever it receives, so a steady rate end to end is the most reliable.
//
// We deliberately do NOT mirror the host baud live. Re-initing the UART from the
// USB line-coding event, which runs in another task and can fire repeatedly
// while esptool opens the port, flushes and corrupts the receive path, so the
// bootloader's replies never reach esptool. Keeping one fixed rate avoids that.
#define DEFAULT_BAUD 115200

HardwareSerial target(1); // UART to the chip being flashed

void setup()
{
  // Big USB receive buffer. During write_flash, esptool sends a full data block
  // (~1 KB) in one burst and then waits for the ack. The default buffer is much
  // smaller, so the burst overflows and bytes are lost before the bridge can
  // relay them out the slower UART. The target then never gets a complete block,
  // never acks, and the flash stalls on the first block ("No more data to read
  // from the serial port"). A roomy buffer absorbs the whole burst.
  Serial.setRxBufferSize(4096);
  Serial.begin(DEFAULT_BAUD);
  // esptool toggles DTR/RTS to reset its target. On the TinyUSB CDC that pattern
  // makes this board reboot itself, which would kill the bridge mid flash, so
  // turn it off. Either way the DTR/RTS reset never reaches the target, so the
  // target must be put into its bootloader by hand (GPIO0 to GND, then reset).
  //
  // The "Hardware CDC and JTAG" USB mode (HWCDC) is the one to use for flashing:
  // unlike TinyUSB it does not gate its output on the host asserting DTR, so
  // esptool actually receives the bootloader's replies. HWCDC has no
  // enableReboot() and does not need it, so only call it in TinyUSB mode.
#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE == 0
  Serial.enableReboot(false);
#endif
  // Roomy receive buffer so a burst of replies from the target is never dropped
  // while the loop is busy pushing a write_flash block the other way.
  target.setRxBufferSize(1024);
  target.begin(DEFAULT_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
}

void loop()
{
  uint8_t buf[256];

  int n = Serial.available();
  if (n > 0)
  {
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    n = Serial.readBytes(buf, n);
    target.write(buf, n);
  }

  n = target.available();
  if (n > 0)
  {
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    n = target.readBytes(buf, n);
    Serial.write(buf, n);
  }
}
