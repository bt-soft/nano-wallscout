# WallScout – betonfal-detektor (induction balance, Arduino Nano)

Kézi detektor, amely betonfalban keresi a **betonvasat, fémcsöveket és vezetékeket (fémet)**, valamint a **feszültség alatt lévő 230 V-os vezetéket**. Kijelzője egy I2C OLED, jelzése egy aktív csipogó.

> **Állapot:** a firmware lefordul (PlatformIO, Flash ≈ 75%, RAM ≈ 64%). A hardvert **még nem építettem meg és nem teszteltem**. Minden alkatrészérték, áram- és frekvenciaadat számolt vagy becsült, ezért az első indításnál a „Üzembe helyezés" fejezet lépéseit kövesd.
>
> **Biztonság:** a készülék nem garantálja, hogy a falban nincs vezeték vagy cső. Fúrás előtt mindig kapcsold le a körzetet a biztosítékról, és használj másik módszert is (pl. tervrajz, kábelkereső).

Fájlok:

| Fájl | Tartalom |
|---|---|
| [src/main.cpp](src/main.cpp) | firmware |
| [platformio.ini](platformio.ini) | build beállítás (U8g2 könyvtár) |
| [hardware.svg](hardware.svg) | főkapcsolási rajz |
| [analog.svg](analog.svg) | részletes analóg rajz (műveleti erősítők, visszacsatolás, VREF) |
| [coil.svg](coil.svg) | a tekercspár rajza, méretekkel |
| [coil_sablon.svg](coil_sablon.svg) | 1:1 nyomtatható, 150 × 125 mm-es D-sablon |
| [coil_sablon_60x50.svg](coil_sablon_60x50.svg) | 1:1 nyomtatható, kisebb, 60 × 50 mm-es D-sablon |

---

## 1. Működési elv

### 1.1 Induction balance (IB)
Két tekercs van: egy **adó (TX)** és egy **vevő (RX)**. A két D alakú tekercs részben átfedi egymást úgy, hogy a TX fluxusa az RX-ben **kioltódik**. Fém nélkül az RX alig kap jelet. Ha fém kerül a tekercsek elé, a mező megbomlik, és az RX jele megnő. A jel tehát a nulláról indul, ezért nagy az érzékenység, és a mélység a tekercs méretével nő (15 cm-es tekercsnél a becslésem szerint több cm-re lát, de ezt nem mértem).

A fém típusát a jel **fázisa** adja. Egy adott fázishelyzet a vasat jelenti (ferromágneses), az ellentétes a nemvas fémet (réz, alumínium). A készülék nem tudja előre, melyik fázis a vas, ezért ezt **be kell tanítani** egy vasdarabbal (lásd 6.4).

### 1.2 Élő vezeték (50 Hz)
Külön csatorna: egy rézlap antenna az 50 Hz-es elektromos teret érzékeli. Ez csak **feszültség alatt álló** vezetéket jelez, a feszültségmentest nem.

---

## 2. Hardver

A teljes kapcsolás a [hardware.svg](hardware.svg) fájlban van, az analóg rész pontos alkatrészeivel (műveleti erősítők, visszacsatolás, VREF) az [analog.svg](analog.svg). Itt a működése.

### 2.1 Táp
- 2× 18650 sorosan (7,4 V), védelmi áramkörrel, kapcsolóval és 1 A-es biztosítékkal.
- A **+7V4** a Nano **VIN** lábára megy, és a TX ágat táplálja.
- A Nano 5 V-ját a 100 Ω + 100 µF szűrő választja le **+5V_A** analóg sínné az MCP6004 és a VREF számára.
- A TX árama külön, vastag GND-ágon folyjon, és csak egy ponton (csillagpont) csatlakozzon az analóg földhöz.

### 2.2 TX: soros rezgőkör
`+7V4 → 2,2 Ω/2 W → CTX 1 µF → L_TX → MOSFET drain`

- A **Q1 (IRLZ44N)** logikai szintű MOSFET. A gate-et a Nano **D9** lába hajtja 100 Ω-on át, a gate–GND között 10 kΩ lehúzó van.
- A D9 a Timer1 hardveres **OC1A** kimenete. A frekvencia a firmware-ből: `f_TX = 500 000 / N` Hz, N = 90…150, tehát **3,3–5,6 kHz**. Alapérték N = 110, azaz **4545 Hz**.
- A **CTX 1 µF fólia (MKP/MKT, ≥ 63 V)**. 1 mH-s tekercsnél a rezonancia kb. 5 kHz. A rezgőkörön a feszültség elérheti a 20–25 V csúcsot, ezért kell a 63 V.
- **D1 (1N5819 Schottky)** a drain és a +7V4 között védi a MOSFET-et a visszalökési feszültségtől. A drain mellé 470 µF + 100 nF kerül.
- A 2,2 Ω-os ellenállás korlátozza az áramot. Becslésem szerint a csúcsáram kb. 0,75 A, átlagfogyasztás kb. 0,3–0,4 A. Ezt mérd meg.

### 2.3 RX lánc
`L_RX ∥ CRX (1 µF) → 1 µF csatolás → IC2A → 1 µF csatolás → IC2B → 1 kΩ + 10 nF → A0`

- Az **L_RX** és a **CRX 1 µF** párhuzamos rezgőkör a GND-hez képest, ezért az RX a TX frekvenciáján a legérzékenyebb.
- **IC2A (MCP6004)**: neminvertáló erősítő, ×11 (Rf 10 kΩ, Rg 1 kΩ). Az Rg a 100 µF-on át megy a GND-re, így az egyenszinten az erősítés 1.
- **IC2B (MCP6004)**: ugyanilyen, de az Rf egy **10 kΩ-os trimmer**, amellyel az erősítés ×1…×11 között állítható. Ez a **GAIN** beállítás.
- A bemenetek **VREF ≈ 2,5 V** közepére vannak előfeszítve. A VREF-et 2×100 kΩ osztó (R1, R2) + 10 µF adja, amit az **IC2D** feszültségkövető pufferel (a 10 µF az osztón, a puffer bemenetén van, a kimenetre max. ~100 pF kerülhet). A fokozatok 100 kΩ-on át (R4, R6) kapják a VREF-et.
- Bemeneti védő ellenállások: R3 és R5 = 4,7 kΩ; az Rf-eken 47 pF (Cf1, Cf2) a stabilitásért; kimeneti RC: R7 1 kΩ + C5 10 nF. Pontos értékek: [analog.svg](analog.svg).
- Az 1 kΩ + 10 nF aluláteresztő szűrő a nagyfrekvenciás zajt vágja az A0 előtt.
- A teljes erősítés legfeljebb kb. ×121.

### 2.4 AC antenna (élő vezeték)
Rézlap (kb. 5×5 cm, árnyékolt kábellel) → 10 MΩ a VREF-hez → **MCP6004 feszültségkövető (IC2C)** → 100 Ω → **A1** (+ 100 nF a GND-re).

### 2.5 Nano bekötés

| Láb | Funkció |
|---|---|
| D9 (OC1A) | TX gate (100 Ω-on át) |
| A0 | RX erősítő kimenete |
| A1 | 50 Hz antenna |
| D2 | BTN1 (GND-re, belső felhúzás) |
| D4 | BTN2 (GND-re, belső felhúzás) |
| D3 | aktív csipogó (+), a másik láb GND |
| A4 / A5 | OLED SDA / SCL (SSD1306 128×64, cím 0x3C) |
| VIN | +7V4 |
| 5V | 100 Ω + 100 µF → +5V_A |

A D5 és a Timer1 külső számláló bemenet már nem használt.

### 2.6 Alkatrészlista (összefoglaló)
Arduino Nano, SSD1306 I2C OLED, aktív csipogó (3–5 V), IRLZ44N (vagy IRL540N, IRLZ34N), 1N5819, 1× MCP6004 (DIP-14, IC2: A, B = RX lánc, C = antenna követő, D = VREF puffer; táp: pin 4 = +5V_A, pin 11 = GND), 3 db 1 µF fólia kondenzátor (1 CTX, 1 CRX; plusz 1–2 csatolásra), 470 µF, 10 µF és 100 µF elkók, 100 nF kerámiák, 10 kΩ trimmer, ellenállások (2,2 Ω/2 W, 100 Ω, 10 kΩ, 1 kΩ, 100 kΩ, 10 MΩ), 2 nyomógomb, 2×18650 + védelem + kapcsoló + 1 A biztosíték, 2 db 0,4 mm-es CuZ tekercs, rézlap az antennának.

---

## 3. Szoftver

A firmware a [src/main.cpp](src/main.cpp) fájl. Fordítás és feltöltés: `pio run -t upload`. A Serial monitor 115200 baudon fut.

### 3.1 Időzítés
- **Timer1**, fázishelyes PWM (mód 9), TOP = OCR1A = N, órajel 16 MHz / 8 = 2 MHz. Az OC1A minden periódusban egyszer vált, így a TX frekvenciája 500 000 / N.
- Az **OCR1B = N/2** egyezése indítja az ADC-t (auto-trigger). Ez minden TX-periódusban **négyszer** történik, a periódus negyedeinél.
- **ADC**: 500 kHz-es órajel (osztó 32, 26 µs/konverzió), referencia AVcc. Ez a leggyorsabb beállítás, amely még belefér a legrövidebb mintaközbe (N = 90-nél 45 µs). Az ajánlott 50–200 kHz-nél gyorsabb, ezért a tényleges felbontás kb. 9 bit; a sok minta átlagolása ezt nagyrészt kompenzálja. Az ADC megszakítás (ISR) kezeli a mintákat.
- Timer0 változatlan (`millis()`), a Timer2 szabad. A csipogót `digitalWrite` vezérli, `tone()` nincs.

### 3.2 Szinkron demoduláció (I/Q)
Az ISR a mintát a TX fázisa szerint sorolja be (a Timer1 számlálási iránya és az OC1A szintje alapján), 4 helyzetbe:

- **I = M0 − M2**, **Q = M1 − M3** (az M a négy fázis mintaösszege).
- Az egyenszint (VREF) és a lassú zaj kiesik, mert a különbséget képezzük.
- Egy mérési keret **20 ms** hosszú (1 hálózati periódus), így az 50 Hz zavar kiátlagolódik. A TX-periódusok száma: `10000 / N`.
- A keretekből a program **I** és **Q** átlagot ad. A jelnagyság `m = √(ΔI² + ΔQ²)`, a fázis `atan2(ΔQ, ΔI)`.
- A mintaértékek 8-nál kisebb vagy 1015-nél nagyobb értéke **telítésnek** számít, ezt az OLED jelzi („TELITES!").

### 3.3 Csatornák váltása
A keretek 3:1 arányban váltakoznak: **3 RX-keret, 1 AC-keret** (az ADC multiplexere A0 és A1 között vált). Az RX így kb. 37 frissítést ad másodpercenként.

Az AC keretben 4 egymás utáni mintát összegez (a TX és harmonikusai kiesnek), és kiszámítja az 50 Hz-es jel RMS-ét.

### 3.4 Kalibráció és drift
- **Kalibráció** (indításkor és BTN1 rövid): 50 RX keretet és legalább 12 AC keretet mér. Az RX alapszint (`bI`, `bQ`) az átlag, a küszöb `thr = max(4σ, 0,4)`. Az AC küszöb `max(5σ, 0,4)`.
- **Drift-követés:** ha a jel a küszöb alatt van, az alapszint lassan (1/128 súllyal, kb. 3–4 s időállandóval) követi a mért értéket.
- **Simítás:** az RX értéken 0,3-as, az AC értéken 0,5-ös exponenciális szűrő fut.

### 3.5 Vas / nemvas
Betanításkor a program az aktuális fázist elmenti az **EEPROM-ba**. Működés közben, ha `cos(fázis − betanított) > 0`, az a vas, különben a nemvas fém. Ha nincs betanítva, a kijelző „FEM (tanitsd be)" feliratot ad.

### 3.6 Automatikus hangolás
BTN2 hosszú nyomására a program végigpásztázza az N = 90…150 értékeket, és kiválasztja azt, ahol az RX jele a legnagyobb (a rezonanciacsúcs). A hangolást **fémmel a tekercs fölött** kell végezni, mert nullázott tekercsnél a jel túl kicsi lenne a csúcs megtalálásához. Az eredmény az EEPROM-ba kerül. Kb. 10 s.

### 3.7 Kezelés

| Gomb | Rövid | Hosszú (≥ 1,2 s) |
|---|---|---|
| **BTN1** (D2) | kalibráció | nullázó mód be/ki |
| **BTN2** (D4) | vas betanítása | automatikus hangolás |

### 3.8 Kijelző és hang
- **FEM sáv:** az RX jel erőssége (a küszöb 25-szörösénél telik be).
- **AC sáv:** az élő vezeték jele (a küszöb 8-szorosánál telik be).
- Alsó sor: `-`, `FEM (tanitsd be)`, `VAS`, `NEM-VAS (Cu/Al)`, `ELO VEZETEK!`, `TELITES!`. Mellette `m=` (jelerősség, relatív egység) és `f=` (TX frekvencia).
- **Nullázó mód:** az RX nyers erősségét mutatja, a csipogás ilyenkor sűrűbb, ha nagyobb a jel. A cél a **legritkább csipogás / legkisebb érték**.
- **Csipogó:** egy rövid csippanás, amelynek üteme az erősséggel gyorsul (600 ms-tól 50 ms-ig). Vasnál és betanítatlan fémnél 25 ms, nemvas fémnél 70 ms hosszú. Élő vezetéknél folyamatos, gyors csipogás.
- **Serial kimenet:** 200 ms-onként `jelerősség,fázis_fok,ac` (CSV), a hangoláshoz.

---

## 4. Tekercsek elkészítése

Két azonos tekercs kell: **60 menet, 0,4 mm-es zománcozott rézhuzal (CuZ)**, D alakú, **150 mm magas × 125 mm mély** (a huzal középvonalán mérve). A méretezett rajz: [coil.svg](coil.svg), 1:1 nyomtatható sablon: [coil_sablon.svg](coil_sablon.svg).

### 4.0 Pontos méretek (egy tekercs)
| Méret | Érték |
|---|---|
| Alak | 150 mm-es egyenes oldal + félellipszis (féltengelyek: 125 mm mély, 75 mm félmagas) |
| Középvonal kerülete | kb. 469 mm → 60 menet ≈ **28,1 m** huzal (hagyj +0,6 m kivezetésre) |
| Horony a vázon | 6 mm széles, 4 mm mély, a középvonalon |
| Külső fal / belső fal | +3 mm / −3 mm a középvonaltól |
| Váz külső mérete | kb. 156 × 131 mm, belső ablak kb. 144 × 119 mm |
| Köteg | 60 × 0,45 mm (lakkal) ≈ 12 mm², ami a 6×4 mm-es horonyban elfér |
| Átfedés (kiindulás) | **31 mm** (a mélység 25%-a), nullpont várhatóan 25–38 mm között |
| Összes szélesség átfedéssel | 219 mm × 150 mm |

Az íves rész pontjai (a kör egyenes oldalától mért mélység → félmagasság a hosszanti tengelytől):

| Mélység (mm) | 0 | 25 | 50 | 75 | 100 | 115 | 120 | 125 |
|---|---|---|---|---|---|---|---|---|
| ± félmagasság (mm) | 75,0 | 73,5 | 68,7 | 60,0 | 45,0 | 29,4 | 21,0 | 0 |

Ezekre a pontokra üss szöget vagy fúrj csaplyukat (a [coil_sablon.svg](coil_sablon.svg)-n jelölve), és kösd össze simán. Az induktivitás (kb. 1 mH) számított becslés, ezért méréssel ellenőrizd, és szükség esetén a CRX-szel hangold (5. fejezet).

### 4.3 Kisebb 60 × 50 mm-es tekercsváltozat

A [coil_sablon_60x50.svg](coil_sablon_60x50.svg) egy tekercs 1:1-es sablonja: 60 mm-es egyenes oldal, 50 mm-es mélység, 60 mm teljes magasság. Két egyforma tekercs kell. A görbe egy 50 × 30 mm-es félellipszis.

Az 1 µF-os kondenzátorral és a kb. 5 kHz-es célfrekvenciával az elméleti célinduktivitás továbbra is kb. 1,0 mH. A korábbi, nagy tekercs 60 menetéhez képest a lineáris méret 0,4-szeres; az egyszerű geometriai skálázás szerint azonos induktivitáshoz kb. `60 / sqrt(0,4) = 95` menet adódik. Ez becslés, mert a tekercs induktivitását a huzalköteg alakja, a rétegek és a TX/RX kölcsönös csatolása is befolyásolja.

- Kezdő érték: **95 menet 0,4 mm CuZ-ből**; kb. 17,8 m a tekercsre, kivezetésekkel együtt kb. 18,4 m huzal tekercsenként.
- Becsült egyenáramú ellenállás: kb. 2,5 Ω/tekercs. Mérés után hasonlítsd össze a tekercseket.
- A 95 menetes huzalköteghez ne használd automatikusan a nagy változat 6 × 4 mm-es hornyát: a huzalok elrendezésétől függően kb. 6–8 mm széles és 5–6 mm mély hely kellhet. A tekercs középvonala maradjon a sablon piros vonalán.
- Kezdő átfedés: a mélység 25%-a, azaz **12,5 mm**. A tényleges nullpontot a készülék RX minimumával kell megkeresni; ez nem garantált méret.
- Az induktivitást mérd meg. `L ≈ 1,0 mH` esetén a 1 µF-os kondenzátorral a rezonancia kb. 5,0 kHz. Ha nem megfelelő, a tekercsmenetszám vagy a kondenzátor értéke alapján újra kell hangolni; a firmware automatikus hangolása csak a beállítható frekvenciatartományon belül működik.

**Várható érzékelési mélység:** a 50 mm mélységű változat hatótávolsága kisebb lesz, mint a 125 mm mélységűé. Durva, nem mért becslésként nagyobb betonvasnál kb. **2–5 cm**, 6–8 mm-es vasnál kb. **1–3 cm** nagyságrend várható kedvező nullázás és alacsony zaj mellett. A valós érték erősen függ a vas méretétől/irányától, betonfedéstől, tekercspártól és a környezeti zajtól; ezt csak próbával lehet megállapítani. A kisebb fej pontosabban lokalizálhat közeli vasakat, viszont mélyebben lévő tárgyakat rosszabbul érzékel.

### 4.1 Szükséges
- 2 × kb. 28,7 m CuZ huzal.
- Váz vagy sablon a fenti méretekkel (nyomtasd ki a [coil_sablon.svg](coil_sablon.svg)-t 100%-ban, ragaszd 4–6 mm-es rétegelt lemezre/MDF-re, fúrd ki a csapokat, vagy vágd ki a kontúrt).
- Szigetelőszalag vagy hajszálragasztó, epoxi, csiszolópapír, multiméter.

### 4.2 Lépések
1. Tekerj **60 menetet** szorosan, egymás mellé a sablon horonyába. Számold a meneteket. A végén hagyj mindkét végén kb. 30 cm-es kivezetést.
2. Kösd át a tekercset szigetelőszalaggal **több helyen**, így a köteg együtt marad.
3. Ha csapos sablont használsz, a csapok közül kiemelheted a köteget. Az alak már D, külön hajlítani nem kell.
4. Csiszold le a kivezetések lakkját (kb. 5 mm), és ellenőrizd az ellenállást: 60 menetnél a számított érték kb. 3,5–4,5 Ω.
5. Mérj induktivitást, ha tudsz. A számított érték kb. **1 mH**, de a D alak kicsit eltérhet. Ha 1 mH-tól nagyon eltér, a rezonancia 5 kHz-es célt ±1 kHz-en belül tartsd (lásd 5. fejezet).
6. Ragaszd a tekercseket **merev, fém- és ferritmentes vázra** (fa, műanyag, textolit). A huzal ne mozogjon, mert a mozgás jelként látszik.
7. Használj árnyékolt kábelt az RX tekercshez a nullázás után.

---

## 5. A rezgőkörök frekvenciája

A TX soros rezgőkör rezonanciája `f = 1 / (2π√(L·C))`. 1 mH és 1 µF esetén f ≈ 5,03 kHz, tehát benne van a 3,3–5,6 kHz-es tartományban.

- Ha a mért vagy számított f **5,6 kHz fölé** esne, tegyél a CTX mellé 0,47 µF fóliakondenzátort párhuzamosan (a CRX-et is hasonlóan).
- Ha **3,3 kHz alá**, csökkentsd a C-t.
- A TX és az RX rezgőkör egyezzen, ezért a TX és az RX tekercs, valamint a CTX és a CRX azonos legyen.

---

## 6. Üzembe helyezés és behangolás

### 6.1 Ellenőrzés tekercsek nélkül
1. Programozás: `pio run -t upload`. Az OLED-en meg kell jelennie a „KALIBRALAS" feliratnak, majd a főképernyőnek.
2. Mérd a **D9** lábat (oszcilloszkóppal vagy Hz-mérős multiméterrel): kb. 4545 Hz-es 50%-os négyszög legyen.
3. Mérd az A0 lábat: egyenszintje kb. 2,5 V legyen (a trimmert állítsd minimumra, ×1).
4. Mérd az 50 Hz antennát: ujjal megérintve az AC sávnak mozdulnia kell.

### 6.2 TX bekapcsolása
1. Kösd be a TX tekercset és a CTX-et. Mérd a 2,2 Ω-os ellenálláson a feszültséget, abból kiszámítható az áram: legfeljebb kb. 1 A legyen, az akku ne melegedjen.
2. A MOSFET és a 2,2 Ω-os ellenállás enyhén melegedhet, de ne legyen forró.
3. Ha az áram túl nagy, növeld az ellenállást.

### 6.3 Nullázás (a legfontosabb lépés)
1. Kösd be az RX tekercset és az erősítőt. A trimmert állítsd a közepére (×5 körül).
2. **BTN1 hosszan:** nullázó mód. Az OLED az RX jelet mutatja, a csipogás sűrűsége követi.
3. Csúsztasd a két tekercset vízszintesen egymáshoz képest. Kezdd 31 mm-es átfedéssel (25–38 mm között keresd) ([coil.svg](coil.svg)). Keresd meg azt a helyzetet, ahol az érték a **legkisebb**.
4. Ha nem találsz minimumot, finomítsd az átfedést és az egymás közti távolságot (egy kis mozgatás is számít). Közben tartsd a tekercseket távol fémtől.
5. Rögzítsd a tekercseket **epoxival**.
6. **BTN1 hosszan:** kilépés a nullázó módból, majd **BTN1 röviden:** kalibráció.

### 6.4 Gain és hangolás
1. Tartsd a tekercset **fémtől távol**, és emeld a trimmerrel az erősítést addig, amíg a kijelzőn nem jelenik meg a „TELITES!". Ezután csökkentsd egy kicsit. Ha a nullázás jó, magas erősítés is lehetséges.
2. **BTN2 hosszan:** automatikus hangolás. Közben tarts egy vasdarabot (pl. betonvasat) a tekercs elé. Kb. 10 s, a végén a kijelző kiírja a kiválasztott frekvenciát.
3. Kalibrálj újra (**BTN1 röviden**).

### 6.5 Vas betanítása
1. Tarts egy **vasdarabot** (betonvas, szög) a tekercs elé, amíg a FEM sáv legalább a küszöb 3-szorosára nő.
2. **BTN2 röviden:** a készülék a fázist elmenti („VAS BETANITVA").
3. Ellenőrzés: egy rézdarab vagy alumínium a „NEM-VAS" feliratot kell, hogy adja.
4. Ha az automatikus hangolás után a frekvencia megváltozott, tanítsd be újra.

### 6.6 Használat falon
1. Kalibrálj **a fal egy üres részén** (BTN1 röviden), és ne mozdítsd a készüléket közben.
2. Húzd lassan a falon, a tekercs lapja a falra simuljon.
3. A jel a vas fölött éri el a maximumot. A pontos helyét keresztirányú mozgatással határozd meg.
4. Új falfelületen, vagy ha elcsúszik az alapszint, kalibrálj újra.

---

## 7. Hibaelhárítás

| Tünet | Lehetséges ok |
|---|---|
| Nincs jel, az m mindig ≈ 0 | A TX nem működik (D9, MOSFET, CTX), az erősítés túl kicsi, vagy az A0 lebeg |
| „TELITES!" mindig | Az erősítés túl nagy, vagy a tekercsek nincsenek nullázva |
| Az érték folyamatosan driftel | A tekercs mozog, a hőmérséklet változik, gyenge a táp. Rögzítsd a tekercset, használj friss akkut |
| Nem találsz nullpontot | A két tekercs nem azonos, a TX és az RX rezgőkör eltér, vagy fém van a közelben |
| Zajos, ugráló kijelzés | Rossz földelés, a TX és RX kábel közel van, kapcsolóüzemű táp zaja. Csillagpontos föld, árnyékolt RX-kábel |
| A frekvencia a tartomány szélén van | A C vagy az L eltér a tervezettől (5. fejezet) |
| Az AC csatorna nem reagál | Az antenna kábele nincs árnyékolva, a 10 MΩ a VREF-re van kötve, vagy az IC2C kimenete nem az A1-re megy |
| Vas és nemvas felcserélődik | Újra kell tanítani (6.5), különösen a hangolás után |

---

## 8. Ismert korlátok

- A hardver **nem tesztelt**. Az áram- és frekvenciaadatok, valamint az átfedés mértéke (25–38 mm) becslések.
- Az AC csatorna csak feszültség alatt lévő vezetéket jelez. Feszültségmentes kábelt az IB tekercs is csak akkor, ha fém.
- Műanyag vízcsövet fémdetektor nem lát.
- A vas/nemvas döntés a fázis előjelére épül. Ez a gyakorlatban a tekercstől, a frekvenciától és a céltárgy méretétől is függ.
- A csatornaváltás utáni első minta kismértékben torzulhat (ADC multiplexer váltás). Ez a mérésben kiátlagolódik.
- A TX és az ADC mintavétel időzítése miatt a TX frekvencia 3,3–5,6 kHz között használható (N = 90…150).
