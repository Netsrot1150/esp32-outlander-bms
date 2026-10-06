#!/usr/bin/env python3
"""Kompilerar BMS_v2 för ESP32-C3 med arduino-cli (följer med Arduino IDE).

Användning från repo-roten:
    python tools/build.py           # vanlig firmware
    python tools/build.py --sim     # med SIMULATE_CMUS (testa utan batteri)

Resultat i release/:
    BMS_v2-<version>[-sim]-usb.bin  första programmering via USB, adress 0x0 (raderar inställningar)
    BMS_v2-<version>[-sim]-ota.bin  uppdatering via webbsidan (behåller inställningar)
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "BMS_v2")
CLI = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", "Arduino IDE", "resources", "app",
                   "lib", "backend", "resources", "arduino-cli.exe")
FQBN = ("esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs,CPUFreq=160,"
        "FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,DebugLevel=none")
APP_MAX = 1966080  # Minimal SPIFFS: 1,9 MB app-partition med OTA


def version():
    with open(os.path.join(SKETCH, "config.h"), encoding="utf-8") as f:
        m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
    return m.group(1) if m else "okand"


def main():
    sim = "--sim" in sys.argv
    if not os.path.exists(CLI):
        sys.exit("Hittar inte arduino-cli: " + CLI)
    out = os.path.join(ROOT, "build", "sim" if sim else "fw")
    cmd = [CLI, "compile", "--fqbn", FQBN, "--output-dir", out, "--warnings", "default"]
    if sim:
        cmd += ["--build-property", "compiler.cpp.extra_flags=-DSIMULATE_CMUS"]
    cmd.append(SKETCH)
    print(" ".join(cmd), flush=True)
    if subprocess.run(cmd).returncode != 0:
        sys.exit("Kompileringen misslyckades")
    rel = os.path.join(ROOT, "release")
    os.makedirs(rel, exist_ok=True)
    tag = "BMS_v2-" + version() + ("-sim" if sim else "")
    app = os.path.join(out, "BMS_v2.ino.bin")
    shutil.copyfile(app, os.path.join(rel, tag + "-ota.bin"))
    shutil.copyfile(os.path.join(out, "BMS_v2.ino.merged.bin"), os.path.join(rel, tag + "-usb.bin"))
    size = os.path.getsize(app)
    print(f"\nApp: {size} byte ({100 * size / APP_MAX:.0f} % av {APP_MAX})")
    print(f"Filer i release/: {tag}-usb.bin, {tag}-ota.bin")
    if size > APP_MAX:
        sys.exit("FEL: firmwaren är större än app-partitionen")


if __name__ == "__main__":
    main()
