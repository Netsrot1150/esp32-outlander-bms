#!/usr/bin/env python3
"""Förhandsvisning av BMS-webbsidan på PC, utan ESP32.

Sidan läses direkt ur BMS_v2/web_ui.h och API:t simuleras med samma JSON-format som firmwaren.
Kör från repo-roten:   python tools/mock_server.py [port]   och öppna http://localhost:8080
"""
import json
import math
import struct
import os
import random
import re
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
START = time.time()
ALARM_NAMES = ["Cell hög", "Cell låg", "Delta", "Temp hög", "Temp låg", "CMU offline", "CAN-fel"]

SETTINGS = {
    "balMode": 1, "balStartmV": 3900, "balStartDeltamV": 20, "balStopDeltamV": 8, "balMaxTempC": 45,
    "balTargetOffsetmV": 5, "manualTargetmV": 3950, "manualTimeoutMin": 60,
    "cellHighWarnmV": 4150, "cellHighAlarmmV": 4200, "cellLowWarnmV": 3300, "cellLowAlarmmV": 3000,
    "deltaWarnmV": 50, "deltaAlarmmV": 100, "tempHighWarnC": 45, "tempHighAlarmC": 55, "tempLowWarnC": 0,
    "tempOffset": 52, "canPollMs": 200, "expectedCmus": 8, "cmuTimeoutMs": 2000,
    "apSsid": "BMS-Transporter", "apPass": "byt-detta-losen", "staSsid": "MittHemnat", "staPass": "hemligt123",
    "staStatic": True, "staIp": "192.168.1.89", "staGw": "192.168.1.99", "staMask": "255.255.255.0",
    "espnowEnabled": True,
    "curEnabled": True, "curInvert": False, "curSensX100": 2200, "curZeromV": 1650, "curFactorX10": 10,
    "curWarnA": 60, "curAlarmA": 70, "capacityAhX10": 460, "socEmptymV": 3300, "socFullmV": 4100,
}
CELLS = [[3915 + ((m * 8 + k) * 7) % 23 - 6 for k in range(8)] for m in range(8)]
CELLS[2][4] = 3955
BLEED = [[0] * 8 for _ in range(8)]
LAST_TICK = [time.time()]
CAPTURE = {"until": 0.0, "frames": 0}


def uptime():
    return int(time.time() - START)


def masked():
    return dict(SETTINGS, apPass="********", staPass="********" if SETTINGS["staPass"] else "")


def offline(m):
    return m == 4 and 15 <= uptime() % 40 < 22   # CMU 5 tappar kontakten en stund var 40:e sekund


def cmus():
    now = time.time()
    tick = now - LAST_TICK[0] >= 1
    if tick:
        LAST_TICK[0] = now
    out = []
    for m in range(8):
        cells = [v + random.randint(-1, 1) for v in CELLS[m]]
        bal = [1 if v > 3940 else 0 for v in CELLS[m]]
        if tick:
            for k in range(8):
                BLEED[m][k] += bal[k]
        raw = [60 + (m + j) % 3 for j in range(5)] + [255]
        off = offline(m)
        out.append({
            "id": m + 1, "online": not off, "age": 7000 if off else random.randint(20, 200),
            "mV": sum(cells), "cells": cells, "bal": bal,
            "tC": [None if r == 255 else r - SETTINGS["tempOffset"] for r in raw], "tRaw": raw,
            "bleedS": BLEED[m][:], "cmuMv": sum(cells) + random.randint(-30, 30),
        })
    return out


def mock_current():
    s = uptime() % 1200
    a = 10.0 if s < 480 else (-25.0 if 600 <= s < 1080 else 0.0)
    return round(a + random.uniform(-0.2, 0.2), 1)


def hist_records(n, step):
    now = int(time.time())
    out = []
    for i in range(n):
        t = now - (n - 1 - i) * step
        ph = t / 3600.0
        soc = 55 + 25 * math.sin(ph / 3.0)
        cell = int(3600 + soc * 5)
        cur = 15 * math.cos(ph / 3.0) + random.uniform(-1, 1)
        out.append(struct.pack("<IHHHbbhH", t, int(cell * 64 / 100), cell - 30, cell + 20,
                               15 + int(2 * math.sin(ph)), 17 + int(2 * math.sin(ph)), int(cur * 10), int(soc * 10)))
    return struct.pack("<I", n) + b"".join(out)


def cell_records(n=336):
    """Cellogg: timmedel för 64 celler. M2C7 självurladdar 4 mV/dygn, M3C3 och M4C4 ligger lågt."""
    now = int(time.time())
    out = []
    for i in range(n):
        t = now - (n - 1 - i) * 3600
        days = (t - now) / 86400.0
        pack = 3950 + 60 * math.sin(t / 86400.0 * 2 * math.pi)
        v = []
        for c in range(64):
            x = pack + ((c * 7) % 11) - 5 + random.uniform(-1, 1)
            if c == 14:
                x += -60 - 4 * days
            elif c == 18:
                x -= 45
            elif c == 27:
                x -= 42 + 1.0 * days
            v.append(int(round(x)))
        base = min(v)
        out.append(struct.pack("<IHBB", t, base, 64, 0) + bytes(min(254, x - base) for x in v))
    return struct.pack("<I", n) + b"".join(out)


def state():
    cs = cmus()
    on = [c for c in cs if c["online"]]
    cells = [(v, i, k) for i, c in enumerate(cs) if c["online"] for k, v in enumerate(c["cells"])]
    lo, hi = min(cells), max(cells)
    temps = [t for c in on for t in c["tC"] if t is not None]
    delta = hi[0] - lo[0]
    active = []
    if len(on) < SETTINGS["expectedCmus"]:
        active.append({"id": 5, "name": ALARM_NAMES[5], "level": "ALARM",
                       "value": SETTINGS["expectedCmus"] - len(on), "loc": ""})
        bal = {"state": "BLOCKED", "targetmV": 3901, "cmdMode": 0,
               "reason": f"Blockerad: {len(on)} av {SETTINGS['expectedCmus']} CMU online"}
    else:
        target = max(lo[0] + 5, 3900)
        bal = {"state": "ACTIVE", "targetmV": target, "cmdMode": 1,
               "reason": f"Balanserar: delta {delta} mV, mål {target} mV"}
    if delta >= SETTINGS["deltaWarnmV"]:
        active.append({"id": 2, "name": ALARM_NAMES[2], "level": "WARN", "value": delta, "loc": ""})
    bal["mode"] = ["off", "auto", "manual"][SETTINGS["balMode"]]
    return {
        "up": uptime(),
        "pack": {"mV": sum(v for v, _, _ in cells), "cells": len(cells),
                 "minmV": lo[0], "minLoc": f"M{lo[1] + 1}C{lo[2] + 1}", "minIdx": [lo[1], lo[2]],
                 "maxmV": hi[0], "maxLoc": f"M{hi[1] + 1}C{hi[2] + 1}", "maxIdx": [hi[1], hi[2]],
                 "deltamV": delta, "minT": min(temps), "maxT": max(temps),
                 "online": len(on), "expected": SETTINGS["expectedCmus"]},
        "bal": bal,
        "alarms": {"alarm": sum(1 << a["id"] for a in active if a["level"] == "ALARM"),
                   "warn": sum(1 << a["id"] for a in active if a["level"] == "WARN"), "active": active},
        "soc": {"valid": True, "pct": 59.8, "chargePct": 61.4, "avgPct": 74.6, "mode": "ah", "kWhLeft": 6.59, "capAh": 46.0},
        "cur": {"enabled": True, "A": mock_current(), "kW": round(mock_current() * 0.247, 2), "mV": 1650,
                "saturated": False, "ahIn": 12.4, "ahOut": 30.1, "kWhIn": 3.05, "kWhOut": 7.4,
                "minutesLeft": 58 if mock_current() < -1 else (124 if mock_current() > 1 else None)},
        "time": {"known": True, "epoch": int(time.time())},
        "cmus": cs,
    }


def alarm_log(t):
    events = []
    cycle = 0
    while cycle * 40 + 15 <= t:
        base = cycle * 40
        events.append({"t": base + 15, "id": 5, "name": ALARM_NAMES[5], "level": "ALARM", "active": True, "value": 1, "loc": ""})
        if base + 22 <= t:
            events.append({"t": base + 22, "id": 5, "name": ALARM_NAMES[5], "level": "ALARM", "active": False, "value": 0, "loc": ""})
        cycle += 1
    return list(reversed(events))[:20]


def diag():
    t = uptime()
    rows = []
    for c in cmus():
        subs = []
        for sub in range(1, 6):
            if sub == 1:
                data = [sum(1 << k for k, b in enumerate(c["bal"]) if b), 0x30] + c["tRaw"]
            elif sub in (2, 3):
                data = []
                for v in c["cells"][(sub - 2) * 4:(sub - 1) * 4]:
                    data += [v >> 8, v & 0xFF]
            elif sub == 4:
                data = [0xFF] * 8
            else:
                data = [0] * 6 + [c["mV"] >> 8, c["mV"] & 0xFF]
            subs.append({"sub": sub, "hex": " ".join(f"{x:02X}" for x in data), "age": c["age"],
                         "fps": 5 if c["online"] else 0, "n": t * 5})
        rows.append({"id": c["id"], "status": 0x30, "invalid": 0, "subs": subs})
    running = time.time() < CAPTURE["until"]
    frames = int((10 - (CAPTURE["until"] - time.time())) * 165) if running else CAPTURE["frames"]
    return {
        "fw": "2.2.0-mock", "up": t, "reset": "omstart (programvara/OTA)", "heap": 148000, "minHeap": 121000, "sim": True,
        "can": {"ok": True, "rx": t * 160, "other": 0, "txOk": t * 5, "txBusy": 0, "txAborts": 0, "overflows": 0,
                "tec": 0, "rec": 0, "eflg": 0, "pollMs": SETTINGS["canPollMs"], "lastErrS": None,
                "mcpStep": 1, "initCanstat": 0x80, "canstat": 0x00, "canctrl": 0x00, "initTries": 1, "reinits": 0, "spiOk": True},
        "net": {"apSsid": SETTINGS["apSsid"], "apIp": "192.168.4.1", "apClients": 1, "staSsid": SETTINGS["staSsid"],
                "staConnected": False, "staIp": "", "rssi": 0, "channel": 1,
                "espnow": SETTINGS["espnowEnabled"], "espnowSent": t * 2},
        "cmus": rows,
        "capture": {"running": running, "frames": frames},
        "alarmLog": alarm_log(t),
        "cur": {"enabled": True, "mV": 1652, "zeromV": SETTINGS["curZeromV"], "saturated": False},
        "time": {"known": True, "epoch": int(time.time())},
        "hist": {"n24": 1440, "nLive": 600, "fsOk": True, "nCells": 336},
    }


def csv():
    lines = ["Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8"]
    t = 0
    for _ in range(3):
        lines.append(f"{t:010d},000003C3,false,Tx,0,8,0F,3D,00,04,03,00,00,00")
        for c in cmus():
            t += 260
            raw = [sum(1 << k for k, b in enumerate(c["bal"]) if b), 0x30] + c["tRaw"]
            lines.append(f"{t:010d},{0x601 + (c['id'] << 4):08X},false,Rx,0,8," + ",".join(f"{x:02X}" for x in raw))
        t += 200000
    return ("\n".join(lines) + "\n").encode()


def load_html():
    with open(os.path.join(ROOT, "BMS_v2", "web_ui.h"), encoding="utf-8") as f:
        m = re.search(r'R"BMSUI\((.*)\)BMSUI"', f.read(), re.S)
    return m.group(1).encode("utf-8")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, code, body, ctype="application/json", headers=None):
        if not isinstance(body, bytes):
            body = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            return self.reply(200, load_html(), "text/html; charset=utf-8")
        if path == "/api/state":
            return self.reply(200, state())
        if path == "/api/settings":
            return self.reply(200, masked())
        if path == "/api/diag":
            return self.reply(200, diag())
        if path == "/api/capture.csv":
            return self.reply(200, csv(), "text/csv", {"Content-Disposition": "attachment; filename=bms-can.csv"})
        if path == "/api/history":
            live = "range=10m" in self.path
            return self.reply(200, hist_records(600, 1) if live else hist_records(1440, 60), "application/octet-stream")
        if path == "/api/cells":
            return self.reply(200, cell_records(), "application/octet-stream")
        if path == "/api/report":
            return self.reply(200, {"state": state(), "diag": diag(), "settings": masked()},
                              headers={"Content-Disposition": "attachment; filename=bms-rapport.json"})
        self.reply(404, b"Hittades inte", "text/plain")

    def do_POST(self):
        path = self.path.split("?")[0]
        n = int(self.headers.get("Content-Length") or 0)
        data = self.rfile.read(n) if n else b""
        if path == "/api/settings":
            body = json.loads(data or b"{}")
            if body.get("balStopDeltamV", 0) >= body.get("balStartDeltamV", 20):
                return self.reply(400, {"error": "Ogiltigt värde", "field": "balStopDeltamV"})
            SETTINGS.update({k: v for k, v in body.items() if v != "********"})
            return self.reply(200, masked())
        if path == "/api/time":
            return self.reply(200, {"known": True, "epoch": int(time.time())})
        if path == "/api/current/zero":
            SETTINGS["curZeromV"] = 1652
            return self.reply(200, {"zeromV": 1652})
        if path == "/api/capture":
            CAPTURE["until"] = time.time() + 10
            CAPTURE["frames"] = 1650
            return self.reply(200, {"ok": True})
        if path in ("/api/bleed/reset", "/api/restart"):
            if path == "/api/bleed/reset":
                for row in BLEED:
                    row[:] = [0] * 8
            return self.reply(200, {"ok": True})
        if path == "/update":
            return self.reply(200, "Uppdatering klar – BMS:en startar om".encode("utf-8"), "text/plain; charset=utf-8")
        self.reply(404, b"Hittades inte", "text/plain")


if __name__ == "__main__":
    print(f"Mock-BMS på http://localhost:{PORT}  (Ctrl+C avslutar)")
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
