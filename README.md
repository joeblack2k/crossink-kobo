# CrossInk for Kobo Glo HD

CrossInk-Kobo is a lightweight, open-source reading system for the **Kobo Glo
HD N437**. It replaces the complete software on a separate internal microSD
card: a small Linux system boots first, then starts the CrossInk reader.

> [!IMPORTANT]
> The downloadable image is only for the **Kobo Glo HD, model N437**.
> Do not flash it to a Kobo Aura, Clara, Libra, Mini or another Kobo model.

## Start here

| | |
|---|---|
| Current release | **CrossInk Kobo Glo HD Beta 3** |
| Download | [Release page](https://github.com/joeblack2k/crossink-kobo/releases/tag/v1.4.0-kobo-beta3) |
| Supported device | Kobo Glo HD N437 |
| Minimum card size | 2 GB |
| Main book format | EPUB |
| Status | Public beta |

The release page contains:

- `CrossInk-Kobo-Glo-HD-N437-1.4.0-beta3.img.xz`, the flashable image;
- `SHA256SUMS`, for checking the download;
- a build manifest with the exact source, kernel and image hashes.

## What is CrossInk-Kobo?

CrossInk-Kobo gives an older Kobo a focused, local-first reading environment
without requiring an account or cloud service. The reader is built around
comfortable EPUB reading, clear typography and predictable e-ink behavior.

This repository is a fork of [CrossInk](https://github.com/uxjulia/CrossInk),
but this branch is specifically a **Kobo Linux port**. Some shared reader code
comes from the original ESP32 project; the downloadable Kobo image does not run
on an ESP32 and is not installed over the normal Kobo desktop updater.

The system has two main layers:

1. **Linux and Buildroot** boot the Kobo hardware and manage the display,
   touchscreen, frontlight, battery, Wi-Fi, storage and sleep/wake behavior.
2. **The CrossInk reader** provides the home screen, library, EPUB reader,
   settings, reading statistics and file-transfer interface.

## What you need

- a Kobo Glo HD N437;
- a separate microSD card of at least 2 GB;
- a computer with a microSD card reader;
- Raspberry Pi Imager or balenaEtcher;
- the original Kobo microSD card kept somewhere safe.

Using a separate card is strongly recommended. The original card is your
easiest recovery route back to the standard Kobo software.

## Installing the beta

1. Download the `.img.xz` file and `SHA256SUMS` from the
   [Beta 3 release](https://github.com/joeblack2k/crossink-kobo/releases/tag/v1.4.0-kobo-beta3).
2. Check the download:

   ```sh
   shasum -a 256 -c SHA256SUMS
   ```

   On Linux, use `sha256sum -c SHA256SUMS`.

3. Open the `.img.xz` file directly in Raspberry Pi Imager or balenaEtcher.
4. Select the separate microSD card, check the target carefully and flash it.
5. Put the flashed card in the Kobo and power it on.
6. Leave the Kobo powered while the first boot completes.

The first boot may restart once. During that restart, CrossInk expands the
`crossink-user` partition and its filesystem to use all remaining space on the
card. This works with larger cards too and is not repeated on later boots.

See the full [installation and recovery guide](./docs/installation.md) before
opening the Kobo or replacing its internal card.

## Adding books

CrossInk creates `/data/Books` automatically. The easiest way to add books is
over Wi-Fi:

1. Open **File Transfer** on the Kobo.
2. Choose **Join Network** or **Create Hotspot**.
3. Open the address or QR code shown on the Kobo.
4. Upload EPUB files with the browser file manager.

Books can also be downloaded from a configured OPDS library or sent through
Calibre Wireless. See the [File Transfer guide](./docs/webserver.md) for the
available methods.

The book partition uses ext4. Standard Windows and macOS installations cannot
write to it directly, so use CrossInk's File Transfer screen instead of trying
to mount the partition on those systems.

## What works

- EPUB reading with touch navigation and reading-position recovery;
- adjustable fonts, font size, line spacing, margins and publisher styling;
- bookmarks, text clippings, recent books and reading statistics;
- multiple reader themes and sleep-screen options;
- Kobo touchscreen, e-ink refresh, frontlight and battery reporting;
- suspend, wake and automatic reader recovery after an application crash;
- Wi-Fi file transfer, WebDAV, Calibre Wireless and OPDS;
- automatic use of the remaining space on 2 GB, 32 GB and larger cards;
- a clean recovery path by reinstalling the untouched original Kobo card.

CrossInk is deliberately a reader rather than a general-purpose tablet. EPUB is
the main format. PDF reading, media playback, games and a full web browser are
outside the project's scope.

## Beta status

Beta 3 is the first public Kobo image. It has passed:

- a clean, pinned Buildroot/Linux/CrossInk build;
- image layout, kernel, DTB, waveform and root-filesystem verification;
- a simulated first boot and storage expansion on a 32 GB card;
- filesystem checking, filesystem resize and repeated-boot checks;
- GitHub host tests, dependency checks, formatting, static analysis and a
  tracked-source secrets scan;
- an anonymous public download and checksum verification.

This exact downloadable image was not subjected to an additional physical
flash-and-boot cycle after packaging. It remains prerelease software, so keep
the original Kobo card and report hardware results through
[GitHub Issues](https://github.com/joeblack2k/crossink-kobo/issues).

Known Beta 3 limitation: progressive JPEG cover thumbnails are skipped on Kobo
to avoid a known ARM decoder crash. Normal baseline JPEG covers continue to
work.

## Storage layout

| Partition | Purpose |
|---|---|
| Boot | Kobo kernel, device tree and display waveform |
| Recovery | Independent maintenance and recovery environment |
| Root filesystem | Minimal Buildroot Linux system |
| `crossink-user` | Books, settings, caches and reading data |

Only the final `crossink-user` partition grows. The fixed boot, recovery and
Linux partitions are left unchanged.

## For developers

The Kobo target lives under `platform/kobo` and `buildroot-external`. Buildroot
starts `/usr/sbin/crossink-supervisor`, which launches the active reader from
`/opt/crossink/current/bin/crossink-kobo` and handles bounded crash recovery.

A complete image build needs Linux, the pinned revisions in `sources.lock`, a
user-owned N437 reference image and an SSH public key kept outside Git:

```sh
export CROSSINK_SSH_PUBLIC_KEY_FILE="$HOME/.config/crossink-kobo/n437_ed25519.pub"
export CROSSINK_N437_REFERENCE_IMAGE="$HOME/firmware/kobo-n437-reference.img"
./scripts/kobo/build-rootfs.sh
```

Read [Kobo build](./docs/kobo-build.md) for the complete prerequisites and
release rules. A successful host build is not proof of correct physical
display, touch, suspend or battery behavior.

## Documentation

- [Installation and recovery](./docs/installation.md)
- [File Transfer and Wi-Fi](./docs/webserver.md)
- [Reader features](./docs/reader-features.md)
- [Troubleshooting](./docs/troubleshooting.md)
- [Kobo build instructions](./docs/kobo-build.md)
- [Project scope](./SCOPE.md)
- [Changelog](./CHANGELOG.md)

## Credits and license

CrossInk-Kobo builds on the work of the
[CrossInk](https://github.com/uxjulia/CrossInk) and Crosspoint Reader
communities. The source is provided under the [MIT License](./LICENSE).
