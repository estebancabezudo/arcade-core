# First boot: Raspberry Pi 4 from USB

[Platform guide](../../README.md) | [Español](README.es.md)

The prepared `kernel8.img`, `config.txt` and `SHA256SUMS` are in [the generated example directory](../../../../../build/standalone/raspberry_pi/examples/pi4_usb/), relative to this document. From the repository root the path is `build/standalone/raspberry_pi/examples/pi4_usb/`.

The image is supplied for **Raspberry Pi 4 Model B hardware testing**, not as a claim of a successful physical boot. It embeds the supplied Pac-Man ROM set and uses neither Linux nor SDL/MAME/CRT.

## First-run procedure from the supplied package

1. Prepare a Pi 4 USB boot medium with the official Raspberry Pi firmware already in its FAT boot partition. The original package proposes Raspberry Pi Imager for this preparation; this project does not include firmware or configure EEPROM. USB boot must already be enabled in the board's boot order. Use a dedicated test medium and back up its configuration.
2. Mount the boot partition. From the project root, install the prepared example:

   ```bash
   bash standalone/platforms/raspberry_pi/scripts/install_pi4_example.sh /path/to/boot
   ```

   The installer verifies SHA-256, backs up existing `config.txt` and `kernel8.img` to a new `arcade-core-backup/install.*` directory, and replaces only those two files. Other firmware files are retained. It neither formats nor selects a disk automatically.
3. Safely unmount the medium, connect HDMI and power up the Pi. Remove an SD card if it takes priority over the USB device. If USB boot is not attempted, use the official tools for the board's firmware/EEPROM configuration; this installer does not alter it.
4. The intended result is direct Pac-Man attract mode without Linux. Playing requires the GPIO controls below; a USB keyboard or gamepad is not supported by this kernel.

## Buttons

Active-low buttons connect GPIO to GND and need external pull-ups to 3.3 V; the supplied guide suggests 10 kΩ. These are GPIO identifiers, not connector pin numbers. Never connect 5 V to a GPIO.

| Function | GPIO |
| --- | ---: |
| Up / Down | 5 / 6 |
| Left / Right | 13 / 19 |
| Coin | 16 |
| Start 1 / Start 2 | 20 / 21 |

## Audio from the supplied backend

Sound uses software PDM on GPIO18, not HDMI or the integrated analogue jack. The original package proposes this initial filter:

```text
GPIO18 -- 1 kΩ --+-- amplified input
                 |
                10 nF
                 |
                GND
```

The incoming guide also recommends capacitive coupling before the amplifier. Do not connect a speaker directly to GPIO. This circuit and the synchronous audio backend still need physical validation; USB HID, HDMI audio and DMA/PWM audio are not implemented.

## Recreating the example

If root `build/` has been removed, supply the shared ROMs and the cross-toolchain, then run `make pacman-rpi-example TOOLCHAIN=clang` from the repository root. This rebuilds Pi 4 and regenerates all three example files. [Validation and image provenance](../../../../../docs/validation/raspberry-pi-integration.md).
