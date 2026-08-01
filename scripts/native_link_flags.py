#!/usr/bin/env python3
"""Print native-only link flags used by PlatformIO's simulator target."""

import os
import sys


if sys.platform == "darwin":
    prefix = os.environ.get("HOMEBREW_PREFIX", "/opt/homebrew")
    openssl = os.path.join(prefix, "opt", "openssl@3")
    print(f"-I{openssl}/include -L{openssl}/lib -lssl -lcrypto -Wno-deprecated-declarations")
else:
    print("-lssl -lcrypto -Wno-deprecated-declarations")
