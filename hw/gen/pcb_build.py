"""Bygger kortet ur circuit.py och layout.py: footprints, nät, placering, kontur, text, nätklasser och zoner.
Körs med KiCads Python:  F:/KiCad/bin/python.exe hw/gen/pcb_build.py
(Routningen görs av hw/gen/route.py.)
"""
from __future__ import annotations
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hw" / "gen"))

import pcbnew
from circuit import COMPS, POWER_1MM, POWER_06
import layout as L

PRJ = ROOT / "hw" / "bms-kort"
PCB = PRJ / "bms-kort.kicad_pcb"
KFP = pathlib.Path("F:/KiCad/share/kicad/footprints")
OWN = ROOT / "hw" / "lib" / "bms.pretty"
IO = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
OX, OY = 50.0, 50.0            # kortets origo på ritytan (mm)


def V(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(float(x) + OX), pcbnew.FromMM(float(y) + OY))


def load_fp(fpid: str):
    lib, name = fpid.split(":")
    path = OWN if lib == "bms" else KFP / f"{lib}.pretty"
    fp = IO.FootprintLoad(str(path), name)
    if fp is None:
        raise RuntimeError(f"footprint saknas: {fpid}")
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def add_zone(board, net, layer, rect, priority=0, keepout=False):
    x0, y0, x1, y1 = rect
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    if keepout:
        z.SetIsRuleArea(True)
        z.SetDoNotAllowZoneFills(True)
        z.SetDoNotAllowTracks(False)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
    else:
        z.SetNet(net)
        z.SetAssignedPriority(priority)
        z.SetLocalClearance(pcbnew.FromMM(0.3))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        z.SetThermalReliefGap(pcbnew.FromMM(0.4))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.5))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # SMD helt, hålmonterat med ekrar (lödning)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        ol.Append(V(x, y))
    board.Add(z)
    return z


def write_project():
    pro = PRJ / "bms-kort.kicad_pro"
    data = json.loads(pro.read_text()) if pro.exists() else {}
    base = {"name": "Default", "clearance": 0.2, "track_width": 0.25, "via_diameter": 0.6, "via_drill": 0.3,
            "microvia_diameter": 0.3, "microvia_drill": 0.1, "diff_pair_width": 0.2, "diff_pair_gap": 0.25,
            "wire_width": 6, "bus_width": 12, "line_style": 0, "schematic_color": "rgba(0, 0, 0, 0.000)",
            "pcb_color": "rgba(0, 0, 0, 0.000)", "priority": 2147483647}
    classes = [base,
               {**base, "name": "Kraft", "track_width": 0.8, "via_diameter": 0.8, "via_drill": 0.4, "priority": 0},
               {**base, "name": "Matning", "track_width": 0.6, "via_diameter": 0.8, "via_drill": 0.4, "priority": 1}]
    patterns = [{"netclass": "Kraft", "pattern": n} for n in POWER_1MM] + \
               [{"netclass": "Matning", "pattern": n} for n in POWER_06]
    data.setdefault("meta", {"filename": "bms-kort.kicad_pro", "version": 3})
    data["net_settings"] = {"classes": classes, "meta": {"version": 4}, "net_colors": None,
                            "netclass_assignments": None, "netclass_patterns": patterns}
    ds = data.setdefault("board", {}).setdefault("design_settings", {})
    ds.setdefault("rules", {}).update({
        "min_clearance": 0.15, "min_track_width": 0.15, "min_via_diameter": 0.5, "min_via_annular_width": 0.1,
        "min_through_hole_diameter": 0.3, "min_hole_clearance": 0.25, "min_hole_to_hole": 0.25,
        "min_copper_edge_clearance": 0.4, "min_silk_clearance": 0.0, "min_text_height": 0.6})
    # Antennen: modulens gårdsbox sticker ut över kanten med USB-kontakten (avsiktligt)
    sev = ds.setdefault("rule_severities", {})
    sev.update({"silk_overlap": "ignore", "silk_over_copper": "ignore", "silk_edge_clearance": "ignore",
                "lib_footprint_issues": "ignore", "lib_footprint_mismatch": "ignore",
                "courtyards_overlap": "error", "footprint_symbol_mismatch": "ignore",
                "starved_thermal": "warning"})
    data.setdefault("libraries", {"pinned_footprint_libs": [], "pinned_symbol_libs": []})
    pro.write_text(json.dumps(data, indent=2))
    (PRJ / "fp-lib-table").write_text('(fp_lib_table\n  (version 7)\n  (lib (name "bms")(type "KiCad")'
                                      '(uri "${KIPRJMOD}/../lib/bms.pretty")(options "")(descr "BMS-kortets egna"))\n)\n')
    (PRJ / "sym-lib-table").write_text('(sym_lib_table\n  (version 7)\n  (lib (name "bms")(type "KiCad")'
                                       '(uri "${KIPRJMOD}/../lib/bms.kicad_sym")(options "")(descr "BMS-kortets egna"))\n)\n')


def build():
    PRJ.mkdir(parents=True, exist_ok=True)
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)
    nets = {}

    def net(name):
        if name not in nets:
            ni = pcbnew.NETINFO_ITEM(board, name)
            board.Add(ni)
            nets[name] = ni
        return nets[name]

    for c in COMPS:
        fp = load_fp(c.footprint)
        fp.SetReference(c.ref)
        fp.SetValue(c.value)
        for pad in fp.Pads():
            num = pad.GetNumber()
            if num == "":
                continue
            if num not in c.pins:
                raise RuntimeError(f"{c.ref}: pad {num} saknas i circuit.py ({c.footprint})")
            if c.pins[num]:
                pad.SetNet(net(c.pins[num]))
        x, y, rot = L.PLACE[c.ref]
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        # JLC monterar bara SMD; hålmonterat och kopparfigurer markeras så att CPL/BOM blir rätt
        if c.asm != "SMD":
            fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
        if c.asm == "NONE":
            fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_EXCLUDE_FROM_BOM)
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(0.8), pcbnew.FromMM(0.8)))
        fp.Reference().SetTextThickness(pcbnew.FromMM(0.12))
        if re.match(r"(J|JP|H|TP)\d", c.ref):          # funktionsetiketterna i layout.SILK räcker
            fp.Reference().SetVisible(False)
        board.Add(fp)

    # kontur med rundade hörn (r = 2 mm)
    r = 2.0
    segs = [((r, 0), (L.W - r, 0)), ((L.W, r), (L.W, L.H - r)), ((L.W - r, L.H), (r, L.H)), ((0, L.H - r), (0, r))]
    for a, b in segs:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*a))
        s.SetEnd(V(*b))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(pcbnew.FromMM(0.1))
        board.Add(s)
    for cx, cy, start in ((r, r, (0, r)), (L.W - r, r, (L.W - r, 0)), (L.W - r, L.H - r, (L.W, L.H - r)),
                          (r, L.H - r, (r, L.H))):
        a = pcbnew.PCB_SHAPE(board)
        a.SetShape(pcbnew.SHAPE_T_ARC)
        a.SetCenter(V(cx, cy))
        a.SetStart(V(*start))
        a.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T))
        a.SetLayer(pcbnew.Edge_Cuts)
        a.SetWidth(pcbnew.FromMM(0.1))
        board.Add(a)

    for txt, x, y, size, rot in L.SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(txt)
        t.SetPosition(V(x, y))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(size), pcbnew.FromMM(size)))
        t.SetTextThickness(pcbnew.FromMM(size * 0.15))
        t.SetTextAngleDegrees(rot)
        board.Add(t)

    # GND-plan på båda lagren och kopparfritt under antennen
    full = (0.3, 0.3, L.W - 0.3, L.H - 0.3)
    add_zone(board, net("GND"), pcbnew.B_Cu, full)
    add_zone(board, net("GND"), pcbnew.F_Cu, full)
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        add_zone(board, None, layer, L.ANTENNA_KEEPOUT, keepout=True)

    board.GetDesignSettings().SetAuxOrigin(V(0, L.H))      # JLC: origo i nedre vänstra hörnet
    board.SetFileName(str(PCB))
    pcbnew.SaveBoard(str(PCB), board)
    pcbnew.SaveBoard(str(PRJ / "placerad.kicad_pcb"), board)     # utgångsläge för route.py
    write_project()          # efter SaveBoard, som annars skriver över reglerna


if __name__ == "__main__":
    build()
    print("sparad", PCB)
