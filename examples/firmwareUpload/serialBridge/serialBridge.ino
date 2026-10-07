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
  1. Flash this to an ESP32-S2 or S3 with Tools > USB Mode set to
     "USB-OTG (TinyUSB)" and "USB CDC On Boot" set to Enabled.
  2. Wire it to the target chip:
       this board TX (PIN_TX) -> target RX
       this board RX (PIN_RX) -> target TX
       common GND, and 3.3V to the target
  3. Put the target into ROM download mode by hand, right before flashing: hold
     its GPIO0 (BOOT) pad to GND, tap reset or power-cycle it, then release
     GPIO0. It now waits in the bootloader. Skip this if you wired the target's
     EN and GPIO0 pads to PIN_TARGET_EN and PIN_TARGET_BOOT.
  4. In the ExpressLRS Configurator, pick your target, choose the UART flashing
     method, select this board's port, and hit Flash.

The bridge follows whatever baud rate the host sets, including esptool's switch
to a faster rate partway through the flash, so no Configurator setting is
needed. You can also flash by hand with plain esptool against this port.

To upload a new sketch to this board later, hold its BOOT button while plugging
it in. This sketch ignores the reset the Arduino IDE normally uses, because
esptool sends that same reset when flashing the target.
*/

#include <HardwareSerial.h>

// TinyUSB is required. In "Hardware CDC and JTAG" mode the USB hardware reboots
// this board into its own bootloader whenever esptool toggles DTR/RTS to reset
// its target, and that cannot be turned off from a sketch.
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools > USB CDC On Boot to 'Enabled'. This sketch needs a USB native ESP32 (S2 or S3)."
#endif
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE != 0
#error "Set Tools > USB Mode to 'USB-OTG (TinyUSB)'. In Hardware CDC and JTAG mode esptool's reset toggle reboots this board."
#endif

#include "esp32-hal-tinyusb.h"

//Rotini V3:
//#define PIN_RX 7
//#define PIN_TX 8
//Rotini V4:
//#define PIN_RX 18
//#define PIN_TX 17

#define PIN_RX 4       // this board's RX, wired to the target's TX
#define PIN_TX 5       // this board's TX, wired to the target's RX

// Optional automatic download mode. Wire these to the target's EN (reset) and
// GPIO0 (boot) pads and esptool puts the target into its bootloader itself, the
// way a USB to serial adapter's auto-reset circuit does, so step 3 is not
// needed. Leave at -1 if the target does not expose those pads.
#define PIN_TARGET_EN   -1
#define PIN_TARGET_BOOT -1

// Rate used until the host sets one. esptool syncs at 115200.
#define DEFAULT_BAUD 115200

HardwareSerial target(1); // UART to the chip being flashed
uint32_t targetBaud = DEFAULT_BAUD;

void setup()
{
  // esptool writes the flash in 16 KB blocks, each sent in one burst. The
  // default receive buffer is 256 bytes, so a block would overflow before it
  // could be relayed out the much slower UART.
  Serial.setRxBufferSize(32768);
  Serial.begin(DEFAULT_BAUD);

  // esptool toggles DTR/RTS to reset its target, and by default that pattern
  // reboots this board instead. It is passed on to the target below.
  Serial.enableReboot(false);

#if SOC_UART_SUPPORT_REF_TICK
  // On the ESP32-S2 the core clocks a UART from the 1 MHz REF_TICK at 250000
  // baud and below, which builds 115200 out of uneven 8 and 9 microsecond bits.
  // The target's bootloader measures that as about 5% fast and keeps the error
  // when esptool switches to 460800, where this board's rate is exact, so the
  // two ends can no longer talk. The APB clock gives even bits at every rate.
  target.setClockSource(UART_CLK_SRC_APB);
#endif
  target.begin(DEFAULT_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Open drain, so the pins only ever pull low and never fight the target's own
  // pull-ups. HIGH means released.
  if (PIN_TARGET_EN >= 0)
  {
    pinMode(PIN_TARGET_EN, OUTPUT_OPEN_DRAIN);
    digitalWrite(PIN_TARGET_EN, HIGH);
  }
  if (PIN_TARGET_BOOT >= 0)
  {
    pinMode(PIN_TARGET_BOOT, OUTPUT_OPEN_DRAIN);
    digitalWrite(PIN_TARGET_BOOT, HIGH);
  }
}

void pullLow(int pin, bool low)
{
  if (pin >= 0) digitalWrite(pin, low ? LOW : HIGH);
}

// esptool resets its target with DTR/RTS: RTS (without DTR) holds the chip in
// reset, and DTR asks for GPIO0 low so it boots into download mode. esptool has
// two orderings of those steps, and one of them passes through states that a
// direct line to pin mapping gets wrong. So hold reset while RTS asks for it, and
// on release boot into download mode if DTR was asserted around the reset.
void followTargetReset()
{
  static bool inReset = false;
  static uint32_t lastDtr = 0;

  uint8_t lines = tud_cdc_n_get_line_state(0);
  bool dtr = lines & 0x01;
  bool rts = lines & 0x02;
  if (dtr) lastDtr = millis();

  if (!inReset && rts && !dtr)
  {
    inReset = true;
    pullLow(PIN_TARGET_BOOT, false);
    pullLow(PIN_TARGET_EN, true);
  }
  else if (inReset && !rts)
  {
    inReset = false;
    bool download = millis() - lastDtr < 500;
    pullLow(PIN_TARGET_BOOT, download);
    pullLow(PIN_TARGET_EN, false);
    if (download)
    {
      delay(50); // hold GPIO0 low while the target starts and samples it
      pullLow(PIN_TARGET_BOOT, false);
    }
  }
}

// Match the target UART to the rate the host has set on the USB port. Polled
// from loop rather than done in a USB event callback, so the UART is never
// reconfigured while it is in use.
void followHostBaud()
{
  uint32_t hostBaud = Serial.baudRate();
  if (hostBaud != 0 && hostBaud != targetBaud)
  {
    target.flush(); // finish sending at the old rate first
    target.updateBaudRate(hostBaud);
    targetBaud = hostBaud;
  }
}

// Serial.write() sends nothing while the host has DTR low, and esptool leaves
// DTR low after its reset toggle, so every reply from the target would be lost.
// TinyUSB itself has no such check, so write to it directly.
void sendToHost(const uint8_t *buf, size_t len)
{
  uint32_t start = millis();
  while (len > 0 && tud_mounted() && millis() - start < 100)
  {
    uint32_t space = tud_cdc_n_write_available(0);
    if (space == 0)
    {
      tud_cdc_n_write_flush(0);
      delay(1);
      continue;
    }
    if (space > len) space = len;
    tud_cdc_n_write(0, buf, space);
    tud_cdc_n_write_flush(0);
    buf += space;
    len -= space;
  }
}

void loop()
{
  uint8_t buf[64];

  if (PIN_TARGET_EN >= 0)
    followTargetReset();

  int n = Serial.available();
  if (n > 0)
  {
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    n = Serial.readBytes(buf, n);
    // Check the rate after reading: the host sets its rate before it sends,
    // so this is the rate these bytes were meant for.
    followHostBaud();
    target.write(buf, n);
  }

  n = target.available();
  if (n > 0)
  {
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    n = target.readBytes(buf, n);
    sendToHost(buf, n);
  }
}
