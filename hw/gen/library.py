"""Genererar projektets egna bibliotek: hw/lib/bms.pretty (footprint) och hw/lib/bms.kicad_sym (symboler).

ESP32-C3 SuperMini enligt Nologos måttritning: 18,00 x 22,52 mm, raderna 15,24 mm isär, första stiftet
1,57 mm från USB-änden, 2,54 mm delning. Footprinten är ritad med USB-C mot +x. Sett ovanifrån med USB
uppåt sitter 5V, G, 3.3, 4, 3, 2, 1, 0 i högra raden (här y = +7,62) och 5, 6, 7, 8, 9, 10, 20, 21 i
vänstra (y = -7,62). Antennen sitter i änden mot -x.
Kör: python hw/gen/library.py
"""
from __future__ import annotations
import pathlib, uuid

ROOT = pathlib.Path(__file__).resolve().parents[2]
LIB = ROOT / "hw" / "lib"

MOD_L, MOD_W = 22.52, 18.00
ROW = 15.24 / 2
FIRST = MOD_L / 2 - 1.57                     # x för stiften närmast USB
ROW_5V = ["5V", "G", "3V3", "4", "3", "2", "1", "0"]          # pad 1-8, y = +ROW
ROW_IO = ["5", "6", "7", "8", "9", "10", "20", "21"]          # pad 9-16, y = -ROW
ANT_X = -MOD_L / 2 + 2.9                     # antennen upptar x < ANT_X


def pin_xy(pad: int) -> tuple[float, float]:
    k = (pad - 1) % 8
    return round(FIRST - 2.54 * k, 3), (ROW if pad <= 8 else -ROW)


def _u():
    return str(uuid.uuid4())


def _line(x0, y0, x1, y1, layer, w=0.12):
    return (f'  (fp_line (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (stroke (width {w}) (type solid)) '
            f'(layer "{layer}") (uuid "{_u()}"))')


def _rect(x0, y0, x1, y1, layer, w=0.12):
    return (f'  (fp_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (stroke (width {w}) (type solid)) '
            f'(fill no) (layer "{layer}") (uuid "{_u()}"))')


def _text(kind, txt, x, y, layer, size=0.8, rot=0, hide=False):
    h = " hide" if hide else ""
    head = f'(fp_text {kind} "{txt}"' if kind == "user" else f'(property "{kind}" "{txt}"'
    return (f'  {head} (at {x:.3f} {y:.3f} {rot}) (layer "{layer}"){h} (uuid "{_u()}") '
            f'(effects (font (size {size} {size}) (thickness {size * 0.15:.3f}))))')


def supermini_footprint() -> str:
    hx, hy = MOD_L / 2, MOD_W / 2
    out = ['(footprint "ESP32C3_SuperMini_Socket"', '  (version 20240108)', '  (generator "bms-hw-gen")',
           '  (layer "F.Cu")',
           '  (descr "ESP32-C3 SuperMini på två hylslister 1x8 2,54 mm, raderna 15,24 mm isär. USB-C mot +x, '
           'antenn mot -x")',
           '  (attr through_hole)',
           _text("Reference", "U4", 0, -hy - 1.2, "F.SilkS", 1.0),
           _text("Value", "ESP32-C3 SuperMini", 0, 0, "F.Fab", 1.0)]
    # modulens kontur, USB-C och antenn
    out.append(_rect(-hx, -hy, hx, hy, "F.SilkS"))
    out.append(_rect(hx - 0.6, -4.5, hx + 1.9, 4.5, "F.Fab", 0.1))
    out.append(_text("user", "USB-C", hx - 2.6, 0, "F.SilkS", 0.8, 90))
    out.append(_rect(-hx, -hy, ANT_X, hy, "F.Fab", 0.1))
    out.append(_text("user", "ANT", -hx + 1.45, 0, "F.SilkS", 0.8, 90))
    out.append(_text("user", "ESP32-C3 SuperMini", 0, 0.0, "F.SilkS", 1.0))
    out.append(_text("user", "komponentsidan upp", 0, 1.6, "F.SilkS", 0.7))
    # gårdsbox: hela modulen inklusive USB-kontakten (den sitter 8,5 mm upp, inget får stå under)
    out.append(_rect(-hx - 0.3, -hy - 0.3, hx + 2.2, hy + 0.3, "F.CrtYd", 0.05))
    for pad in range(1, 17):
        x, y = pin_xy(pad)
        shape = "rect" if pad == 1 else "circle"
        out.append(f'  (pad "{pad}" thru_hole {shape} (at {x:.3f} {y:.3f}) (size 1.7 1.7) (drill 1.0) '
                   f'(layers "*.Cu" "*.Mask") (remove_unused_layers no) (uuid "{_u()}"))')
        lab = (ROW_5V + ROW_IO)[pad - 1]
        out.append(_text("user", lab, x, y + (-2.0 if y > 0 else 2.0), "F.SilkS", 0.75))
    out.append(')')
    return "\n".join(out) + "\n"


# --- symboler ---------------------------------------------------------------------------------
def _pin(num, name, x, y, ang, kind="passive", length=2.54):
    return (f'      (pin {kind} line (at {x} {y} {ang}) (length {length}) '
            f'(name "{name}" (effects (font (size 1.27 1.27)))) (number "{num}" (effects (font (size 1.27 1.27)))))')


def _symbol(name, ref, value, fp, desc, body, pins):
    x0, y0, x1, y1 = body
    return "\n".join([
        f'  (symbol "{name}" (exclude_from_sim no) (in_bom yes) (on_board yes)',
        f'    (property "Reference" "{ref}" (at {x0} {y1 + 1.27} 0) (effects (font (size 1.27 1.27)) (justify left)))',
        f'    (property "Value" "{value}" (at {x0} {y0 - 1.27} 0) (effects (font (size 1.27 1.27)) (justify left)))',
        f'    (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        f'    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        f'    (property "Description" "{desc}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        f'    (symbol "{name}_0_1"',
        f'      (rectangle (start {x0} {y1}) (end {x1} {y0}) (stroke (width 0.254) (type default)) (fill (type background)))',
        '    )',
        f'    (symbol "{name}_1_1"',
        *pins,
        '    )',
        '  )'])


def symbols() -> str:
    tps = _symbol("TPS560430X", "U", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
                  "4-36 V 600 mA synkron buck, PFM", (-7.62, -5.08, 7.62, 5.08),
                  [_pin(5, "VIN", -10.16, 2.54, 0, "power_in"), _pin(4, "EN", -10.16, 0, 0, "input"),
                   _pin(2, "GND", -10.16, -2.54, 0, "power_in"), _pin(1, "CB", 10.16, 2.54, 180),
                   _pin(6, "SW", 10.16, 0, 180, "power_out"), _pin(3, "FB", 10.16, -2.54, 180, "input")])
    pins = []
    for pad in range(1, 9):
        pins.append(_pin(pad, ROW_5V[pad - 1] if ROW_5V[pad - 1] in ("5V", "G", "3V3") else "IO" + ROW_5V[pad - 1],
                         -12.7, 8.89 - 2.54 * (pad - 1), 0, "bidirectional"))
    for pad in range(9, 17):
        pins.append(_pin(pad, "IO" + ROW_IO[pad - 9], 12.7, 8.89 - 2.54 * (pad - 9), 180, "bidirectional"))
    sm = _symbol("ESP32C3_SuperMini", "U", "ESP32-C3 SuperMini", "bms:ESP32C3_SuperMini_Socket",
                 "ESP32-C3 SuperMini på hylslist", (-10.16, -11.43, 10.16, 11.43), pins)
    return "(kicad_symbol_lib (version 20231120) (generator \"bms-hw-gen\")\n" + tps + "\n" + sm + "\n)\n"


def main():
    (LIB / "bms.pretty").mkdir(parents=True, exist_ok=True)
    (LIB / "bms.pretty" / "ESP32C3_SuperMini_Socket.kicad_mod").write_text(supermini_footprint(), encoding="utf-8")
    (LIB / "bms.kicad_sym").write_text(symbols(), encoding="utf-8")
    print("skrev", LIB)


if __name__ == "__main__":
    main()
