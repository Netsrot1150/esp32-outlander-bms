"""Routar kortet: Freerouting för alla signal- och matningsnät, GND via planen på båda lagren och stygnvior.
Körs med KiCads Python efter pcb_build.py:  F:/KiCad/bin/python.exe hw/gen/route.py
Placeringen sparas först som placerad.kicad_pcb så att routningen kan göras om utan att bygga om.
"""
from __future__ import annotations
import math, os, pathlib, re, shutil, subprocess, sys, tempfile, time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hw" / "gen"))

import pcbnew
import layout as L
from pcb_build import write_project

PRJ = ROOT / "hw" / "bms-kort"
PCB = PRJ / "bms-kort.kicad_pcb"
PLACED = PRJ / "placerad.kicad_pcb"
# Freerouting 2.x: sätt FREEROUTING_JAR eller lägg jar-filen i hw/tools/ (ingår inte i repot)
JAR = pathlib.Path(os.environ.get("FREEROUTING_JAR", ROOT / "hw" / "tools" / "freerouting.jar"))
CLI = r"F:\KiCad\bin\kicad-cli.exe"
OX, OY = 50.0, 50.0
VIA_D, VIA_DRILL = 0.6, 0.3
STITCH_PITCH = 3.5


def V(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x + OX), pcbnew.FromMM(y + OY))


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def autoroute(board, passes=40, timeout_s=900):
    tmp = pathlib.Path(tempfile.gettempdir()) / "bms_kort_route"
    tmp.mkdir(exist_ok=True)
    dsn, ses = tmp / "b.dsn", tmp / "b.ses"
    if ses.exists():
        ses.unlink()
    # GND routas inte: den ansluts av planen (F.Cu och B.Cu) och stygnviorna efteråt
    keep = [z for z in board.Zones()]
    for z in keep:
        board.Remove(z)
    ok = pcbnew.ExportSpecctraDSN(board, str(dsn))
    for z in keep:
        board.Add(z)
    if not ok:
        raise RuntimeError("DSN-export misslyckades")
    t = dsn.read_text(encoding="utf-8", errors="replace")
    # Ta bort GND ur nätlistan: dess pads blir hinder utan nät, som de andra näten håller avstånd till
    i = t.index("(net GND")
    depth, j = 0, i
    while True:
        depth += {"(": 1, ")": -1}.get(t[j], 0)
        j += 1
        if depth == 0:
            break
    t = t[:i] + t[j:]
    t = re.sub(r"(\(class \S+[^(]*?)\sGND\b", r"\1", t)
    dsn.write_text(t, encoding="utf-8")
    jt = tmp / "jt"
    jt.mkdir(exist_ok=True)
    cmd = ["java", "-Xmx2g", f"-Djava.io.tmpdir={jt}", "-jar", str(JAR), "-de", str(dsn), "-do", str(ses),
           "-mp", str(passes), "-mt", "1", f"--router.max_passes={passes}", "--gui.enabled=false",
           "--router.optimizer.max_passes=5", "--usage_and_diagnostic_data.disable_analytics=true",
           "--profile.allow_telemetry=false"]
    log = open(tmp / "fr.log", "w")
    p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
    try:
        p.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        subprocess.run(["taskkill", "/F", "/T", "/PID", str(p.pid)], capture_output=True)
    if not ses.exists():
        raise RuntimeError(f"Freerouting gav ingen SES (logg: {tmp / 'fr.log'})")
    if not pcbnew.ImportSpecctraSES(board, str(ses)):
        raise RuntimeError("SES-import misslyckades")


def gnd_zones(board):
    return [z for z in board.Zones() if not z.GetIsRuleArea() and z.GetNetname() == "GND"]


def stitch(board):
    """GND-vior där båda lagrens fyllda GND-yta täcker hela vian med marginal (då är isolationen mot annan
    koppar redan uppfylld av zonens 0,3 mm). Inte i antennområdet."""
    gnd = board.FindNet("GND")
    zones = gnd_zones(board)
    ko = L.ANTENNA_KEEPOUT
    r = VIA_D / 2 + 0.25
    ring = [(r * math.cos(a), r * math.sin(a)) for a in [k * math.pi / 6 for k in range(12)]]
    holes = [(pcbnew.ToMM(p.GetPosition().x) - OX, pcbnew.ToMM(p.GetPosition().y) - OY)
             for fp in board.GetFootprints() for p in fp.Pads() if p.GetDrillSize().x > 0]
    vias = [(pcbnew.ToMM(t.GetPosition().x) - OX, pcbnew.ToMM(t.GetPosition().y) - OY)
            for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    n = 0
    y = 1.5
    while y < L.H - 1.5:
        x = 1.5
        while x < L.W - 1.5:
            ok = not (ko[0] - 0.5 < x < ko[2] + 0.5 and ko[1] - 0.5 < y < ko[3] + 0.5)
            ok = ok and all(math.hypot(x - hx, y - hy) > 2.0 for hx, hy in holes)
            ok = ok and all(math.hypot(x - vx, y - vy) > 1.2 for vx, vy in vias)
            if ok:
                for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
                    if not any(all(z.HitTestFilledArea(layer, V(x + dx, y + dy)) for dx, dy in ring)
                               for z in zones if z.GetLayer() == layer):
                        ok = False
                        break
            if ok:
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(V(x, y))
                v.SetWidth(pcbnew.FromMM(VIA_D))
                v.SetDrill(pcbnew.FromMM(VIA_DRILL))
                v.SetNet(gnd)
                board.Add(v)
                vias.append((x, y))
                n += 1
            x += STITCH_PITCH
        y += STITCH_PITCH
    return n


def _free(board, shape, layers, clearance=0.25, skip_net="GND"):
    """True om formen inte krockar med koppar från andra nät på de givna lagren."""
    cl = pcbnew.FromMM(clearance)
    for t in board.GetTracks():
        if t.GetNetname() == skip_net:
            continue
        for layer in layers:
            if t.IsOnLayer(layer) and t.GetEffectiveShape(layer).Collide(shape, cl):
                return False
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetNetname() == skip_net and p.GetDrillSize().x == 0:
                continue
            for layer in layers:
                if p.IsOnLayer(layer) and p.GetEffectiveShape(layer).Collide(shape, cl):
                    return False
            if p.GetDrillSize().x > 0 and p.GetEffectiveHoleShape().Collide(shape, pcbnew.FromMM(0.3)):
                return False
    return True


def fanout_pad(board, pad) -> bool:
    """Via till B.Cu-planet bredvid en GND-pad som hamnat på en kopparö, med kort ledare."""
    gnd = board.FindNet("GND")
    c = pad.GetPosition()
    fpc = pad.GetParentFootprint().GetPosition()
    base = math.atan2(c.y - fpc.y, c.x - fpc.x)
    for dist in (1.1, 1.4, 1.8, 2.3):
        for a in [base] + [base + s * k * math.pi / 8 for k in range(1, 9) for s in (1, -1)]:
            p = pcbnew.VECTOR2I(int(c.x + pcbnew.FromMM(dist) * math.cos(a)),
                                int(c.y + pcbnew.FromMM(dist) * math.sin(a)))
            via = pcbnew.SHAPE_CIRCLE(p, pcbnew.FromMM(VIA_D / 2))
            seg = pcbnew.SHAPE_SEGMENT(c, p, pcbnew.FromMM(0.4))
            if _free(board, via, (pcbnew.F_Cu, pcbnew.B_Cu)) and _free(board, seg, (pcbnew.F_Cu,)):
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(p)
                v.SetWidth(pcbnew.FromMM(VIA_D))
                v.SetDrill(pcbnew.FromMM(VIA_DRILL))
                v.SetNet(gnd)
                board.Add(v)
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(c)
                t.SetEnd(p)
                t.SetWidth(pcbnew.FromMM(0.4))
                t.SetLayer(pcbnew.F_Cu)
                t.SetNet(gnd)
                board.Add(t)
                return True
    return False


def fix_gnd_islands(board, rounds=4):
    """Kör DRC, ge varje oansluten GND-pad en egen via, fyll om. Upprepas tills inget återstår."""
    pads = {p.m_Uuid.AsString(): p for fp in board.GetFootprints() for p in fp.Pads()}
    done = 0
    for _ in range(rounds):
        pcbnew.SaveBoard(str(PCB), board)
        write_project()
        d = drc(PCB)
        todo = []
        for x in d.get("unconnected_items", []):
            for it in x["items"]:
                p = pads.get(it.get("uuid", ""))
                if p is not None and p.GetNetname() == "GND" and p.GetDrillSize().x == 0 and p not in todo:
                    todo.append(p)
        if not todo:
            break
        for p in todo:
            if fanout_pad(board, p):
                done += 1
            else:
                print("  ingen plats för GND-via vid", p.GetParentFootprint().GetReference(), p.GetNumber())
        fill(board)
    return done


def stitch_islands(board):
    """Varje fylld GND-ö på F.Cu utan via eller hålmonterad GND-pad får en via (dit B.Cu-planet når)."""
    gnd = board.FindNet("GND")
    fz = [z for z in gnd_zones(board) if z.GetLayer() == pcbnew.F_Cu]
    bz = [z for z in gnd_zones(board) if z.GetLayer() == pcbnew.B_Cu]
    anchors = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
    anchors += [p.GetPosition() for fp in board.GetFootprints() for p in fp.Pads()
                if p.GetNetname() == "GND" and p.GetDrillSize().x > 0]
    r = VIA_D / 2 + 0.25
    ring = [(r * math.cos(k * math.pi / 6), r * math.sin(k * math.pi / 6)) for k in range(12)]
    added = 0
    for z in fz:
        polys = z.GetFilledPolysList(pcbnew.F_Cu)
        for i in range(polys.OutlineCount()):
            if any(polys.Contains(a, i) for a in anchors):
                continue
            bb = polys.Outline(i).BBox()
            x0, y0 = pcbnew.ToMM(bb.GetX()), pcbnew.ToMM(bb.GetY())
            w, h = pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight())
            best = None
            steps = 24
            for a in range(steps + 1):
                for b in range(steps + 1):
                    x, y = x0 + w * a / steps, y0 + h * b / steps
                    pts = [pcbnew.VECTOR2I(pcbnew.FromMM(x + dx), pcbnew.FromMM(y + dy)) for dx, dy in ring]
                    if all(polys.Contains(q, i) for q in pts) and                             any(all(bzz.HitTestFilledArea(pcbnew.B_Cu, q) for q in pts) for bzz in bz):
                        best = (x, y)
                        break
                if best:
                    break
            if best is None:
                print(f"  GND-ö utan plats för via vid ({x0 - OX:.1f}, {y0 - OY:.1f})")
                continue
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(best[0]), pcbnew.FromMM(best[1])))
            v.SetWidth(pcbnew.FromMM(VIA_D))
            v.SetDrill(pcbnew.FromMM(VIA_DRILL))
            v.SetNet(gnd)
            board.Add(v)
            anchors.append(v.GetPosition())
            added += 1
    return added


def drc(path) -> dict:
    import json
    with tempfile.TemporaryDirectory() as td:
        out = pathlib.Path(td) / "d.json"
        subprocess.run([CLI, "pcb", "drc", "--format", "json", "--severity-error", "-o", str(out), str(path)],
                       capture_output=True)
        return json.loads(out.read_text(encoding="utf-8"))


def main():
    if not PLACED.exists():
        raise SystemExit("Kör pcb_build.py först (den skriver placerad.kicad_pcb)")
    # Laddas under projektets namn så att nätklasserna (ledningsbredder) i bms-kort.kicad_pro följer med
    shutil.copy(PLACED, PCB)
    write_project()
    board = pcbnew.LoadBoard(str(PCB))
    t0 = time.time()
    autoroute(board)
    fill(board)
    n = stitch(board)
    fill(board)
    ni = stitch_islands(board)
    fill(board)
    nf = fix_gnd_islands(board)
    print(f"GND: {ni} vior i öar, {nf} utfläktade pads")
    board.BuildConnectivity()
    unrouted = board.GetConnectivity().GetUnconnectedCount(False)
    pcbnew.SaveBoard(str(PCB), board)
    write_project()          # SaveBoard skriver över projektets regler
    d = drc(PCB)
    v = [x for x in d["violations"]]
    print(f"routat på {time.time() - t0:.0f} s, {n} stygnvior, oanslutna {unrouted}, "
          f"DRC-fel {len(v)}, DRC oanslutna {len(d.get('unconnected_items', []))}")
    for x in v[:30]:
        print("  ", x["type"], "|", " / ".join(i["description"][:70] for i in x["items"]))
    for x in d.get("unconnected_items", [])[:30]:
        print("   oansluten |", " / ".join(i["description"][:70] for i in x["items"]))


if __name__ == "__main__":
    main()
