# BMS v2.1 – laddnivå, strömmätning och historik

**Datum:** 2026-10-05
**Status:** Godkänd design, väntar på granskning av specen
**Bygger på:** `2026-10-04-bms-firmware-v2-design.md` (firmware v2.0.5 körs i bilen)

## 1. Mål

- Laddnivå i procent och uppskattad energi kvar.
- Strömmätning med en WCS1700-hallsensor: ström, effekt och Ah/kWh in och ut.
- Historik i 24 h som överlever omstart och OTA, plus de senaste 10 minuterna med 1 s upplösning, visade i grafer.
- Larm för hög ström.
- ESP-NOW-paket version 2 med laddnivå och ström för den kommande skärmen.

## 2. Ingår inte

- LM393-utgången (DO) på sensormodulen. Komparatorn triggar när utsignalen stiger, men sensorn monteras så att urladdning ger sjunkande utsignal. Strömlarmet görs i stället i firmware från den analoga signalen.
- Inlärning av kapaciteten (SoH). Kapaciteten är en inställning, standard 40 Ah.
- Manuell inställning av laddnivån.

## 3. Fakta från hårdvaran

- Laddaren bryter vid 4,10 V/cell (262,4 V för 64S).
- Lasten är normalt under 10 kW, med enstaka toppar över 20 kW. Mätbehovet är ca 10–15 kW, alltså 41–71 A vid 211–246 V.
- Batteriet är kopplat med två parallella +-kablar och två parallella −-kablar, RK 6 mm² (H07V-K, ytterdiameter ca 5–5,5 mm). Två kablar får inte plats i sensorns hål på 9 mm. Det finns dock ställen där strömmen går i **en enda ledare**. Sensorn monteras där, och faktorn blir 1,0 (standard). Faktorn finns kvar som inställning för kalibrering mot laddarens visade ström.

## 4. Strömmätning

### 4.1 Hårdvara

| Sensormodul | ESP32-C3 |
|---|---|
| VCC | 3,3 V |
| GND | GND |
| AO | GPIO3 (ADC1 kanal 3) |
| DO | ej ansluten |

WCS1700 är ratiometrisk. Vid 3,3 V matning ger den ca **22 mV/A**, med nollpunkten ca 1,65 V. Den mäter ±70 A och har 4 kV isolation; högvoltskabeln går bara genom hålet.

**Montering:** urladdning ska ge *sjunkande* utsignal. Då används 1,65 → ca 0,1 V för urladdning (ca 70 A) och 1,65 → 2,5 V för laddning (ca 38 A). Riktningen kan vändas i inställningarna om sensorn sitter åt andra hållet.

**Teckenkonvention:** positiv ström = laddning (in i batteriet), negativ = urladdning. Det är samma konvention som Victron och de flesta BMS:er använder.

### 4.2 Firmware

- Var 100:e ms: 64 × `analogReadMilliVolts(GPIO3)`, medelvärde.
- Ström: `I = (mV − nollpunkt) / känslighet × faktor`, med motsatt tecken om riktningen är vänd. Sjunkande spänning ger alltså negativ ström (urladdning).
- Värden utanför 0,05–3,1 V markeras som "mättad". Strömmen sätts då till gränsvärdet, ±70 A × faktor.
- **"Nolla ström"** (knapp i UI) sparar aktuell medelspänning som nollpunkt. Den trycks när ingen ström går.
- **Räknare:** Ah in, Ah ut, kWh in och kWh ut (effekt = packspänning × ström). De sparas i NVS var 10:e minut.

## 5. Laddnivå (SoC)

### 5.1 Gränser

0 % = lägsta cell 3,30 V och 100 % = 4,10 V (inställbara: `socEmptymV`, `socFullmV`). Laddnivån baseras på **lägsta cellen**, eftersom den sätter gränsen för urladdning i en seriesträng.

### 5.2 Vilospänningskurva (OCV)

Tabell för cellens vilospänning mot laddnivå, med linjär interpolation mellan punkterna. Värdena är typiska för NMC-celler och kalibreras senare mot Ah-räkningen.

| mV | 3300 | 3450 | 3550 | 3620 | 3670 | 3720 | 3780 | 3850 | 3920 | 3980 | 4040 | 4100 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| % | 0 | 5 | 10 | 20 | 30 | 40 | 50 | 60 | 70 | 80 | 90 | 100 |

Med andra gränser än 3300/4100: `soc = (tab(v) − tab(empty)) / (tab(full) − tab(empty)) × 100`, begränsat till 0–100.

### 5.3 Beräkning

- **Utan strömsensor** (`curEnabled = false`): SoC = OCV(lägsta cell), utjämnat med en tidskonstant på 60 s. Märks "uppskattad".
- **Med strömsensor:** Ah-räkning, `soc += I × dt / 3600 / kapacitet × 100`, begränsat till 0–100. Två korrigeringar:
  1. **Full:** laddström > 2 A har förekommit sedan förra full-korrigeringen, och därefter är högsta cell ≥ socFull − 20 mV och 0 ≤ I < 1 A i 60 s. Då sätts SoC till 100 %. Det motsvarar att laddaren har slutat.
  2. **Vila:** |I| < 0,5 A i 30 min. Då sätts SoC till OCV(lägsta cell). Det tar bort nollpunktsdrift och delningsfel.
- **Vid start** används senast sparade SoC från NVS om den finns. Annars, och alltid utan strömsensor, används OCV.
- **Sparas** i NVS var 5:e minut om värdet ändrats ≥ 0,5 %.
- **Energi kvar:** `kWh kvar = soc/100 × kapacitet [Ah] × antal celler × 3,7 V / 1000`, en uppskattning med nominell cellspänning.
- **Tid till tomt/fullt** visas bara med strömsensor och |I| ≥ 1 A.

## 6. Historik

### 6.1 Post (16 byte, little-endian)

| Fält | Typ | Enhet |
|---|---|---|
| t | uint32 | epoch-sekunder. Bit 31 satt = upptid i sekunder från denna start (tiden okänd ännu). |
| packdV | uint16 | 0,1 V |
| minCellmV | uint16 | mV |
| maxCellmV | uint16 | mV |
| minT | int8 | °C (INT8_MIN = okänd) |
| maxT | int8 | °C |
| currentdA | int16 | 0,1 A (INT16_MIN = ingen sensor) |
| socX10 | uint16 | 0,1 % |

### 6.2 Buffertar

- **24 h:** ringbuffert med 1 440 poster (1 per minut, medelvärde av minuten) i RAM, totalt 23 kB. Den skrivs till LittleFS-filen `/hist.bin` var 10:e minut, alltså 144 skrivningar/dygn, och läses in vid start. Partitionen "spiffs" på 128 kB finns redan i Minimal SPIFFS-schemat (ca 26 raderingar per block och dygn, alltså ca 10 års livslängd).
- **10 min live:** ringbuffert med 600 poster (1 per sekund), bara i RAM.

### 6.3 Klocka

- NTP (`pool.ntp.org`) när ESP:n är ansluten till hemnätet.
- `POST /api/time {"epoch": n}` skickas av webbsidan varje gång den öppnas, så att telefonens tid används när bilen är utanför hemnätet.
- När tiden blir känd skrivs poster med bit 31 satt om till epoch. Poster från en tidigare start som fortfarande har relativ tid kastas vid inläsning.

## 7. Larm

Två nya bitar läggs till. Bitnumren 0–6 är oförändrade.

| Id | Larm | Varning | Larm |
|---|---|---|---|
| 7 | Urladdningsström hög (−I) | ≥ `curWarnA` (standard 60 A) | ≥ `curAlarmA` (standard 70 A, sensorns gräns) |
| 8 | Strömsensor mättad | medan sensorn är mättad | – |

Larmet för hög ström släpper när strömmen gått tillbaka 2 A.

## 8. Inställningar

Nya fält läggs **efter** de befintliga (offset 252), och `SETTINGS_VERSION` blir 2. En sparad v1-blob på 252 byte läses in i en struct som redan fyllts med standardvärden. Därmed behålls bland annat `tempOffset` 65.

| Fält | Typ | Standard | Giltigt |
|---|---|---|---|
| curSensX100 (mV/A × 100) | uint16 | 2200 | 500–10000 |
| curZeromV | uint16 | 1650 | 500–2800 |
| curFactorX10 | uint16 | 10 | 1–100 |
| curWarnA | uint16 | 60 | 1–500, < curAlarmA |
| curAlarmA | uint16 | 70 | ≤ 500 |
| capacityAhX10 | uint16 | 400 | 10–2000 |
| socEmptymV | uint16 | 3300 | 2800–3700, < socFullmV |
| socFullmV | uint16 | 4100 | 3800–4250 |
| curEnabled | bool | false | – |
| curInvert | bool | false | – |

## 9. API och UI

### 9.1 API

- **`/api/state` utökas:**
  - `soc: {pct, mode: "ocv" | "ah", kWhLeft, capAh}`
  - `cur: {enabled, A, kW, mV, saturated, ahIn, ahOut, kWhIn, kWhOut, minutesLeft | null}`
  - `time: {known, epoch}`
- **`GET /api/history?range=10m|24h`** ger binärt svar: 4 byte antal poster, följt av posterna (avsnitt 6.1), äldst först.
- **`POST /api/time`** `{"epoch": n}`
- **`POST /api/current/zero`** sätter nollpunkten och svarar med det nya värdet.

### 9.2 UI

- **Översikt:** en stor laddnivåruta (batterisymbol, %, kWh kvar, "uppskattad" eller "Ah-räkning") och en strömruta (A, kW, tid till tomt/fullt).
- **Ny flik "Grafer":**
  - Fyra canvas-grafer: packspänning; lägsta/högsta cell och delta; ström och effekt; laddnivå och temperatur.
  - Intervallknappar för 10 min, 1 h, 6 h och 24 h.
  - Ett hårkors visar värdena under muspekaren eller fingret.
  - Grafritaren är egenskriven, utan externt bibliotek.
- **Inställningar:** ny grupp "Ström och laddnivå" med knappen "Nolla ström".
- **Diagnostik:** rå ADC-mV, nollpunkt, mättnad och tid (känd/okänd).

## 10. ESP-NOW version 2

`BMS_PACKET_VERSION` blir 2. Tre fält läggs till sist: `uint16 socX10`, `int16 currentdA` (INT16_MIN = ingen sensor) och `uint8 socMode` (0 = OCV, 1 = Ah). Det ger 237 byte.

## 11. Moduler

| Fil | Ansvar | Ren |
|---|---|---|
| `soc.h/.cpp` | OCV-tabell, SoC-tillstånd, Ah-räkning, korrigeringar | ✓ |
| `current.h/.cpp` | `currentFromMv()`, mättnad | ✓ |
| `history.h/.cpp` | `HistRec`, ringbuffert, minutmedel, omskrivning av relativ tid | ✓ |
| `current_sensor.h/.cpp` | ADC-läsning (GPIO3) | |
| `history_store.h/.cpp` | LittleFS: läsa och skriva `/hist.bin` | |
| `clock.h/.cpp` | NTP, webbläsartid, epoch | |
| Ändras | `settings*`, `alarms*`, `bms_packet.h`, `packet_builder*`, `web*`, `web_ui.h`, `BMS_v2.ino`, `tools/mock_server.py` | |

## 12. Test

- **PC-tester:**
  - OCV-interpolation och gränser
  - SoC i båda lägena, inklusive full- och vilokorrigering
  - strömomräkning, invertering, faktor och mättnad
  - historikens ringbuffert, minutmedel och omskrivning av relativ tid
  - migrering av v1-inställningar (offset 252)
  - larm 7–8
  - paketstorlek 237
- **Kompilering** av vanlig och simulerad firmware. Simuleringen får en simulerad ström (laddning/urladdning i cykler) så att graferna kan testas.
- **Mock-servern** får historik och strömdata. Graferna kontrolleras i webbläsaren, även i mobilbredd.
- **På hårdvaran:**
  1. OTA, och kontrollera att `tempOffset` 65 finns kvar.
  2. SoC i OCV-läge innan sensorn är monterad.
  3. Efter montering: "Nolla ström", jämför med laddarens ström, kalibrera faktorn.
