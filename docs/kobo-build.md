---
title: Kobo build
nav_order: 15
---

# Kobo build

This port targets the Kobo Glo HD N437. Public beta binaries are published on
GitHub Releases, but the source tree does not contain a stock Kobo image,
private recovery data, credentials, or generated release images.

## Prerequisites

- A recursive checkout of this repository.
- The exact vendor revisions listed in `sources.lock`.
- A user-owned full N437 reference SD image, supplied outside Git.
- A dedicated SSH public key, supplied outside Git.
- A Buildroot-capable Linux build environment. macOS is useful for source
  checks, but is not the reference environment for the complete image build.

Never put the reference image, private key, API key, password, or device dump
under this repository.

## Build

Create a local secrets directory and point the build at files outside Git:

```sh
export CROSSINK_SSH_PUBLIC_KEY_FILE="$HOME/.config/crossink-kobo/n437_ed25519.pub"
export CROSSINK_N437_REFERENCE_IMAGE="$HOME/firmware/kobo-n437-reference.img"
./scripts/kobo/build-rootfs.sh
```

The script checks pinned vendor revisions, applies the small required vendor
patch set, extracts the device waveform from the user-supplied reference image,
and builds the image under `output-kobo/`.

Before any deployment, verify the model, preserve a recovery path, and follow
the deployment checklist in `docs/K4_11_DEPLOYMENT_CHECKLIST.md`. A successful
host build is not proof that suspend, wake, battery life, touch, or display
behavior is correct on hardware.

## Reproducibility

Record the source commit, submodule commits, Buildroot output manifest, and
hardware test evidence for every candidate. Do not call a candidate Beta 4
until the acceptance gates in `docs/K4_00_READ_ME_FIRST.md` and
`docs/K4_12_EXECUTION_LOG.md` are complete.
