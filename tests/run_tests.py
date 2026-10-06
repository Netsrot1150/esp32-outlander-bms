#!/usr/bin/env python3
"""Bygger och kör PC-testerna för BMS v2 med zig (clang).

Kör från repo-roten:  python tests/run_tests.py
"""
import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "BMS_v2")
ARDUINOJSON = os.path.join(os.path.expanduser("~"), "Documents", "Arduino", "libraries", "ArduinoJson", "src")
# Rena moduler (ingen Arduino-kod) som testas på PC. Moduler som inte finns ännu hoppas över.
PURE = ["cmu.cpp", "settings.cpp", "settings_json.cpp", "balancer.cpp", "alarms.cpp", "current.cpp", "soc.cpp", "history.cpp", "celllog.cpp",
        "packet_builder.cpp", "sim.cpp"]


def main():
    srcs = sorted(glob.glob(os.path.join(ROOT, "tests", "*.cpp")))
    srcs += [os.path.join(SKETCH, f) for f in PURE if os.path.exists(os.path.join(SKETCH, f))]
    exe = os.path.join(ROOT, "tests", "run_tests.exe")
    cmd = [sys.executable, "-m", "ziglang", "c++", "-std=c++17", "-Wall", "-Wextra",
           "-Wno-missing-field-initializers", "-I", SKETCH, "-I", ARDUINOJSON, *srcs, "-o", exe]
    if subprocess.run(cmd).returncode != 0:
        sys.exit("Kompileringen av testerna misslyckades")
    sys.exit(subprocess.run([exe]).returncode)


if __name__ == "__main__":
    main()
