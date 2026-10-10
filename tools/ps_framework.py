#!/usr/bin/env python3
"""Install the ESP32 power-saving core next to the stock one.

*_ps environments build against framework-arduinoespressif32 with CONFIG_PM_ENABLE
(https://github.com/rogovogor/arduino-esp32-ps). PlatformIO keeps one package per
name, so the PS core is not installed as a package: it is unpacked once into
<core_dir>/ps/<name> and linked from [esp32_ps] in platformio.ini, while the
normal environments stay pinned to the stock core.

    python tools/ps_framework.py                 # download the release, verify, unpack
    python tools/ps_framework.py --archive F     # from a local archive (same sha256 check)
    python tools/ps_framework.py --check         # only report what is installed

Standard library only. Safe to run again: an installed, matching core is left as is.
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

PS_VERSION = "2.0.17-ps.1"
PKG_VERSION = "3.20017.241212-ps.1"
URL = ("https://github.com/rogovogor/arduino-esp32-ps/releases/download/"
       f"{PS_VERSION}/framework-arduinoespressif32-{PS_VERSION}.tar.gz")
SHA256 = "fe39860de634dc4797dd57a930eaa1f12f6a41981e7adb5bbca6f3e9166518d1"
NAME = f"framework-arduinoespressif32-{PS_VERSION}"
PM_CHECK = "tools/sdk/esp32s3/qio_qspi/include/sdkconfig.h"


def core_dir() -> Path:
    env = os.environ.get("PLATFORMIO_CORE_DIR")
    return Path(env) if env else Path.home() / ".platformio"


def installed_ok(target: Path) -> bool:
    try:
        version = json.loads((target / "package.json").read_text())["version"]
        pm = "CONFIG_PM_ENABLE 1" in (target / PM_CHECK).read_text()
    except (OSError, KeyError, ValueError):
        return False
    return version == PKG_VERSION and pm


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    print(f"downloading {url}")
    with urllib.request.urlopen(url) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--archive", type=Path, help="local archive instead of downloading")
    ap.add_argument("--check", action="store_true", help="only report the installed state")
    args = ap.parse_args()

    target = core_dir() / "ps" / NAME
    if installed_ok(target):
        print(f"PS core {PS_VERSION} installed: {target}")
        return 0
    if args.check:
        print(f"PS core {PS_VERSION} NOT installed (expected at {target})")
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        archive = args.archive or tmp / f"{NAME}.tar.gz"
        if not args.archive:
            download(URL, archive)
        digest = sha256_of(archive)
        if digest != SHA256:
            print(f"sha256 mismatch: got {digest}, expected {SHA256}", file=sys.stderr)
            return 2
        print("sha256 OK, unpacking")
        with tarfile.open(archive, "r:gz") as tar:
            tar.extractall(tmp / "x")
        roots = [p.parent for p in (tmp / "x").glob("*/package.json")]
        if len(roots) != 1:
            print("unexpected archive layout (no single top directory with package.json)", file=sys.stderr)
            return 2
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(roots[0]), str(target))

    if not installed_ok(target):
        print(f"installed, but the check failed: {target}", file=sys.stderr)
        return 2
    print(f"PS core {PS_VERSION} installed: {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
