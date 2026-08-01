---
title: Installation
nav_order: 14
---

# Installation

There is no supported public binary or web installer for the Kobo port yet.
Build a local image as described in [Kobo build](./kobo-build.md), and keep the
original Kobo recovery path available before testing.

## Command Line

These instructions are for macOS and Linux. Windows users should use the web installer.

Install `esptool`:

```sh
pip3 install esptool
```

The Kobo image is built locally. Do not use ESP32 `esptool` instructions for it.

Find the device port:

```sh
# Linux
dmesg | grep tty

# macOS
ls /dev/cu.*
```

Flash the firmware:

```sh
# Linux
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin

# macOS
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Replace the port and firmware path with your actual values.
