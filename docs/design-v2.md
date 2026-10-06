# BMS v2 – firmware för ESP32-C3 med 8 Outlander PHEV-CMU:er

**Datum:** 2026-10-04
**Status:** Godkänd design, väntar på granskning av specen
**Ersätter:** `BMS_python_anpassning_1` (det som sitter på ESP:n idag) och `BMS1`. Båda behålls orörda.

## 1. Bakgrund

ESP32-C3:an läser 8 Mitsubishi Outlander PHEV-CMU:er via en MCP2515 och visar data på en webbsida. Problem med nuvarande kod (`BMS_python_anpassning_1.ino`):

1. `setup()` väntar på hemnätet utan timeout. Utanför hemmet når koden aldrig `loop()`, så ingen CAN-läsning och ingen balansering sker.
2. Temperaturerna i 0x6X1 tolkas som 16-bitarsvärden / 1000. En riktig CAN-dump visar att det är en byte per givare.
3. Bara en CAN-ram läses per varv i `loop()`, och den synkrona `WebServer` blockerar. Med 0x3C3 var 40:e ms kommer ca 800 ramar/s, och MCP2515:ans två mottagningsbuffertar svämmar över.
4. Koden förutsätter 8 celler per CMU. Det finns CMU:er med 4 celler, som aldrig skickar 0x6X3.
5. Balanseringen använder en fast, manuellt inställd spänning, och "Force Balance" (läge 2) slår på alla bleed-resistorer utan spänningskontroll och utan tidsgräns.

## 2. Mål

- BMS:en fungerar fullt ut oavsett om hemnätet finns: ett eget WiFi är alltid på, och hemnätet används när det finns.
- Robust CAN-läsning och korrekt avkodning av cellspänningar, temperaturer och balanseringsstatus för 4- och 8-cellsmoduler.
- Automatisk toppbalansering med skyddsvillkor.
- Larm visas i webbgränssnittet (och senare på skärmen).
- Nytt webbgränssnitt: översikt, inställningar och diagnostik.
- Inställningar sparas i flash och ändras via webben.
- Firmware kan uppdateras via webben (OTA).
- Förberett för en separat skärmenhet (Waveshare ESP32-S3-Touch-AMOLED-1.64) via ESP-NOW.
- Diagnostikdata kan exporteras som filer, så att CMU-beteendet kan analyseras i efterhand.

## 3. Ingår inte

- Skärmenhetens firmware. Den blir ett eget projekt som använder paketformatet i avsnitt 9.
- Relä- eller MOSFET-utgångar för att bryta laddning eller last. Larm visas bara.
- SoC-beräkning och coulombräkning (strömgivare saknas).
- Långtidsloggning till flash eller SD-kort, samt molntjänster.

## 4. Hårdvara

| Del | Detalj |
|---|---|
| MCU | ESP32-C3, 4 MB flash. FQBN `esp32:esp32:esp32c3` med `CDCOnBoot=cdc`. Arduino-core 3.3.4. |
| CAN-styrkrets | MCP2515 med 8 MHz kristall, 500 kbit/s |
| SPI | SCK GPIO4, MISO GPIO5, MOSI GPIO6 (C3:ans standard) |
| MCP2515 CS / INT | GPIO7 / GPIO0 |
| BOOT-knapp | GPIO9, används för att återställa nätverksinställningarna |
| Batteri | 8 CMU:er, alla moduler i serie (troligen 64S, 192–269 V) |

**Bibliotek** (redan installerade):
- Inget MCP2515-bibliotek: egen drivrutin (`mcp2515.*`) med rollover, RX-avbrott och sändning utan väntan. AA_MCP2515:s `write()` väntar minst 1 ms efter varje sändning och dess avbrottsrutin kör SPI i ISR.
- ESPAsyncWebServer 3.9.4
- AsyncTCP 3.4.10
- ArduinoJson 7.4.2
- Preferences (ingår i core)

**Partitionsschema:** "Minimal SPIFFS (1.9MB APP with OTA)" från start (app-partition 1 966 080 byte). Första USB-programmeringen skriver om partitionstabellen. Firmwaren är ca 1,14 MB.

## 5. CMU-protokollet (så långt det är känt)

Källor:
- Tom de Bree, [OutlanderPHEVBMS](https://github.com/Tom-evnut/OutlanderPHEVBMS)
- CAN-dump från två CMU:er i [knoxsp/OutlanderCANManInTheMiddle](https://github.com/knoxsp/OutlanderCANManInTheMiddle) (`data/outlander_cmu_can_dump_2_modules`)

X är CMU-id 1–15, och ram-id är `0x600 | (X << 4) | sub`.

| Ram | Riktning | Innehåll |
|---|---|---|
| 0x6X1 | CMU → ESP | b0: balanseringsbitar (bit i = cell i+1 bleeds). b1: status, okänd betydelse (0x30 i dumpen), visas rått. b2–b7: temperaturgivare, en byte var, 0xFF = ingen givare. |
| 0x6X2 | CMU → ESP | Cell 1–4: uint16 big-endian, mV |
| 0x6X3 | CMU → ESP | Cell 5–8: uint16 big-endian, mV. Skickas inte av 4-cells-CMU:er. |
| 0x6X4 | CMU → ESP | Bara 0xFF i dumpen. Sparas och visas rått, tolkas inte. |
| 0x3C3 | ESP → alla CMU | b0–b1: målspänning i mV (big-endian). b2: läge (0 = all bleed av, 1 = bleed för celler över målet, 2 = all bleed på, används **inte**). b3 = 0x04, b4 = 0x03, b5–b7 = 0. |

Exempel ur dumpen: `0x6B1: 00 30 3C 3C 3B 3B 3B FF` och `0x612: 0F 7F 0F 7B 0F 7C 0F 7C` (3967, 3963, 3964, 3964 mV).

**Temperatur:** T [°C] = rå − förskjutning. Förskjutningen är 40 som standard och kan ställas in. Den är **obekräftad** (40 och 50 är båda rimliga) och kalibreras mot IR-mätning (avsnitt 12).

**Svarsfrekvens:** I dumpen kommer ramarna var 40:e ms. Hypotesen är att CMU:erna svarar på varje 0x3C3. Därför pollar vi var 200:e ms som standard (inställbart 40–1000 ms), vilket ger ca 160 ramar/s med 8 CMU:er. Diagnostiken visar ramar per sekund per CMU så att hypotesen kan bekräftas.

**Antal celler per CMU:** 8 om någon giltig 0x6X3 har tagits emot, annars 4.

**Giltig cellspänning:** 2000–4500 mV. Värden utanför det intervallet, inklusive 0xFFFF, ignoreras: det senaste giltiga värdet behålls och en räknare för ogiltiga värden ökar.

## 6. Arkitektur

Det blir en ny sketch, `BMS_v2/`, för Arduino IDE. Logikmodulerna (märkta *ren*) innehåller ingen Arduino-kod, så de kan kompileras och testas på PC:n.

| Fil | Ansvar | Ren |
|---|---|---|
| `BMS_v2.ino` | `setup()`/`loop()`, kopplar ihop modulerna, BOOT-knappen | |
| `config.h` | Pinnar, firmwareversion, standardvärden, `SIMULATE_CMUS` | ✓ |
| `settings.h/.cpp` | `Settings`-struct, läsa och spara i NVS, validering, JSON in/ut | delvis |
| `cmu.h/.cpp` | Datamodell (`CmuData`, `PackState`), `decodeFrame()`, `computeStats()` | ✓ |
| `balancer.h/.cpp` | `BalanceDecision decide(const PackStats&, const Settings&, now)` | ✓ |
| `alarms.h/.cpp` | Larmutvärdering med hysteres, ringbuffert för logg | ✓ |
| `can_bus.h/.cpp` | MCP2515-init, mottagningsuppgift, sändning av 0x3C3, felräknare, CAN-inspelning | |
| `sim.h/.cpp` | Simulerade CMU-ramar när `SIMULATE_CMUS` är definierat | |
| `net.h/.cpp` | Eget nät + hemnät, återanslutningslogik, mDNS `bms.local` | |
| `web.h/.cpp` | ESPAsyncWebServer: routes, JSON, OTA | |
| `web_ui.h` | Hela webbsidan som en rå strängliteral (HTML, CSS och JS inline, inga CDN-länkar) | |
| `bms_packet.h` | ESP-NOW-paketformatet, delas med skärmprojektet | ✓ |
| `espnow_tx.h/.cpp` | Broadcast av `BmsPacket` | |

### 6.1 Samtidighet

- **`canTask`** (FreeRTOS, prioritet 10, över loop och AsyncTCP, under WiFi):
  - En ISR på INT-pinnen (fallande flank) ger en semafor. Tasken vaknar och läser tills MCP2515 svarar NODATA.
  - Varje ram avkodas till den delade `PackState`, skyddad av en mutex som bara hålls under själva skrivningen.
  - Tasken skickar också 0x3C3 enligt intervallet, med målspänning och läge från det senaste balanseringsbeslutet.
  - Som reserv, om ett avbrott missas, väntar semaforen högst 5 ms och läser sedan ändå.
  - ISR:en gör ingen SPI-kommunikation. Den egna drivrutinen gör ingen SPI i ISR.
- **`loop()`:**
  - Var 100:e ms: tar en kopia av `PackState`, räknar statistik, kör balanseringsbeslutet och larmutvärderingen, och publicerar resultatet som "senaste snapshot" under mutexen.
  - Var 500:e ms: ESP-NOW-paket.
  - Dessutom nätverksunderhåll och kontroll av BOOT-knappen.
- **Webbhanterarna** (AsyncTCP-tasken) läser bara den senaste snapshoten, kopierad under mutexen. De rör aldrig SPI eller CAN.

### 6.2 Datamodell (i korthet)

```cpp
struct CmuData {
  uint8_t  id;              // 1–15
  bool     seen;
  uint32_t lastRxMs;
  uint8_t  cellCount;       // 4 eller 8
  uint16_t cellmV[8];
  uint8_t  tempRaw[6];      // 0xFF = ingen givare
  uint8_t  balBits;
  uint8_t  status;          // byte 1 i 0x6X1, rå
  uint8_t  raw[4][8];       // senaste rådata för sub 1–4
  uint32_t lastSubRxMs[4];
  uint32_t frames[4];       // räknare per sub, används för ramar/s
  uint32_t invalidCells;
};
```

`CmuData` skrivs bara av `canTask`. Bleed-tiden (`bleedSeconds[16][8]`) ägs av `loop()` och ligger därför utanför `CmuData`.

- `PackState` har 16 platser indexerade på CMU-id.
- "Modul 1–8" i UI:t är de sedda CMU-id:na i stigande ordning.
- `PackStats` innehåller:
  - packspänning (summa av alla giltiga celler)
  - lägsta och högsta cell med plats (modul och cell)
  - delta
  - lägsta och högsta giltiga temperatur
  - antal CMU:er online (`now − lastRxMs < cmuTimeoutMs`) och antal celler

## 7. Balansering

Lägen: **Av**, **Auto**, **Manuell**.

| Inställning | Standard |
|---|---|
| `balStartmV` | 3900 |
| `balStartDeltamV` | 20 |
| `balStopDeltamV` | 8 |
| `balMaxTempC` | 45 |
| `balTargetOffsetmV` | 5 |
| `manualTargetmV` | 3950 |
| `manualTimeoutMin` | 60 |

**Auto** är aktiv när *alla* villkor är uppfyllda:
1. Antal CMU:er online = `expectedCmus` (standard 8).
2. Högsta cell ≥ `balStartmV`.
3. Delta ≥ `balStartDeltamV` för att starta. När balanseringen väl är igång fortsätter den tills delta ≤ `balStopDeltamV` (hysteres).
4. Högsta giltiga temperatur ≤ `balMaxTempC`.

När Auto är aktiv blir målspänningen max(lägsta cell + `balTargetOffsetmV`, `balStartmV`), och 0x3C3 skickas med läge 1. Annars skickas läge 0 med målspänningen 3901 mV, samma värden som den nuvarande koden använder när balanseringen är av.

**Manuell** skickar `manualTargetmV` med läge 1. Villkor 1 och 4 gäller fortfarande. Läget går automatiskt tillbaka till Av efter `manualTimeoutMin`.

**Av** skickar läge 0. 0x3C3 skickas alltid, så att CMU:erna hålls vakna och svarar.

**Utdata från `decide()`:**
- tillstånd: `OFF`, `WAITING`, `ACTIVE`, `MANUAL` eller `BLOCKED`
- målspänning
- läge
- en orsakstext, t.ex. "Väntar: högsta cell 3862 mV < 3900 mV" eller "Blockerad: CMU 0xB offline"

**Bleed-tid:** `bleedSeconds` ökar per cell utifrån `balBits` (räknas i loop, med 1 s upplösning). Räknarna kan nollställas från UI:t.

## 8. Larm

Larmen visas bara i UI:t (och senare på skärmen). Inget kopplas bort.

| Larm | Varning | Larm | Släpper när värdet gått tillbaka |
|---|---|---|---|
| Cell hög | ≥ 4150 mV | ≥ 4200 mV | 20 mV |
| Cell låg | ≤ 3300 mV | ≤ 3000 mV | 20 mV |
| Delta | ≥ 50 mV | ≥ 100 mV | 10 mV |
| Temperatur hög | ≥ 45 °C | ≥ 55 °C | 2 °C |
| Temperatur låg | ≤ 0 °C | – | 2 °C |
| CMU offline (> `cmuTimeoutMs`, standard 2000) | – | larm | när CMU:n svarar igen |
| CAN-fel (RX-overflow har ökat senaste 10 s, error passive eller bus-off) | varning | – | efter 10 s utan nya fel |

- Alla gränsvärden kan ändras i inställningarna.
- Larmloggen är en ringbuffert med 20 händelser i RAM: upptid, typ, nivå, värde, plats, aktiv/släppt.
- Larmstatus exponeras som bitmasker (`alarmBits` och `warnBits`), med samma bitnummer i JSON och i ESP-NOW-paketet.

## 9. ESP-NOW-paket (förberedelse för skärm)

`bms_packet.h` (packad struct, little-endian, högst 250 byte):

```cpp
#pragma pack(push, 1)
struct BmsCmuPacket {        // 25 byte
  uint8_t  id;
  uint8_t  flags;            // bit0 online, bit1 åtta celler
  uint8_t  balBits;
  uint16_t cellmV[8];
  int8_t   tempC[6];         // INT8_MIN = ogiltig (rå 0xFF)
};
struct BmsPacket {           // 32 + 8*25 = 232 byte
  uint8_t  magic[2];         // 'B','M'
  uint8_t  version;          // 1
  uint8_t  seq;
  uint32_t uptimeS;
  uint32_t packmV;
  uint16_t minCellmV, maxCellmV;
  uint8_t  minCellLoc, maxCellLoc;   // (modulindex << 4) | cellindex
  int8_t   minTempC, maxTempC;
  uint8_t  balState;
  uint8_t  cmuCount;
  uint16_t balTargetmV;
  uint32_t alarmBits;
  uint32_t warnBits;
  BmsCmuPacket cmu[8];
};
#pragma pack(pop)
```

- Paketet skickas som broadcast till `FF:FF:FF:FF:FF:FF` var 500:e ms på den aktuella WiFi-kanalen.
- När ESP:n är ansluten till hemnätet följer kanalen hemnätets kanal, så skärmenheten måste söka igenom kanalerna. Det hanteras i skärmprojektet.
- Sändningen kan stängas av i inställningarna (`espnowEnabled`).
- Ett `static_assert` kontrollerar storleken.

## 10. Nätverk

- **Eget nät:**
  - Alltid på: SSID `BMS-Transporter`, WPA2, IP 192.168.4.1.
  - Lösenordet ställs in (minst 8 tecken). Standardvärdet ligger i `config.h`.
- **Hemnätet:**
  - Hemnätet (namn och lösenord i secrets.h), fast IP 192.168.1.89 med gateway 192.168.1.99 och mask /24. Fast IP kan stängas av (DHCP).
  - Anslutningen startas utan att blockera uppstarten.
  - Om den misslyckas görs ett nytt försök var 60:e sekund, men bara när inga klienter är anslutna till det egna nätet, eftersom sökningen får det egna nätet att byta kanal.
- **mDNS:** `bms.local`.
- **Ändringar:** nätverksinställningar sparas i NVS och börjar gälla efter omstart. UI:t har en omstartsknapp.
- **Återställning:** BOOT-knappen (GPIO9) hålls in i 5 s medan BMS:en är igång. Då återställs nätverksinställningarna till standardvärdena i `config.h` och ESP:n startar om. (Att hålla knappen *vid* uppstart startar C3:ans nedladdningsläge och används därför inte.)

## 11. Webb

### 11.1 API

| Metod och sökväg | Funktion |
|---|---|
| `GET /` | Webbsidan (`web_ui.h`) |
| `GET /api/state` | Live-data (se nedan), ca 3 kB |
| `GET /api/settings` | Inställningar. Lösenord returneras som `"********"`. |
| `POST /api/settings` | JSON med de fält som ska ändras. Valideras, sparas i NVS och svarar med de nya inställningarna. Vid ogiltiga värden svarar den 400 med fältnamnet. |
| `POST /api/bleed/reset` | Nollställer bleed-tiderna |
| `GET /api/diag` | Rå CAN per CMU och sub (hex, ålder, ramar/s), MCP2515-felräknare (TEC, REC, EFLG, antal overflow), upptid, ledigt heap-minne, WiFi-status (RSSI, IP-adresser, antal klienter på det egna nätet), firmwareversion |
| `POST /api/capture?s=10` | Startar CAN-inspelning: högst 10 s eller 2000 ramar, ca 32 kB RAM som behålls tills nästa inspelning |
| `GET /api/capture.csv` | Inspelningen i SavvyCAN-format: `Time Stamp,ID,Extended,Dir,Bus,LEN,D1..D8`, tid i µs. Både mottagna ramar och 0x3C3 som ESP:n skickat finns med. |
| `GET /api/report` | Nedladdning av state, settings, diag och larmlogg som en JSON-fil |
| `POST /api/restart` | Omstart |
| `POST /update` | OTA: uppladdning av `.bin`, sedan omstart |

Exempel på `/api/state`:

```json
{
  "up": 12345,
  "pack": {"mV": 251234, "cells": 64, "minmV": 3901, "minLoc": "M3C5", "maxmV": 3932,
           "maxLoc": "M7C2", "deltamV": 31, "minT": 18, "maxT": 24, "online": 8, "expected": 8},
  "bal": {"mode": "auto", "state": "ACTIVE", "targetmV": 3906, "reason": "Delta 31 mV ≥ 20 mV"},
  "alarms": {"alarm": 0, "warn": 0, "active": []},
  "cmus": [{"id": 1, "online": true, "age": 120, "cells": [3912, 3915, 3911, 3920, 3901, 3918, 3910, 3915],
            "bal": [0, 0, 0, 1, 0, 1, 0, 0], "tC": [19, 19, 20, 19, 19, null], "tRaw": [59, 59, 60, 59, 59, 255],
            "mV": 31302, "bleedS": [0, 0, 0, 340, 0, 120, 0, 0]}]
}
```

### 11.2 Gränssnitt

En sida med tre flikar. Mörkt tema, anpassad för mobil först, ingen extern resurs. Den hämtar `/api/state` 2 gånger per sekund.

- **Översikt:**
  - Larmrad överst: röd vid larm, gul vid varning, dold annars.
  - Rutor för packspänning, lägsta cell (V och plats), högsta cell, delta, temperatur min/max, balansering (tillstånd och orsak) och CMU:er online/förväntade.
  - Ett kort per modul med modulspänning, temperaturer och 8 (eller 4) cellstaplar. Staplarnas skala går från packens lägsta − 10 mV till högsta + 10 mV, med mV-värde på varje stapel. Lägsta cell markeras blå, högsta röd och bleedande celler med en randig markering.
- **Inställningar:**
  - Grupper: Balansering, Larmgränser, Temperatur (förskjutning, med rå och beräknad temperatur visade bredvid), CAN (pollintervall, förväntat antal CMU:er, timeout), WiFi och ESP-NOW.
  - Sparas med "Spara". WiFi-ändringar visar "kräver omstart".
- **Diagnostik:**
  - Tabell med rå CAN per CMU och sub.
  - Felräknare och systeminfo.
  - Knapparna "Spela in CAN 10 s", "Ladda ner CSV", "Ladda ner rapport" och "Nollställ bleed-tid".
  - Uppladdning av firmware.
  - Larmlogg.

## 12. Test och verifiering

1. **Enhetstester på PC** av de rena modulerna (`cmu`, `balancer`, `alarms`, storleken på `bms_packet`).
   - Testfall:
     - avkodning av ramarna ur knoxsp-dumpen, både 4- och 8-cells-CMU
     - ogiltiga värden
     - temperaturförskjutning
     - alla balanseringsvillkor inklusive hysteres och timeout i Manuell
     - larmtrösklar och hysteres
   - Kräver en C++-kompilator på PC:n. Det kontrolleras i planen och användaren tillfrågas innan något installeras.
2. **Kompilering** av firmwaren för `esp32c3` med arduino-cli (som följer med Arduino IDE), med och utan `SIMULATE_CMUS`. Storleken kontrolleras mot OTA-partitionen.
3. **Förhandsvisning av UI:t** på PC: ett Python-skript (`tools/mock_server.py`) plockar ut sidan ur `web_ui.h` och serverar den med simulerade `/api/*`-svar, så att gränssnittet kan granskas i webbläsaren utan hårdvara.
4. **Hårdvarukontroll**, som användaren gör och skickar resultatet av:
   1. Programmera via USB i Arduino IDE.
   2. Anslut till `BMS-Transporter` och öppna 192.168.4.1. Fungerar det helt utan hemnätet?
   3. Diagnostik: syns alla 8 CMU-id? Hur många ramar/s per CMU vid 200 ms poll, och ändras det med pollintervallet? Hur många overflow?
   4. Spela in CAN i 10 s, ladda ner CSV och rapport, och skicka dem för analys.
   5. Temperaturkalibrering: jämför rådata med IR-mätning på modulen eller omgivningen och ställ in förskjutningen.
   6. Hemma: nås 192.168.1.89 och `bms.local`, och fungerar det egna nätet samtidigt?
   7. Balansering i Manuell: sätt målet strax under högsta cell, kontrollera att bara celler över målet får bleed-bit, och att alla bitar släpper i läge Av.
   8. OTA: ladda upp en ny `.bin` via webben.

## 13. Risker och öppna frågor

| Risk | Hantering |
|---|---|
| Temperaturförskjutningen (40 eller 50) är obekräftad | Den kan ställas in, rådata visas och den kalibreras i steg 4.5 |
| Okänt om CMU:erna svarar per 0x3C3 eller sänder fritt | Ramar/s visas i diagnostiken och pollintervallet kan ställas in |
| MCP2515 tappar ramar när WiFi belastar C3:ans enda kärna | Overflow-räknaren visas. Om den ökar stadigt byter vi till C3:ans inbyggda TWAI-kontroller med en SN65HVD230-transceiver (hårdvarubyte, eget beslut). |
| Byte 1 i 0x6X1 och ramen 0x6X4 är okända | Visas rått, tolkas inte |
| Kanalbyten när ESP:n är ansluten till hemnätet påverkar ESP-NOW | Skärmenheten söker igenom kanalerna |
| Hemnätets lösenord står i klartext i källkoden | Det ligger i `config.h` som idag. Bör inte delas offentligt. |
