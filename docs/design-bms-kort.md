# BMS-kort rev A – design

Datum: 2026-10-15. Delprojekt 1 av 2 (kortet). Delprojekt 2 är firmware: förvaringsläge, väckning,
månadskontroll med notis, lågspänningsskydd och TWAI-drivrutin.

## Bakgrund

Under en vecka i oktober 2026 stod bilen still med DC-DC-laddaren (Vevor SDC20, på knapp) avstängd.
CMU:erna matas från det separata 12 V-AGM-batteriet och tömde det tills CAN-kretsen slutade svara ca 68 h
av 174 h. ESP:n satt på bilens 12 V via cigg-USB och klarade sig. Kortet ska göra det möjligt att stänga
av allt utom ESP:n, mäta 12 V och hålla sömnströmmen under 0,2 mA.

## Beslut

| Fråga | Beslut |
|---|---|
| Brytning av BMS-matningen | P-MOSFET på plussidan (Q2), styrd av GPIO10 via N-MOSFET (Q3). Av vid start, krasch och djupsömn |
| ESP-matning | Alltid på: TPS560430X (4–36 V, 600 mA, PFM, 80 µA) 12 -> 4,9 V, via Schottky till SuperMini 5V |
| CAN | C3:ans TWAI + SN65HVD230 på BMS-skenan (3,3 V från L78L33). Ersätter MCP2515-modulen |
| ESP | ESP32-C3 SuperMini på hylslist. Ström-LED:en tas bort (sömn ca 43 µA) |
| Extra | WCS1700-ingång (på BMS-skenans 3,3 V), 120 Ω CAN-avslutning med bygel, expansionslist (ej monterad) |
| CMU-ström | Dimensioneras för 2 A (polyfuse 2 A, 1 mm ledare), troligen mindre |
| Tillverkning | JLCPCB, 2 lager, SMD monteras av JLC, hålmonterat löds för hand |

## Skydd

- Omvänd polaritet: Q1 (AO3401A) med gate-delare 1M/2M (|Vgs| = Vin/3, tål ±12 V).
- Spikar: SMBJ18A (klämmer 29 V). Alla delar på VIN tål minst 30 V.
- Djupurladdning i hårdvara: omvandlarens EN-delare startar vid 11,5 V och stänger av under ca 10,3 V.
- CMU-utgång: polyfuse 2 A (1812L200/30).
- CAN: NUP2105L.

## Pinnar (ESP32-C3)

| GPIO | Funktion |
|---|---|
| 0 | Strömsensor (ADC1_CH0, RC 1k/100n) |
| 1 | 12 V-mätning (ADC1_CH1, 1M/200k, 15 V -> 2,5 V) |
| 3 | CAN RX |
| 4 | CAN TX (via 1 kΩ, begränsar ström in i avstängd transceiver) |
| 10 | BMS-skena på |
| 6, 7, 20 | Expansionslist J5 |
| 2, 8, 9 | Lediga (strapping/LED/BOOT) |

Firmware måste sätta GPIO4 låg eller som ingång innan BMS-skenan stängs av.

## Sömnström (uppskattad, från 12 V)

TPS560430X ca 80 µA, SuperMini utan LED ca 43 µA vid 5 V (ca 20 µA från 12 V), delare 4 + 11 + 10 µA.
Summa ca 0,13 mA, alltså ca 0,1 Ah per månad.

## Kontroller före beställning

1. Mät radavståndet på ditt SuperMini (ska vara 15,24 mm) och jämför pinnordningen med kortets tryck.
2. Kontrollera att plintarnas pluggar (KF2EDGK-5.08-2P, 15EDGK-3.5-03P) passar kablarna.
3. Kontrollera WCS1700-modulens stiftordning mot J4 (1 = 3,3 V, 2 = GND, 3 = AO).
