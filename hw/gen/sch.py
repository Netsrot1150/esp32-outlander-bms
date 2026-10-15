"""Ritar circuit.py som ett platt KiCad-schema (A3), ett block per funktion. Varje pinne får en kort ledning
och en nätetikett, så att schemat och kortet alltid har samma nätlista.
Kör: python hw/gen/sch.py
"""
from __future__ import annotations
import functools, os, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hw" / "gen"))
KSYM = pathlib.Path("F:/KiCad/share/kicad/symbols")
OWN = ROOT / "hw" / "lib" / "bms.kicad_sym"
os.environ["KICAD_SYMBOL_DIR"] = f"{KSYM};{OWN.parent}"
import kicad_sch_api as ksa

from circuit import COMPS
from sexpr import parse, find_all, child

OUT = ROOT / "hw" / "bms-kort" / "bms-kort.kicad_sch"
G = 2.54
STUB = 2.54
BLOCKS = {  # x0, y0, kolumner, rubrik
    "INGANG": (15, 25, 4, "Ingång: polaritetsskydd Q1, TVS D1"),
    "BUCK": (15, 105, 5, "Omvandlare 12 -> 4,9 V (alltid på), EN: på 11,5 V / av 10,3 V"),
    "SKENA": (140, 25, 6, "BMS-skena: Q2/Q3 (GPIO10), polyfuse F1, LED, 3,3 V (U2)"),
    "CAN": (140, 125, 5, "CAN: SN65HVD230 på TWAI (GPIO4 TX, GPIO3 RX), ESD D4, avslutning JP1"),
    "MATNING": (290, 25, 3, "Mätning: 12 V till GPIO1, WCS1700 till GPIO0"),
    "ESP": (290, 105, 2, "ESP32-C3 SuperMini, expansion J5"),
    "MEK": (290, 200, 5, "Hål och mätpunkter"),
}
CELL_X, CELL_Y = 25.4, 22.86
IC_X, IC_Y = 45.72, 40.64


def snap(v):
    return round(v / G) * G


@functools.lru_cache(maxsize=None)
def _lib(name):
    path = OWN if name == "bms" else KSYM / f"{name}.kicad_sym"
    return parse(path.read_text(encoding="utf-8"))


def sym_pins(lib_id):
    lib, name = lib_id.split(":")
    syms = {s[1]: s for s in _lib(lib) if isinstance(s, list) and s and s[0] == "symbol"}
    sym = syms[name]
    ext = child(sym, "extends")
    if ext:
        sym = syms[ext[1]]
    out = {}
    for sub in sym:
        if isinstance(sub, list) and sub and sub[0] == "symbol":
            for p in find_all(sub, "pin"):
                at = child(p, "at")
                out[child(p, "number")[1]] = (float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0)
    return out


def _dir(angle):
    return {0: (-1, 0, 180), 180: (1, 0, 0), 90: (0, 1, 270), 270: (0, -1, 90)}[int(round(angle)) % 360]


def layout():
    pos, by = {}, {}
    for c in COMPS:
        by.setdefault(c.block, []).append(c)
    for block, comps in by.items():
        x0, y0, cols, _ = BLOCKS[block]
        big = [c for c in comps if len(sym_pins(c.symbol)) > 4]
        small = [c for c in comps if c not in big]
        x, y = x0, y0 + 12
        for c in big:
            pos[c.ref] = (snap(x + IC_X / 2), snap(y + IC_Y / 2))
            x += IC_X
            if x + IC_X > x0 + cols * CELL_X + 1:
                x, y = x0, y + IC_Y
        if big and x != x0:
            y += IC_Y
        for i, c in enumerate(small):
            pos[c.ref] = (snap(x0 + (i % cols) * CELL_X + CELL_X / 2), snap(y + (i // cols) * CELL_Y + CELL_Y / 2))
    return pos


def render():
    sch = ksa.create_schematic("bms-kort")
    sch.set_paper_size("A3")
    pos = layout()
    for _, (x0, y0, _, title) in BLOCKS.items():
        sch.add_text(title, (snap(x0), snap(y0)), size=2.0, bold=True)
    for c in COMPS:
        pins = sym_pins(c.symbol)
        if set(pins) != set(c.pins):
            raise ValueError(f"{c.ref}: symbolens pinnar {sorted(pins)} != {sorted(c.pins)}")
        X, Y = pos[c.ref]
        props = {"LCSC": c.lcsc} if c.lcsc else {}
        if c.mfr:
            props["MFR"] = c.mfr
        sch.components.add(c.symbol, reference=c.ref, value=c.value, position=(X, Y), footprint=c.footprint, **props)
        for num, (px, py, ang) in pins.items():
            ax, ay = X + px, Y - py
            net = c.pins[num]
            if net is None:
                sch.no_connects.add((ax, ay))
                continue
            dx, dy, rot = _dir(ang)
            ex, ey = ax + dx * STUB, ay + dy * STUB
            sch.add_wire((ax, ay), (ex, ey))
            sch.add_label(net, position=(ex, ey), rotation=rot, size=1.0)
    # PWR_FLAG: VIN drivs av batteriet via Q1, GND av J1, VBMS via Q2 (ERC kan inte se det)
    for i, net in enumerate(("VIN", "GND", "VBMS")):
        X, Y = snap(300 + 25 * i), snap(250)
        sch.components.add("power:PWR_FLAG", reference=f"#FLG0{i + 1}", value="PWR_FLAG", position=(X, Y))
        sch.add_wire((X, Y), (X, Y + STUB))
        sch.add_label(net, position=(X, Y + STUB), rotation=270, size=1.0)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    sch.save(str(OUT))
    s = OUT.read_text(encoding="utf-8")
    s = re.sub(r'(\(property "(?:LCSC|MFR)" "[^"]*"\s*\(at [^)]*\)\s*\(effects\s*\(font\s*\(size [^)]*\)\s*\))',
               r'\1 (hide yes)', s)
    s = re.sub(r'(\(text "[^"]*"\s*\(exclude_from_sim no\)\s*\(at [^)]*\)\s*\(effects\s*\(font\s*\(size 2 2\)\s*\(bold yes\)\s*\))',
               r'\1 (justify left)', s)
    s = re.sub(r'\(property "private" "([^"]*)" ([^"\n][^\n]*?)\s*\n', r'(property private "\1" "\2"\n', s)
    OUT.write_text(s, encoding="utf-8")
    return OUT


if __name__ == "__main__":
    print(render())
