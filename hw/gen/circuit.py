"""Kretsbeskrivning för BMS-kortet: den enda sanningskällan för schema, mönsterkort och BOM.

Varje komponent har referens, värde, footprint, pad -> nät, symbol (för schemat), LCSC-nummer och
monteringssätt: "SMD" (JLC monterar), "HAND" (hålmonterat, löds för hand) eller "NONE" (bara koppar).

Funktion (se docs/design-bms-kort.md):
  INGANG   12 V från det separata AGM-batteriet, skydd mot omvänd polaritet (Q1) och TVS (D1)
  BUCK     12 -> 5 V alltid på (TPS560430X, PFM, 80 µA) till SuperMini via D2. EN-delaren startar vid
           11,5 V och stänger av under ca 10,3 V: djupurladdningsskydd i hårdvara
  SKENA    BMS-skena (Q2, styrd av GPIO10 via Q3): CMU-matning via polyfuse F1 och 3,3 V (U2) till CAN
           och strömsensorn. Av vid start, krasch och djupsömn
  CAN      SN65HVD230 på C3:ans TWAI (GPIO4 TX, GPIO3 RX), ESD-skydd D4, 120 Ω med bygel JP1
  MATNING  12 V-delare till GPIO1, WCS1700 till GPIO0
  ESP      ESP32-C3 SuperMini på hylslist, expansionslist J5
"""
from __future__ import annotations
from dataclasses import dataclass, field

FP_R = "Resistor_SMD:R_0603_1608Metric"
FP_C = "Capacitor_SMD:C_0603_1608Metric"
FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"
FP_C1206 = "Capacitor_SMD:C_1206_3216Metric"
SOT23 = "Package_TO_SOT_SMD:SOT-23"


@dataclass
class Comp:
    ref: str
    value: str
    footprint: str
    pins: dict            # padnummer -> nät (None = ej ansluten)
    symbol: str           # lib_id för schemat
    lcsc: str | None
    asm: str              # SMD | HAND | NONE
    block: str
    mfr: str = ""
    note: str = ""


def R(ref, value, a, b, lcsc, block, fp=FP_R):
    return Comp(ref, value, fp, {"1": a, "2": b}, "Device:R", lcsc, "SMD", block)


def C(ref, value, a, b, lcsc, block, fp=FP_C):
    return Comp(ref, value, fp, {"1": a, "2": b}, "Device:C", lcsc, "SMD", block)


# Basic-delar hos JLC (ingen extra avgift). 0603 1 %.
R_LCSC = {"1k": "C21190", "4.7k": "C23162", "47k": "C25819", "100k": "C25803", "120k": "C25808",
          "200k": "C25811", "470k": "C23178", "1M": "C22935", "2M": "C22976", "68k": "C23231", "120": "C22787"}
C_100N = "C14663"      # 100 nF 50 V X7R 0603
C_1U = "C15849"        # 1 µF 50 V X5R 0603
C_10U_50 = "C13585"    # 10 µF 50 V X5R 1206
C_2U2_50 = "C50254"    # 2,2 µF 50 V X7R 1206
C_22U_25 = "C45783"    # 22 µF 25 V X5R 0805

COMPS: list[Comp] = [
    # --- INGANG ------------------------------------------------------------------------------
    Comp("J1", "12V IN", "Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_2-G-5,08_1x02_P5.08mm_Horizontal",
         {"1": "VBAT_IN", "2": "GND"}, "Connector_Generic:Conn_01x02", "C441204", "HAND", "INGANG",
         "KF2EDGR-5.08-2P", "Plugg: KF2EDGK-5.08-2P (C440880)"),
    # P-FET mot omvänd polaritet: D mot ingången, S mot kretsen. AO3401A tål ±12 V gate-source, så gaten
    # får en tredjedel av Vin: 10,3 V -> 3,4 V (helt på), 29 V (TVS-klämning) -> 9,7 V. Delaren drar 4 µA.
    Comp("Q1", "AO3401A", SOT23, {"1": "Q1_G", "2": "VIN", "3": "VBAT_IN"}, "Transistor_FET:Q_PMOS_GSD",
         "C15127", "SMD", "INGANG", "AO3401A"),
    R("R1", "1M", "VIN", "Q1_G", R_LCSC["1M"], "INGANG"),
    R("R2", "2M", "Q1_G", "GND", R_LCSC["2M"], "INGANG"),
    Comp("D1", "SMBJ18A", "Diode_SMD:D_SMB", {"1": "VIN", "2": "GND"}, "Device:D_Zener", "C353379", "SMD",
         "INGANG", "SMBJ18A"),
    C("C1", "10u 50V", "VIN", "GND", C_10U_50, "INGANG", FP_C1206),
    C("C2", "100n", "VIN", "GND", C_100N, "INGANG"),
    # --- BUCK --------------------------------------------------------------------------------
    Comp("U1", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
         {"1": "BUCK_CB", "2": "GND", "3": "BUCK_FB", "4": "BUCK_EN", "5": "VIN", "6": "BUCK_SW"},
         "bms:TPS560430X", "C524782", "SMD", "BUCK", "TPS560430XDBVR"),
    C("C3", "2.2u 50V", "VIN", "GND", C_2U2_50, "BUCK", FP_C1206),
    C("C4", "100n", "BUCK_CB", "BUCK_SW", C_100N, "BUCK"),
    Comp("L1", "22u", "Inductor_SMD:L_Sunlord_SWPA5040S", {"1": "BUCK_SW", "2": "+5V_BUCK"}, "Device:L",
         "C68434", "SMD", "BUCK", "SWPA5040S220MT"),
    C("C5", "22u 25V", "+5V_BUCK", "GND", C_22U_25, "BUCK", FP_C0805),
    C("C6", "22u 25V", "+5V_BUCK", "GND", C_22U_25, "BUCK", FP_C0805),
    R("R3", "470k", "+5V_BUCK", "BUCK_FB", R_LCSC["470k"], "BUCK"),     # Vut = 1,0 * (1 + 470/120) = 4,92 V
    R("R4", "120k", "BUCK_FB", "GND", R_LCSC["120k"], "BUCK"),
    R("R5", "1M", "VIN", "BUCK_EN", R_LCSC["1M"], "BUCK"),              # på 1,23 * 9,33 = 11,5 V, av 10,3 V
    R("R6", "120k", "BUCK_EN", "GND", R_LCSC["120k"], "BUCK"),
    Comp("D2", "B5819W", "Diode_SMD:D_SOD-123", {"1": "+5V", "2": "+5V_BUCK"}, "Device:D_Schottky",
         "C8598", "SMD", "BUCK", "B5819W SL"),
    C("C7", "22u 25V", "+5V", "GND", C_22U_25, "BUCK", FP_C0805),
    # --- SKENA -------------------------------------------------------------------------------
    # Gate-delare 47k/68k: |Vgs| = 0,41 * Vin (10,3 V -> 4,2 V, 29 V -> 11,9 V)
    Comp("Q2", "AO3401A", SOT23, {"1": "Q2_G", "2": "VIN", "3": "VBMS_RAW"}, "Transistor_FET:Q_PMOS_GSD",
         "C15127", "SMD", "SKENA", "AO3401A"),
    R("R7", "47k", "VIN", "Q2_G", R_LCSC["47k"], "SKENA"),
    R("R8", "68k", "Q2_G", "Q3_D", R_LCSC["68k"], "SKENA"),
    Comp("Q3", "2N7002", SOT23, {"1": "Q3_G", "2": "GND", "3": "Q3_D"}, "Transistor_FET:Q_NMOS_GSD", "C8545",
         "SMD", "SKENA", "2N7002"),
    R("R9", "1k", "BMS_EN", "Q3_G", R_LCSC["1k"], "SKENA"),
    R("R10", "100k", "Q3_G", "GND", R_LCSC["100k"], "SKENA"),
    Comp("F1", "2A 30V", "Fuse:Fuse_1812_4532Metric", {"1": "VBMS_RAW", "2": "VBMS"}, "Device:Polyfuse",
         "C18198342", "SMD", "SKENA", "1812L200/30GR"),
    C("C8", "100n", "VBMS", "GND", C_100N, "SKENA"),
    # frihjulsdiod: kabelhärvans induktans ger en negativ spik på VBMS när Q2 bryter
    Comp("D5", "B5819W", "Diode_SMD:D_SOD-123", {"1": "VBMS", "2": "GND"}, "Device:D_Schottky",
         "C8598", "SMD", "SKENA", "B5819W SL"),
    Comp("J2", "CMU 12V", "Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_2-G-5,08_1x02_P5.08mm_Horizontal",
         {"1": "VBMS", "2": "GND"}, "Connector_Generic:Conn_01x02", "C441204", "HAND", "SKENA",
         "KF2EDGR-5.08-2P", "Plugg: KF2EDGK-5.08-2P (C440880)"),
    R("R11", "4.7k", "VBMS", "LED_A", R_LCSC["4.7k"], "SKENA"),
    Comp("D3", "BMS", "LED_SMD:LED_0805_2012Metric", {"1": "GND", "2": "LED_A"}, "Device:LED", "C2297", "SMD",
         "SKENA", "KT-0805G"),
    Comp("U2", "L78L33ACUTR", "Package_TO_SOT_SMD:SOT-89-3", {"1": "+3V3_BMS", "2": "GND", "3": "VBMS"},
         "Regulator_Linear:L78L33_SOT89", "C14170", "SMD", "SKENA", "L78L33ACUTR"),
    C("C9", "1u", "VBMS", "GND", C_1U, "SKENA"),
    C("C10", "1u", "+3V3_BMS", "GND", C_1U, "SKENA"),
    # --- CAN ---------------------------------------------------------------------------------
    Comp("U3", "SN65HVD230", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
         {"1": "CAN_TXD", "2": "GND", "3": "+3V3_BMS", "4": "CAN_RX", "5": None, "6": "CANL", "7": "CANH",
          "8": "GND"}, "Interface_CAN_LIN:SN65HVD230", "C12084", "SMD", "CAN", "SN65HVD230DR"),
    C("C11", "100n", "+3V3_BMS", "GND", C_100N, "CAN"),
    R("R12", "1k", "CAN_TX", "CAN_TXD", R_LCSC["1k"], "CAN"),   # begränsar ström in i avstängd transceiver
    Comp("D4", "NUP2105L", SOT23, {"1": "CANH", "2": "CANL", "3": "GND"}, "Power_Protection:NUP2105L",
         "C14486", "SMD", "CAN", "NUP2105LT1G"),
    R("R13", "120", "CANH", "TERM", R_LCSC["120"], "CAN"),
    Comp("JP1", "120R", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", {"1": "TERM", "2": "CANL"},
         "Connector_Generic:Conn_01x02", "C492401", "HAND", "CAN", "PZ254V-11-02P",
         "Bygel på = 120 Ω avslutning"),
    Comp("J3", "CAN", "Connector_Phoenix_MC:PhoenixContact_MC_1,5_3-G-3.5_1x03_P3.50mm_Horizontal",
         {"1": "GND", "2": "CANL", "3": "CANH"}, "Connector_Generic:Conn_01x03", "C5160226", "HAND", "CAN",
         "15EDGRC-3.5-03P", "Plugg: 15EDGK-3.5-03P (C5188250)"),
    # --- MATNING (mätning) -------------------------------------------------------------------
    R("R14", "1M", "VIN", "VBAT_SENSE", R_LCSC["1M"], "MATNING"),       # 15 V -> 2,5 V
    R("R15", "200k", "VBAT_SENSE", "GND", R_LCSC["200k"], "MATNING"),
    C("C12", "100n", "VBAT_SENSE", "GND", C_100N, "MATNING"),
    R("R16", "1k", "CUR_RAW", "CUR_SENSE", R_LCSC["1k"], "MATNING"),
    C("C13", "100n", "CUR_SENSE", "GND", C_100N, "MATNING"),
    Comp("J4", "WCS1700", "Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical",
         {"1": "+3V3_BMS", "2": "GND", "3": "CUR_RAW"}, "Connector_Generic:Conn_01x03", "C144394", "HAND",
         "MATNING", "B3B-XH-A", "1 = 3,3 V, 2 = GND, 3 = AO"),
    # --- ESP ---------------------------------------------------------------------------------
    Comp("U4", "ESP32-C3 SuperMini", "bms:ESP32C3_SuperMini_Socket",
         {"1": "+5V", "2": "GND", "3": "+3V3", "4": "CAN_TX", "5": "CAN_RX", "6": None, "7": "VBAT_SENSE",
          "8": "CUR_SENSE", "9": None, "10": "IO6", "11": "IO7", "12": None, "13": None, "14": "BMS_EN",
          "15": "IO20", "16": None},
         "bms:ESP32C3_SuperMini", "C27438", "HAND", "ESP", "2 st hylslist 1x8 2,54 mm 8,5 mm",
         "Hylslisten (2 st C27438) löds på kortet. Ta bort röda ström-LED:en på SuperMini"),
    Comp("J5", "EXP", "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical",
         {"1": "+3V3", "2": "GND", "3": "IO6", "4": "IO7", "5": "IO20"},
         "Connector_Generic:Conn_01x05", None, "NONE", "ESP", "", "Expansion, monteras vid behov"),
    # --- Mekanik och mätpunkter -----------------------------------------------------------------
    # Opläterade hål utan nät: HV-sidans 12 V är isolerad från karossen och får inte jordas via skruvarna
    *[Comp(f"H{i}", "M3", "MountingHole:MountingHole_3.2mm_M3", {},
           "Mechanical:MountingHole", None, "NONE", "MEK") for i in range(1, 5)],
    *[Comp(f"TP{i}", n, "TestPoint:TestPoint_Pad_D1.5mm", {"1": n}, "Connector:TestPoint", None, "NONE", "MEK")
      for i, n in enumerate(["VIN", "+5V", "VBMS", "+3V3_BMS", "GND"], start=1)],
]

BY_REF = {c.ref: c for c in COMPS}

# Nätklasser: ledningsbredd (mm). Kraft 0,8 mm: 2 A ger ca 10 °C i 1 oz ytterlager (IPC-2221).
POWER_1MM = ["VBAT_IN", "VIN", "VBMS_RAW", "VBMS"]
POWER_06 = ["+5V_BUCK", "+5V", "BUCK_SW", "GND"]


def nets() -> set[str]:
    return {n for c in COMPS for n in c.pins.values() if n}


if __name__ == "__main__":
    print(len(COMPS), "komponenter,", len(nets()), "nät")
    for n in sorted(nets()):
        members = [f"{c.ref}.{p}" for c in COMPS for p, m in c.pins.items() if m == n]
        print(f"  {n:<12} {' '.join(members)}")
