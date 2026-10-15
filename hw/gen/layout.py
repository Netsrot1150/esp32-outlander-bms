"""Placering av BMS-kortet (mm, origo uppe till vänster, y nedåt; vinkel moturs i grader som i KiCad).

Tanken bakom placeringen:
  - Alla kablar kommer in längs underkanten: J1 12 V in, J2 CMU-matning och J3 CAN bredvid varandra (J2 och
    J3 går till samma CMU-kabelhärva), J4 strömsensorn längst till höger. Pluggarna sticker ut över kanten.
  - SuperMini uppe till höger med USB-C över högerkanten (programmering) och antennen mot kortets mitt,
    där inget kopparplan läggs (ANTENNA_KEEPOUT). Under modulen står inga komponenter.
  - Effektvägen går nerifrån och upp: J1 -> Q1 (polaritet) -> D1/C1 -> VIN, som delar sig till
    omvandlaren uppe till vänster och BMS-brytaren Q2 rakt ovanför J2. F1 står i linje med J2.
  - Omvandlarens slinga (C3 - U1 - L1 - C5/C6) är så liten som SOT-23-6 tillåter; utgången går via D2
    till 5V-stiftet, som sitter i modulens nedre högra hörn.
  - CAN-transceivern sitter mellan J3 och ESP-stiften IO3/IO4 med CANL/CANH i samma ordning som J3, så
    att ledningarna inte korsar. D4 (ESD) sitter mellan transceivern och kontakten.
  - 12 V-delarens övre motstånd (R14) sitter vid VIN-noden, så att bara den tunna mätledaren går över
    kortet; R15/C12 sitter vid IO1 och filtrerar den (17 ms). RC-filtret för strömsensorn sitter vid IO0.
"""
W, H = 74.0, 56.0

# Modulens mitt: USB-änden 1 mm innanför högerkanten (kontakten sticker ut 1,9 mm)
U4_X, U4_Y = W - 11.26 - 1.0, 17.0

PLACE = {
    # ref: (x, y, vinkel)
    "U4": (U4_X, U4_Y, 0),
    # kontakter längs underkanten (plintkroppen slutar vid kanten)
    "J1": (12.0, 45.4, 0),
    "J2": (25.5, 45.4, 0),
    "J3": (38.4, 47.4, 0),
    "J4": (53.0, 51.5, 0),
    "J5": (71.6, 29.5, 0),
    "JP1": (54.0, 44.0, 90),
    # ingång
    "Q1": (12.0, 39.6, 270),
    "R1": (12.0, 36.4, 0),
    "R2": (14.8, 39.9, 270),
    "D1": (11.5, 32.4, 0),
    "C1": (17.6, 33.2, 270),
    "C2": (15.4, 36.9, 270),
    # omvandlare
    "U1": (15.0, 17.0, 0),
    "C4": (15.0, 14.5, 0),
    "L1": (21.2, 14.2, 0),
    "C3": (18.7, 19.4, 270),
    "R3": (12.6, 20.4, 0),
    "R4": (12.6, 22.0, 0),
    "R5": (21.6, 19.0, 270),
    "R6": (21.6, 22.2, 270),
    "C5": (25.4, 14.4, 270),
    "C6": (27.4, 14.4, 270),
    "D2": (31.2, 13.6, 180),
    "C7": (35.0, 14.4, 270),
    # BMS-skena
    "Q2": (25.5, 33.0, 270),
    "R7": (25.5, 29.9, 0),
    "R8": (28.4, 31.4, 90),
    "Q3": (31.4, 32.5, 180),
    "R9": (36.0, 33.0, 180),
    "R10": (33.4, 36.0, 270),
    "F1": (25.5, 39.3, 270),
    "C8": (22.3, 41.4, 270),
    "D5": (19.6, 40.3, 90),
    "R11": (29.4, 39.6, 0),
    "D3": (32.6, 39.6, 180),
    "U2": (41.5, 37.5, 0),
    "C9": (36.8, 40.6, 0),
    "C10": (40.6, 33.2, 0),
    # CAN
    "U3": (49.2, 38.0, 270),
    "C11": (48.6, 33.2, 0),
    "R12": (52.6, 32.6, 270),
    "D4": (44.0, 42.9, 270),
    "R13": (50.6, 44.0, 0),
    # mätning vid IO0/IO1
    "R16": (51.3, 28.2, 90),
    "C13": (52.9, 28.2, 270),
    "R14": (20.6, 34.6, 0),
    "R15": (56.2, 28.2, 270),
    "C12": (57.8, 28.2, 270),
    # hål och mätpunkter
    "H1": (4.0, 4.0, 0), "H2": (W - 4.0, 4.0, 0), "H3": (4.0, H - 4.0, 0), "H4": (W - 4.0, H - 4.0, 0),
    "TP1": (7.6, 36.0, 0),      # VIN
    "TP2": (38.6, 13.6, 0),     # +5V
    "TP3": (22.0, 36.6, 0),     # VBMS
    "TP4": (44.6, 32.6, 0),     # +3V3_BMS
    "TP5": (7.6, 40.0, 0),      # GND
}

# Ingen kopparyta under antennen (båda lagren). Ledare till IO0/IO21 får passera.
ANTENNA_KEEPOUT = (U4_X - 11.26 - 1.0, U4_Y - 9.0 - 0.3, U4_X - 11.26 + 2.9, U4_Y + 9.0 + 0.3)

# Text på kortet: (text, x, y, storlek, vinkel)
SILK = [
    ("BMS Transporter  rev A", 18.0, 4.2, 1.2, 0),
    ("ESP32-C3 SuperMini  -  TWAI/SN65HVD230", 18.0, 6.2, 0.8, 0),
    ("+12V  IN  GND", 14.5, 42.3, 0.8, 0),
    ("CMU 12V  GND", 28.0, 42.6, 0.8, 0),
    ("GND CANL CANH", 41.9, 44.6, 0.7, 0),
    ("3V3 GND AO", 55.5, 47.8, 0.7, 0),
    ("WCS1700", 55.5, 46.8, 0.7, 0),
    ("120R", 55.3, 41.8, 0.7, 0),
    ("3V3 GND IO6 IO7 IO20", 69.3, 34.5, 0.7, 90),
    ("J5", 71.6, 26.9, 0.8, 0),
    ("VIN", 7.6, 34.4, 0.7, 0),
    ("GND", 7.6, 41.6, 0.7, 0),
    ("5V", 38.6, 11.9, 0.7, 0),
    ("VBMS", 22.1, 38.7, 0.6, 0),
    ("3V3", 44.6, 30.9, 0.7, 0),
    ("USB-C ->", 64.0, 5.4, 0.7, 0),
]
