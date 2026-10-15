"""Tillverkningsfiler för JLCPCB: Gerber + borr (zip), BOM och CPL i JLC-format, handlödningslista och bilder.
Kör: python hw/gen/fab.py   (efter pcb_build.py och route.py)
"""
from __future__ import annotations
import csv, io, pathlib, re, shutil, subprocess, sys, tempfile, zipfile
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hw" / "gen"))
from circuit import COMPS

PCB = ROOT / "hw" / "bms-kort" / "bms-kort.kicad_pcb"
SCH = ROOT / "hw" / "bms-kort" / "bms-kort.kicad_sch"
FAB = ROOT / "hw" / "fab"
CLI = r"F:\KiCad\bin\kicad-cli.exe"
LAYERS = "F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts"


def run(*args):
    subprocess.run([CLI, *args], check=True, capture_output=True)


def rotations():
    rows = []
    with open(ROOT / "hw" / "gen" / "cpl_rotations_db.csv", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) >= 2 and row[0].strip():
                rows.append((re.compile(row[0].strip()), float(row[1])))
    return rows


def gerbers():
    out = FAB / "gerber"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    run("pcb", "export", "gerbers", "--layers", LAYERS, "--use-drill-file-origin", "-o", str(out) + "\\", str(PCB))
    run("pcb", "export", "drill", "--format", "excellon", "--drill-origin", "plot", "--excellon-separate-th",
        "-o", str(out) + "\\", str(PCB))
    z = FAB / "bms-kort-gerber.zip"
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in sorted(out.iterdir()):
            zf.write(f, f.name)
    return z


def positions():
    with tempfile.TemporaryDirectory() as td:
        pos = pathlib.Path(td) / "pos.csv"
        run("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "--use-drill-file-origin",
            "-o", str(pos), str(PCB))
        text = pos.read_text(encoding="utf-8")
    return {row["Ref"]: row for row in csv.DictReader(io.StringIO(text))}


def bom_and_cpl():
    pos = positions()
    rots = rotations()
    smd, hand, groups = [], [], defaultdict(list)
    for c in COMPS:
        fpname = c.footprint.split(":")[1]
        if c.asm == "SMD":
            groups[(c.lcsc, c.value, fpname)].append(c.ref)
            r = pos[c.ref]
            rot = float(r["Rot"])
            for rx, delta in rots:
                if rx.search(fpname):
                    rot = (rot + delta) % 360
                    break
            smd.append({"Designator": c.ref, "Mid X": r["PosX"], "Mid Y": r["PosY"],
                        "Layer": "Top" if r["Side"] == "top" else "Bottom", "Rotation": f"{rot:g}"})
        elif c.asm == "HAND":
            hand.append({"Ref": c.ref, "Värde": c.value, "LCSC": c.lcsc or "", "Del": c.mfr, "Not": c.note})
    FAB.mkdir(parents=True, exist_ok=True)
    with open(FAB / "bms-kort-cpl.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writeheader()
        w.writerows(sorted(smd, key=lambda d: d["Designator"]))
    with open(FAB / "bms-kort-bom.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
        for (lcsc, val, fpname), refs in sorted(groups.items(), key=lambda kv: kv[0][0]):
            w.writerow([val, ",".join(sorted(refs, key=lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r))))),
                        fpname, lcsc])
    with open(FAB / "handlodning.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["Ref", "Värde", "LCSC", "Del", "Not"])
        w.writeheader()
        w.writerows(sorted(hand, key=lambda d: d["Ref"]))
    return len(smd), len(groups), len(hand)


def pictures():
    for side in ("top", "bottom"):
        run("pcb", "render", "--side", side, "--width", "2000", "--height", "1500", "--quality", "high",
            "-o", str(FAB / f"render_{side}.png"), str(PCB))
    run("sch", "export", "pdf", "-o", str(FAB / "bms-kort-schema.pdf"), str(SCH))


def main():
    z = gerbers()
    n_smd, n_lines, n_hand = bom_and_cpl()
    pictures()
    print(f"{z.name}, CPL {n_smd} rader, BOM {n_lines} rader, handlödning {n_hand} rader")


if __name__ == "__main__":
    main()
