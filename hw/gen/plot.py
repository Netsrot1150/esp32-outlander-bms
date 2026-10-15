"""Ritar kortets ledare per lager (F.Cu röd, B.Cu blå, pads grå, vior svarta) till PNG för granskning.
Körs med KiCads Python:  F:/KiCad/bin/python.exe hw/gen/plot.py [ut.png]
"""
import pathlib, sys
import pcbnew
from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
PCB = ROOT / "hw" / "bms-kort" / "bms-kort.kicad_pcb"
S = 22                                   # pixlar per mm
OX, OY = 50.0, 50.0


def main(out):
    b = pcbnew.LoadBoard(str(PCB))
    bb = b.GetBoardEdgesBoundingBox()
    w, h = pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight())
    img = Image.new("RGB", (int(w * S) + 40, int(h * S) + 40), "white")
    d = ImageDraw.Draw(img, "RGBA")
    P = lambda v: (20 + (pcbnew.ToMM(v.x) - OX) * S, 20 + (pcbnew.ToMM(v.y) - OY) * S)
    d.rectangle([20, 20, 20 + w * S, 20 + h * S], outline="black", width=2)
    for fp in b.GetFootprints():
        cy = fp.GetCourtyard(pcbnew.F_CrtYd)
        if cy.OutlineCount():
            r = cy.BBox()
            d.rectangle([P(r.GetOrigin()), P(r.GetEnd())], outline=(170, 170, 170), width=1)
        for p in fp.Pads():
            r = p.GetBoundingBox()
            d.rectangle([P(r.GetOrigin()), P(r.GetEnd())], fill=(150, 150, 150, 200))
        c = P(fp.GetPosition())
        d.text((c[0] + 3, c[1] - 14), fp.GetReference(), fill=(0, 120, 0))
    for layer, col in ((pcbnew.B_Cu, (30, 80, 230, 170)), (pcbnew.F_Cu, (220, 30, 30, 190))):
        for t in b.GetTracks():
            if t.GetClass() == "PCB_TRACK" and t.GetLayer() == layer:
                d.line([P(t.GetStart()), P(t.GetEnd())], fill=col, width=max(2, int(pcbnew.ToMM(t.GetWidth()) * S)))
    for t in b.GetTracks():
        if t.GetClass() == "PCB_VIA":
            c = P(t.GetPosition())
            r = pcbnew.ToMM(t.GetWidth()) / 2 * S
            d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r],
                      fill=(0, 0, 0) if t.GetNetname() != "GND" else (120, 120, 120))
    img.save(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "hw" / "bms-kort" / "ledare.png"))
