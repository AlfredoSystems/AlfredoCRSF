# Flashing ESP8285/ESP8266 receivers through a native USB ESP32

Notes from getting ELRS receivers (ESP8285) flashed through an ESP32-S2 or
ESP32-S3 acting as the USB to serial link, using the ExpressLRS Configurator.
Two examples came out of it:

- `serialBridge`: flash a **blank** chip through its ROM bootloader, with the
  Configurator's UART method.
- `elrsPassthrough`: update a receiver that **already runs ELRS**, with the
  Configurator's Betaflight Passthrough method.

A native USB ESP32 fights you in several non-obvious ways. Each one is below,
with how it was found and the fix.

## Which example do you need

A receiver with ELRS on it can be sent into its bootloader with a CRSF command,
so `elrsPassthrough` handles it with nothing extra wired. A **blank** chip has no
firmware to receive that command, so it can only be flashed through its **ROM
bootloader**, which needs GPIO0 held low at reset. That is `serialBridge`.

## ESP8285 bootloader facts

- The ESP8285 is an ESP8266 with embedded flash. esptool sees it as an ESP8266.
- The boot ROM prints a banner at a fixed **74880 baud**, such as
  `ets Jan 8 2013,rst cause:1, boot mode:(1,7)`. `boot mode:(1,x)` is UART
  download mode (GPIO0 low at reset); `(3,x)` is a normal flash boot.
- The ROM bootloader **locks its baud rate to the first bytes it receives** and
  keeps it. Anything sent before esptool's SYNC can lock it to the wrong rate.
- An ELRS receiver sent into its bootloader by the CRSF `bl` command announces
  its target name (for example `UNIFIED_ESP8285_2400_RX`) at **74880** and then
  talks at 74880. Relaying at any other rate garbles every reply
  (`Invalid head of packet (0x82)`).

## The two USB modes

On a native USB ESP32, `Tools > USB Mode` picks what `Serial` is:

| | USB-OTG (TinyUSB) | Hardware CDC and JTAG |
| --- | --- | --- |
| Class | `USBCDC` | `HWCDC` |
| Available on | S2, S3 | S3 (not S2) |
| esptool's DTR/RTS reset toggle | An event the sketch sees; `enableReboot(false)` ignores it | **Reboots the ESP32 itself into its bootloader. Cannot be turned off on the S3** |
| `Serial.write()` while DTR is low | **Sends nothing** (see below) | Sends normally |
| Line coding (host baud) visible to the sketch | Yes, `Serial.baudRate()` | No |

Neither mode is right for everything, which is why the two examples differ.

### TinyUSB's `Serial.write()` drops data while DTR is low

Arduino's `USBCDC::write()` returns without sending anything unless the host
has DTR asserted. esptool leaves DTR low, so every reply from the target is
thrown away and esptool reports `No serial data received`. The Arduino Serial
Monitor raises DTR, so a passive test (watch the target's boot banner come
through) works, which makes this very confusing.

The check lives only in Arduino's wrapper. TinyUSB itself does not care, so the
fix is to send to the host through TinyUSB directly:

```cpp
#include "esp32-hal-tinyusb.h"

void sendToHost(const uint8_t *buf, size_t len) {
  // tud_cdc_n_write_available / tud_cdc_n_write / tud_cdc_n_write_flush
}
```

`serialBridge` does this, which is what makes the S2 usable at all.

### HWCDC reboots on esptool's reset toggle

In Hardware CDC mode the S3's USB hardware treats esptool's DTR/RTS reset
sequence as "reset this chip into download mode", exactly as it does when you
flash the S3 itself. esptool then reports `This chip is ESP32-S3, not ESP8266`.
The S3 has no register to disable this (newer chips like the C6 do).

`--before no-reset` avoids it when you run esptool yourself, but the
Configurator's UART method always sends the reset. That is why `serialBridge`
uses TinyUSB.

## serialBridge: the Configurator's UART method

The Configurator runs, in effect:

```
esptool --chip esp8266 --baud 460800 --after soft_reset write_flash 0x0000 firmware.bin
```

That means the default reset toggle, the stub flasher, and a baud change partway
through. The bridge has to handle all three:

- **Reset toggle:** TinyUSB mode plus `Serial.enableReboot(false)`, so the bridge
  does not reboot itself.
- **Replies:** sent with TinyUSB directly, as above.
- **Baud change:** esptool syncs at 115200, loads the stub, then switches the
  target to 460800. The bridge follows the host's rate by polling
  `Serial.baudRate()` from `loop()`, checked after reading host bytes and before
  writing them out, so each byte goes out at the rate the host set before
  sending it. An earlier version reconfigured the UART from the USB line-coding
  event callback, which runs in another task, and that raced with the relay and
  corrupted replies.
- **Buffering:** the stub writes the flash in 16 KB blocks, each sent as one USB
  burst, far more than the 256 byte default receive buffer.
  `Serial.setRxBufferSize(32768)` absorbs it. The failure without it is very
  specific: connect, chip ID and erase all work (they are small), then the flash
  dies on the first data block with `No more data to read from the serial port`,
  and lowering the baud does not help.

The target still has to be put in download mode by hand (GPIO0 to GND, tap
reset, release) because a bare chip's reset and boot pads are not wired to the
bridge. `serialBridge` has optional `PIN_TARGET_EN` and `PIN_TARGET_BOOT` pins
that drive them from DTR/RTS like a USB adapter's auto-reset circuit, but that
part is **untested on hardware**.

Because the bridge ignores the reset pattern the Arduino IDE uses to start an
upload, reflash the bridge itself by holding its BOOT button while plugging in.

## elrsPassthrough: the Configurator's Betaflight Passthrough method

The sketch pretends to be a Betaflight CLI, sends the receiver the `bl` command
itself, then bridges. This one runs in Hardware CDC mode: the flasher runs
esptool with `--before no_reset`, so there is no reset toggle to worry about.
Two things had to be fixed:

- **Relay at 74880 after `bl`.** The command goes out at the CRSF rate the
  receiver is running at (420000); the bootloader that comes up is at 74880
  (`BL_BAUD`). For an ESP32 receiver, whose bootloader keeps the CRSF rate, set
  `BL_BAUD` to the CRSF rate instead.
- **Survive the port being reopened.** The Configurator opens the port three
  times: the CLI handshake at 115200, a `reset_to_bootloader` step at 420000,
  then esptool. It opens with plain pyserial, which asserts DTR and RTS on
  Windows (esptool 5.x deliberately opens with both off), and closing and
  reopening resets the S3, dropping it back into the CLI. The sketch keeps a
  flag in `RTC_NOINIT` memory, which survives a reset but not a power cycle, and
  goes straight back into the bridge. It must **not** send `bl` again at that
  point: the receiver is already in its bootloader, and a stray CRSF frame would
  lock the ROM to the wrong baud rate.

The flasher's `reset_to_bootloader` step sends ROM training bytes and the ELRS
init sequence (`EC .. 32 'b' 'l' "ESP82" crc`) and then reads the target name.
Through the bridge the name comes back at 74880 while the flasher listens at
420000, so it prints `Cannot detect RX target, blindly flashing!`. That is
harmless.

## The esptool version conflict with the ELRS flasher

Installing esptool 5.x (`pip install esptool`) for manual commands breaks the
Configurator's bundled `flasher.pyz`, which expects 4.2.1 and fails with
`ImportError: cannot import name 'make_image'` because it picks up the newer
copy from user site-packages. Run the flasher with `python -s` to ignore user
site-packages; run `python -m esptool` without it.

## Manual flashing

Useful to take the Configurator out of the picture. With the target in download
mode:

```
python -m esptool --chip esp8266 --port COMxx --baud 115200 \
    --before no-reset --after no-reset --no-stub write-flash 0x0 firmware.bin
```

`firmware.bin` must be the uncompressed image. The Configurator's `--flash=dir`
output is `firmware.bin.gz`; decompress it first.

## Debugging techniques that worked

**Two-wire sniffer.** A second ESP32 with two UART RX pins tapping the two data
wires, printing each byte labeled by direction, at exactly the wire baud (the
flash baud, or 74880 for boot banners). A healthy SYNC looks like
`C0 00 08 24 ... 55 55 ... C0` from esptool and
`C0 01 08 02 00 07 07 12 20 00 00 C0` repeated from the ROM. Seeing the target
reply on the wire while esptool saw nothing is what exposed the dropped writes.

**Read the flasher's source.** `flasher.pyz` unpacks to
`%USERPROFILE%\.shiv\flasher.pyz_*\site-packages`. `binary_flash.py` shows the
exact esptool arguments per method, and `BFinitPassthrough.py` shows the
Betaflight passthrough sequence.

**Replay the Configurator one step at a time.** A small script that performs
the flasher's steps separately (CLI handshake, then `reset_to_bootloader`, then
the stub) and ends each run with a known-good esptool connect. The first step
that breaks the connect is the culprit. This is how the port reopen reset was
found, after several wrong guesses.

## Fallbacks

- A real USB to UART adapter (CP2102, CH340, FTDI) avoids every native USB issue
  here.
- A classic ESP32 dev board with EN held to GND turns its onboard USB to serial
  chip into a plain adapter on TX0/RX0.
