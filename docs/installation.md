---
title: Installation
nav_order: 14
---

# Installation

The public beta image supports only the **Kobo Glo HD N437**. It replaces the
complete contents of the target microSD card. Keep the Kobo's original card
unchanged and test with a separate card of at least 2 GB.

## Download

Download these two assets from the latest GitHub prerelease:

- `CrossInk-Kobo-Glo-HD-N437-1.4.0-beta3.img.xz`
- `SHA256SUMS`

Verify the compressed download before flashing:

```sh
shasum -a 256 -c SHA256SUMS
```

On Linux, `sha256sum -c SHA256SUMS` provides the same check.

## Flash

1. Power off the Kobo and remove its original microSD card.
2. Insert a separate microSD card into the computer.
3. Open the `.img.xz` directly in Raspberry Pi Imager or balenaEtcher.
4. Select the separate microSD card, verify the target once more, and flash it.
5. Insert that card into the Kobo and power it on.

The first boot can restart once while CrossInk expands the `crossink-user`
partition to all remaining card space. Do not remove power during this step.
Later boots do not repeat the resize.

Books and writable application data live on the final ext4 partition. CrossInk
creates `/data/Books` automatically. Add books through CrossInk's File Transfer
screen or an OPDS source; the ext4 partition is not directly writable by
standard Windows or macOS installations.

## Recovery

This is beta software. To return to the stock Kobo system, power off the reader
and reinstall the untouched original microSD card.

CrossInk exposes key-only maintenance SSH at `192.168.7.2` over its physical USB
Ethernet link. It does not expose root SSH over Wi-Fi, and the release contains
no password or private SSH key.

Developers who need to produce their own image can use the reproducible
[Kobo build instructions](./kobo-build.md).
