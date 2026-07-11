#!/usr/bin/env python3
"""Extract and validate the N437 EPDC waveform from a full SD image.

Netronix stores an NTX binary header in sector 14335 and the payload from
sector 14336.  The payload is device data, so it is intentionally not kept in
Git.  A final image build extracts it from the user-owned N437 reference image.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
from pathlib import Path

SECTOR_SIZE = 512
HEADER_SECTOR = 14335
PAYLOAD_SECTOR = 14336
SLOT_SECTORS = 20480
NTX_MAGIC = b"\xff\xf5\xaf\xff"
EXPECTED_SHA256 = "a158cba8276dc5ed5a146f7465285db1741612a5066497d10269f526a597de67"


def fail(message: str) -> "NoReturn":
    raise SystemExit(f"N437 waveform extraction failed: {message}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path, help="full N437 SD reference image")
    parser.add_argument("output", type=Path, help="destination epdc.fw")
    parser.add_argument(
        "--allow-unpinned-waveform",
        action="store_true",
        help="accept a structurally valid device waveform with a different hash",
    )
    args = parser.parse_args()

    minimum_size = (PAYLOAD_SECTOR + SLOT_SECTORS) * SECTOR_SIZE
    try:
        image_size = args.image.stat().st_size
    except OSError as error:
        fail(str(error))
    if image_size < minimum_size:
        fail(f"image is too small ({image_size} bytes; need at least {minimum_size})")

    with args.image.open("rb") as source:
        source.seek(HEADER_SECTOR * SECTOR_SIZE + SECTOR_SIZE - 16)
        header = source.read(16)
        if len(header) != 16 or header[:4] != NTX_MAGIC:
            fail("NTX waveform header magic is absent at sector 14335")
        payload_size = struct.unpack_from("<I", header, 8)[0]
        if not 4096 <= payload_size <= SLOT_SECTORS * SECTOR_SIZE:
            fail(f"implausible payload size {payload_size}")
        source.seek(PAYLOAD_SECTOR * SECTOR_SIZE)
        payload = source.read(payload_size)

    if len(payload) != payload_size:
        fail("short read")
    if len(payload) < 64 or not any(payload):
        fail("payload is empty or all zero")

    # waveform_data_header is 48 bytes in the modern mxc-epdc driver.
    luts = payload[36]
    trc = payload[38]
    if (luts & 0x0C) not in (0x00, 0x04):
        fail(f"unsupported LUT format byte 0x{luts:02x}")
    if not 1 <= trc + 1 <= 64:
        fail(f"implausible temperature table size {trc + 1}")

    digest = hashlib.sha256(payload).hexdigest()
    if digest != EXPECTED_SHA256 and not args.allow_unpinned_waveform:
        fail(
            f"SHA-256 {digest} differs from pinned N437 waveform; "
            "inspect it before using --allow-unpinned-waveform"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_name(f".{args.output.name}.tmp-{os.getpid()}")
    temporary.write_bytes(payload)
    temporary.chmod(0o644)
    temporary.replace(args.output)
    print(f"{digest}  {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
