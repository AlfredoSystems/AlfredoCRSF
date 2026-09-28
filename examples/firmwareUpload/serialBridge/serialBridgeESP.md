# Flashing a blank ESP8285/ESP8266 with an ESP32 as a serial bridge

Notes from getting a bare ELRS receiver chip (a blank ESP8285) talking to
esptool through an ESP32-S2 and an ESP32-S3 acting as a USB to serial adapter.
The short version: it works, but a native USB ESP32 fights you in several
non-obvious ways, and only the ESP32-S3 can actually do the job. This documents
each trap and the setup that finally worked.

## Why a bridge at all

A blank chip has no ELRS firmware on it yet, so the usual update paths do not
apply:

- Betaflight passthrough talks to the flight controller and the receiver's
  running ELRS firmware. There is none.
- The CRSF "bl" (bootloader) command is an ELRS feature. Again, none on board.

A blank ESP8285/ESP8266 can only be flashed through its **ROM bootloader**,
which speaks the esptool (SLIP) protocol over plain UART. So you need a USB to
UART adapter. If you do not have a real one (CP2102, CH340, FTDI), a native USB
ESP32 running a transparent bridge sketch can stand in, with caveats below.

## The target: ESP8285 ROM behavior

- The ESP8285 is an ESP8266 with embedded flash. esptool sees it as an ESP8266.
- The boot ROM prints a banner at a fixed **74880 baud**, for example:
  `ets Jan 8 2013,rst cause:1, boot mode:(1,7)`. This baud is not configurable.
- `boot mode:(1,x)` means UART download mode (entered with GPIO0 held low at
  reset). `boot mode:(3,x)` means normal flash boot (GPIO0 high). You want (1,x)
  to flash.
- Download mode is entered by hand: hold GPIO0 (BOOT) to GND, tap reset or power
  cycle, then release GPIO0. The chip stays in the bootloader until the next
  reset. The bridge does not wire the boot/reset lines, so esptool cannot do
  this for you.
- Once in download mode the chip is silent until it receives a SYNC, then it
  auto-bauds to whatever rate the SYNC arrived at and replies at that rate.

## The two ESP32 boards are not equal

This is the single most important lesson.

| | ESP32-S2 | ESP32-S3 |
| --- | --- | --- |
| Native USB | Yes | Yes |
| USB-OTG / TinyUSB CDC | Yes | Yes |
| USB-Serial-JTAG (HWCDC) | **No** | **Yes** |
| Usable as an esptool bridge | **No** | **Yes** |

The S2 only has the TinyUSB CDC, and that path cannot deliver esptool's traffic
reliably (next section). The S3 has a second USB mode that works. Use an S3.

## The USB CDC trap (why the S2 fails and the S3 needs a specific mode)

On a native USB ESP32 there are two USB serial implementations, chosen by the
Arduino IDE `Tools > USB Mode` setting:

1. **USB-OTG (TinyUSB)** -> `Serial` is the `USBCDC` class.
2. **Hardware CDC and JTAG** -> `Serial` is the `HWCDC` class (the built in
   USB-Serial-JTAG peripheral). Only exists on the S3/C3.

### TinyUSB (USBCDC) gates its output on DTR

TinyUSB only actually transmits bytes to the host while the host has asserted
**DTR** (the port marked "connected"). This produces a maddening symptom:

- The Arduino Serial Monitor raises DTR when it opens, so a passive test (reset
  the target, watch its boot banner stream through) works fine. Bytes flow.
- esptool does **not** hold DTR asserted, so the target's replies are silently
  dropped on the way back to the host. esptool reports:
  `Failed to connect to ESP8266: No serial data received.`

A sniffer on the wires proves the target IS receiving SYNC and IS replying
correctly. The replies simply never make it from the bridge's USB out to
esptool. Because this is a property of TinyUSB, it happens on **both** the S2 and
the S3 whenever they run in USB-OTG mode. The S2 has no other mode, so the S2 is
a dead end.

### HWCDC (Hardware CDC and JTAG) does not gate on DTR

Switch the S3 to `USB Mode = Hardware CDC and JTAG`. The USB-Serial-JTAG
peripheral is what esptool natively uses to flash S3/C3 chips, and it does not
withhold output waiting on DTR. With that mode, esptool immediately connects and
reads the target. That is the fix.

Two consequences of HWCDC mode:

- **`Serial.enableReboot()` does not exist on `HWCDC`.** Guard the call so the
  sketch compiles in both modes:
  ```cpp
  #if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE == 0
    Serial.enableReboot(false);   // TinyUSB only
  #endif
  ```
- **HWCDC honors esptool's reset in hardware.** esptool's default reset toggle
  will drop the **S3 itself** into its own ROM bootloader, and esptool then
  reports `This chip is ESP32-S3, not ESP8266`. You must tell esptool not to
  reset: `--before no-reset`. The target is already in download mode by hand, so
  no reset is wanted anyway.

## Do not mirror the host baud live

An early version of the bridge listened for the USB line-coding event and called
`target.updateBaudRate()` to follow whatever baud the host opened the port at.
Do not do this. The event fires in another task and can fire repeatedly while
esptool opens and pokes the port. Re-initializing the UART mid connect flushes
and corrupts the receive path, so replies arrive intact only some of the time.
On a sniffer you see occasional garbage frames in an otherwise clean exchange,
and esptool never gets a clean enough run to lock on.

Instead, pin the bridge to one fixed baud on both sides and set esptool to the
same number. A blank ROM auto-bauds to whatever it receives, so a steady 115200
end to end is reliable. Rule: `DEFAULT_BAUD` in the sketch and esptool's
`--baud` must match.

## Use `--no-stub`

After a clean connect, esptool's next step is to upload a small "stub flasher"
into the target's RAM. That is a larger, faster transfer, and over loose jumper
wires it tends to corrupt: `Serial data stream stopped: Possible serial noise or
corruption`. Pass `--no-stub` to skip it and talk to the ROM bootloader
directly. Slower, far more tolerant of a marginal link, and fine for a small
ELRS image.

## Give the bridge a big USB receive buffer (the write_flash stall)

With `--no-stub`, `write_flash` sends a full data block (about 1 KB) as a single
USB burst and then waits for the target's ack before the next one. The S3's
default USB CDC receive buffer is only 256 bytes, so most of that burst is lost
before the bridge can drain it out the slower UART. The target never receives a
complete block, never acks, and the flash dies on the **first** block with
`No more data to read from the serial port`.

The giveaway that this is a buffer problem and not a noisy link: it fails
deterministically at the same spot, and **lowering the baud does not help** (a
slower UART drains the burst even more slowly, so the overflow is the same or
worse). Connect, chip ID and erase all succeed because those are tiny; only the
bulk write overflows.

Fix it in the sketch by enlarging the USB receive buffer before `Serial.begin()`:

```cpp
Serial.setRxBufferSize(4096);   // absorb a whole write_flash burst
Serial.begin(DEFAULT_BAUD);
```

A 1024 byte buffer on the target UART for the reply direction does not hurt
either. With the roomy USB buffer, a 550 KB image writes and verifies in one go
at 115200. Only if the write still corrupts partway (as opposed to stalling on
block one) should you suspect the wires and drop the baud.

## The esptool version conflict with the ELRS flasher

Installing a modern esptool (`pip install esptool`, which gives 5.x) is needed
for the direct `python -m esptool` commands, but it **breaks the ELRS
Configurator's bundled `flasher.pyz`**. That bundle expects esptool 4.2.1 and
imports `make_image`, which esptool 5.x removed:
`ImportError: cannot import name 'make_image'`. The `.pyz` picks up the 5.x in
your user site-packages instead of its own bundled copy.

Run the flasher with `python -s` to ignore user site-packages, so it falls back
to its bundled esptool. This does not affect your `python -m esptool` commands
(run those without `-s`).

## The working recipe (ESP32-S3)

1. Flash `serialBridge.ino` to the S3 with:
   - `USB CDC On Boot = Enabled`
   - `USB Mode = Hardware CDC and JTAG`
   - fixed `DEFAULT_BAUD 115200`, no live baud mirror.
   - `Serial.setRxBufferSize(4096)` before `Serial.begin()` (see above).
2. Wire the S3 to the target:
   - S3 `PIN_TX` -> target RX
   - S3 `PIN_RX` -> target TX
   - common GND, and 3.3V to the target
   - use safe GPIOs (avoid the flash pins). Pins 7/8 work on the S3.
3. Put the target in download mode by hand: GPIO0 to GND, tap reset, release.
4. Find the S3's COM port (it can change after the USB Mode switch).
5. Confirm the link (reads the chip, no stub):
   ```
   python -m esptool --chip esp8266 --port COMxx --baud 115200 \
       --before no-reset --after no-reset --no-stub flash-id
   ```
   A good result prints the chip type (e.g. `ESP8285H16`), MAC, and flash size.
6. Build the firmware binary with the bind phrase baked in, without flashing:
   ```
   python -s "<...>\flasher.pyz" "--dir=<binary-targets>" "--fdir=<firmware>" \
       --target=<your.target> --phrase=<yourphrase> --no-auto-wifi \
       --lock-on-first-connection --flash=dir --out=<output dir>
   ```
   (The Configurator's own UART flash cannot be used through the bridge: it
   resets the S3 into the S3's bootloader, same as esptool without no-reset.)
7. Write it with esptool, target in download mode again:
   ```
   python -m esptool --chip esp8266 --port COMxx --baud 115200 \
       --before no-reset --after no-reset --no-stub write-flash 0x0 <firmware.bin>
   ```

## Debugging aid: a two-wire sniffer

A second native USB ESP32 with two hardware UARTs makes an excellent passive
sniffer. Tap each of the two data wires with a separate UART RX pin (both as
inputs, sharing ground with the rest), read them at the exact wire baud, and
print each byte labeled by direction. This is how the SYNC exchange was
confirmed: esptool's request is `C0 00 08 24 ... 55 55 ... C0`, and a healthy
ROM replies with `C0 01 08 02 00 07 07 12 20 00 00 C0` repeated several times.

Two things to remember when reading a sniffer:

- The sniff baud must equal the wire baud or every byte is garbage. During a
  flash that is esptool's `--baud`. For the boot banner it is always 74880.
- The sniffer taps the wires directly, so it sees traffic the bridge may fail to
  forward to the host. That difference is exactly what exposed the DTR gating:
  the target was clearly replying on the wire while esptool saw nothing.

## Fallbacks if the bridge still misbehaves

- A real USB to UART adapter (CP2102, CH340, FTDI) avoids all of the native USB
  CDC issues and is the guaranteed path.
- A classic ESP32 dev board held in reset (EN/RST to GND) turns its onboard
  USB-UART chip into a plain adapter on its TX0/RX0 pins.

## One line summary

Use an ESP32-**S3** in **Hardware CDC and JTAG** mode, a **fixed** baud with no
live mirror, and drive esptool with **`--before no-reset --no-stub`**. The S2
cannot do it because its only USB path (TinyUSB) drops the target's replies
whenever the host is not esptool-with-DTR-held, which esptool never does.
