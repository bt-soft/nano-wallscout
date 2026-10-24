# Pulzusindukciós (PI) betonvas-kereső – Arduino Nano + DD tekercs + MCP6004 + IRLZ44N + SSD1306

Célmélység: 5–6 cm biztosan, 8 cm határeset (Ø8–12 mm betonvas, a tekercsek méretétől függően). Platform: **Arduino Nano (ATmega328P)**.

## Működés

1. Az IRLZ44N 60 µs-ra bekapcsol, a **Tx tekercs** (L1) árama ≈ 0,6 A-re nő (1 mH, 12 V mellett kb. 11 A/ms).
2. Kikapcsolásnál a flyback-feszültséget a zener-sor ~36–39 V-ra lecsípi, a tekercsáram kb. 30 µs alatt nullára esik.
3. A tranziens az `Rd` csillapítóval gyorsan lecseng. A fémben indukált örvényáram lassabban cseng le, ez a mérendő "farok".
4. A **külön Rx tekercs** (L2) az L1-gyel DD-ben (részben egymásra csúsztatva) van elhelyezve, így a közvetlen áthallás ≈ 0, és csak a fém által visszavert jel jut az erősítőbe.
5. A Nano 1 kHz-en ismétel, és impulzusonként két ponton mintavételez az ADC-vel: **S1** a kikapcsolás után 60 µs-mal, **S2** (alapvonal) 600 µs-mal.
6. Jel = S1 − S2, 100 impulzusra átlagolva (kb. 7 Hz kijelzőfrissítés). Ez kiejti az offsetet és a lassú driftet.
7. Indításkor automatikus nullázás (a kereső legyen levegőben). Az alapvonal lassan követi a driftet.
8. A kijelző oszlopot, számértéket és (kalibrálás után) becsült mélységet mutat. A csipogó hangmagassága a jellel nő.

Gombok:
- **D4 (NULLÁZÁS)** röviden = újranullázás. Hosszan (1 s) = **DD átfedés-beállító mód** ki/be.
- **D5 (KALIBRÁLÁS)** hosszan (1 s) nyomva, miközben egy vasat pontosan 5 cm-re tartasz = kalibrálás (az `S ≈ K/d³` modellre, EEPROM-ba mentve).

Mélység: d = ∛(K/S). Ez a mélység becslése, nem pontos mérés.

### Miért elég a Nano?

A 10 bites, 5 V-os ADC lépése ≈ 4,9 mV. A ×100 erősítés miatt ez az Rx-nél ≈ 49 µV/LSB. A 100 impulzus átlagolása az ADC saját zajával együtt 0,1 LSB alá viszi a felbontást, ami a Pico valós (kb. 9 bites, 3,3 V-os) felbontásával egyenértékű. A mintavételi időzítést a Timer1 (0,5 µs) adja, az impulzus közben a megszakítások tiltva vannak.

## DD tekercsek

| Paraméter | Érték |
|---|---|
| Tekercsek | 2 db D-tekercs, egyenként 1 mH, 6 cm egyenes oldal, 5 cm D-magasság |
| Szerep | L1 = Tx (adó), L2 = Rx (vevő) |
| Elrendezés | az egyenes oldalaikkal részben egymásra csúsztatva (DD) |
| Csúcsáram (12 V / 60 µs) | ≈ 0,6 A (9 V-nál ≈ 0,45 A), a tekercs R-jétől függően |

**Mérd meg az L és R értéket** (a tekercs egyenáramú ellenállása sokat számít, ha 6 Ω fölött van, a csúcsáram kisebb lesz). Az átfedést a DD beállító móddal keresd meg, majd ragaszd/fixáld. A két tekercs síkja azonos legyen. Rövid, csavart, árnyékolt vezeték a vezérlőig.

### DD átfedés beállítása (firmware-es segítség)

1. Tartsd a keresőt levegőben, fémtől legalább fél méterre.
2. Tartsd lenyomva a **NULLÁZÁS** gombot 1 másodpercig: a kijelzőn `DD ATFEDES` jelenik meg.
3. A beállító mód automatikusan két csatornát használ. A **durva** (`DD durva x1`) a nyers Rx jelet méri erősítés nélkül (A3, R15-ön át), ez nem telít. Amikor a jel elég kicsi lett, **finomra** (`DD finom x10`) vált át az U1A kimenetére (A2). Ha ott telítés lép fel, visszavált durvára. A kijelzett érték mindig ×100-as egyenértékűre van skálázva, ezért a küszöbök ugyanazok. A kijelzőn a közvetlen áthallás látszik **előjelesen** (S1 − S2), alatta külön sorban a `min` (az eddigi legkisebb abszolút érték) és a nyers `S1` (nyugalmi értéke ≈ 512), legalul középre nullázott logaritmikus oszlop. A csipogó annál magasabb, minél nagyobb az áthallás, és elhallgat, ha az érték 1,5 alá esik (`NULL OK`).
4. Csúsztasd az egyik tekercset a másikhoz képest. Az érték **előjelet vált**, ezen a ponton kell a minimumot keresni. A kezdeti átfedés a D egyenes oldalának néhány mm–1,5 cm-e.
5. Ha megvan, rögzítsd a tekercseket, és nyomd hosszan újra a gombot: a kereső kilép a módból, és újranulláz. Egy rövid nyomás a beállító módban a `min` értéket törli.

Ha az erősítő telítésbe megy (az S1 a 0 vagy 1023 közelében), a fejléc `TULVEZERLES`-re vált, a szám az S1 eltérése 512-től (az előjel megmutatja, merre telít), a csipogó mély, folyamatos hangot ad, és a `min` **nem frissül**. A telítés tehát nem számít jó minimumnak. Addig csúsztasd a tekercset, amíg a `TULVEZERLES` el nem tűnik, és az `S1` 512 körül van.

## Kapcsolási rajz

Három lapon (SVG, böngészőben vagy VS Code-ban megnyitható):

1. [Táp és tekercs-meghajtó](rajz/1_tap_es_meghajto.svg) – akku, 7805, Q1, Tx tekercs, Rd, zener-sor lecsípés
2. [Analóg jelút](rajz/2_analog_jelut.svg) – MCP6004: Rx tekercs bemenet és védelem, 2 × ×10 erősítő, Vmid, ADC-puffer
3. [Nano bekötése](rajz/3_nano_bekotes.svg) – OLED, gombok, csipogó, akkumérés, ADC

![1. lap](rajz/1_tap_es_meghajto.svg)
![2. lap](rajz/2_analog_jelut.svg)
![3. lap](rajz/3_nano_bekotes.svg)

Az SVG-k a `rajz/gen.py` szkripttel generálódnak (`python rajz/gen.py`).

### Védelem

- **Drain-oldal:** a flyback-et UF4007 (D2) + 3 db sorosan kötött zener (DZ1–DZ3, együtt 36–39 V) csípi le. Kisebb impulzusenergiánál (≈ 0,2 mJ) ez elég, de a zenerek feszültsége nagy áramnál megnő. Ezért a `T_ON` növelése előtt mindig ellenőrizd szkóppal, hogy a drain csúcsa **50 V alatt** marad (Q1 Vds,max = 55 V).
- **Rx-oldal:** külön védődióda nincs. A 10 kΩ soros ellenállás (R5) az MCP6004 belső ESD-diódáival együtt a megmaradó tüskét ≤ 2 mA-re korlátozza. A DD elrendezésnél az Rx jele eleve kicsi.

## Alkatrészlista

| Db | Alkatrész |
|---|---|
| 1 | Arduino Nano (ATmega328P) |
| 1 | MCP6004 (DIP14 vagy SOIC14) |
| 1 | IRLZ44N |
| 1 | SSD1306 128×64 I2C OLED |
| 1 | L7805 (TO-220) |
| 2 | D-tekercs, 1 mH (Tx és Rx) |
| 3 | zener 12–13 V, ≥ 1 W (DZ1–DZ3, sorosan) |
| 1 | UF4007 (D2), 1 db 1N5819 (D1), 1 db 1N4148 (D5) |
| 1 | 1000 µF/25 V low-ESR, 10 µF kerámia |
| 1 | BC547 (csipogóhoz), passzív csipogó |
| – | Ellenállások: 10 Ω, 47 Ω, 330 Ω ×2 (R8, R14), 1 kΩ ×2 (R7, R11) + Rd 1 kΩ, 10 kΩ ×7 (R2, R5, R9, R10, R15, Rg1, Rg2), 33 kΩ, 91 kΩ ×2, 100 kΩ |
| 2 | trimmer: 1 kΩ (Rd), 2,2 kΩ (R3, Rx csillapítás) |
| – | Kondenzátorok: 330 nF, 100 nF ×4 (C4, C7, C10 + MCP6004 lábnál), 1 nF, 1 µF, 10 µF ×2 |
| 2 | nyomógomb, főkapcsoló |

## Beüzemelés (ebben a sorrendben)

1. Először **tekercs és Rx nélkül** (a drain-t nézd szkóppal, kis `T_ON`-nal) ellenőrizd a táp- és meghajtó-oldalt: a lecsípés ≤ 50 V legyen.
2. Kösd be a Tx tekercset, és állítsd az `Rd` trimmert kritikus csillapításra: a leggyorsabb lecsengés túllövés nélkül (1 mH-nál kb. 1–2 kΩ).
3. Kösd be a jelutat az Rx tekerccsel. Az A0 nyugalmi értéke kb. 2,5 V legyen (`s1` ≈ 512 a soros monitoron). Az `R3` trimmerrel állítsd az Rx csillapítását a leggyorsabb lecsengésre.
4. A DD beállító móddal állítsd az átfedést minimális áthallásra (lásd fent), és rögzítsd.
5. Nézd meg az U1A kimenetén (1. láb), mikor ér vissza a jel az alapszintre. Ha 60 µs-nál később, a `T_SAMPLE1`-et növeld ([main.cpp](src/main.cpp) tetején).
6. Tarts egy vasat a tekercshez: a jel nőjön. Az `abs()` miatt a polaritás nem számít, de a soros monitoron (`sig`) látod.
7. Ha telít (`TULVEZERLES`), javítsd az átfedést, vagy a második fok Rf-jét cseréld 47 kΩ-ra.
8. Kalibrálás 5 cm-nél, majd ellenőrzés 6 és 8 cm-nél.

## Ismert kockázatok

- Az MCP6004 (1 MHz GBW) túlvezérlésből felépülése lassú lehet. A DD elrendezés és az R5 soros ellenállás ezt enyhíti, de a mintavételi késleltetést a mérésed alapján kell beállítani.
- A kis tekercsek (≈ 6 × 10 cm) miatt a 8 cm határeset, főleg vékonyabb vasnál és betonban.
- A szám-értékek (`DETECT_TH`, `FULL_SCALE`, `T_SAMPLE1`) számolt/javasolt értékek, a prototípuson hangolni kell.
- Az akku/tekercsáram tápingadozása az alapvonalba megy, akkucsere után nullázz újra.
- A 7805 mellett a 3S akku legalább ~7,5 V-ig működik; 9 V alatt az akkut töltsd.
- A watchdog (1 s) lefagyás esetén újraindítja a Nanót, a 10 kΩ lehúzó (R2) a gate-et addig is alacsonyan tartja. A régi (57600 baudos) bootloaderes Nanók watchdog-reset után megállhatnak, ilyenkor elég kikapcsolni és újra bekapcsolni, vagy a `USE_WATCHDOG`-ot 0-ra kell állítani.

## Fordítás

```
pio run                      # régi bootloaderes Nano (nano környezet)
pio run -e nano_newboot      # új (115200 baudos) bootloader
pio run -t upload            # feltöltés
```
Az USB-t csak programozáskor dugd be, a D1 dióda megakadályozza, hogy az USB-ről a 7805 áramot kapjon vissza.
