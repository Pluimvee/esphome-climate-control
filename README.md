# SmartControl — Analyse van de originele Arduino-logica

Bronbestanden: `SmartControl.cpp`, `HeatingCurve.cpp`, `Temperature.cpp`

---

## 1. Temperatuurverwerking (Temperature.cpp)

Elke temperatuurvariabele (inside, outside, setpoint, inlet, outlet, target) is een `Temperature`-object met dezelfde pipeline:

### 1a. Validatie & spike-detectie bij binnenkomst

Bij elke nieuwe meting (`set()`):

1. **Min/max grenzen** — waarden buiten het absolute bereik worden genegeerd.
2. **Rate limiter** — de wijziging per seconde mag een drempel (`max_diff_psec`) niet overschrijden. Per sensor:
   - inside/outside: `0.02 °C/s` (maximaal ~1.2 °C/min)
   - inlet/outlet: `1.0 °C/s` (snelle wijzigingen bij defrost toegestaan)
3. **Spike-detectie** — als de buffer vol is en k ≠ 0: reject als `|waarde − gemiddelde| > k × stddev`. k = 3.0 voor inside/outside/inlet/outlet.

Spikes worden **wel** toegevoegd aan de statistiekbuffer (om de stddev te updaten), maar **niet** als `_cur_val` opgeslagen. Zo blijft de huidige waarde de laatste niet-spike meting.

### 1b. Statistieken

Twee lopende buffers per temperatuur (beide 10 entries, `STATISTICS_BUFFER_SIZE`):

| Buffer | Interval | Dekking | Gebruik |
|--------|----------|---------|---------|
| `_statistics` | 30 s | ~5 minuten | spike-detectie, `average()`, recente trend |
| `_longterm_stat` | 5 min (= 30s × 10) | ~50 minuten | lange termijn basis voor trend |

`average()` geeft `_statistics.getFastAverage()` — een lopend gemiddelde over de laatste ~5 minuten.

### 1c. Trend

```
trend = avg_recent_5min − avg_long_50min
```

- Positief → temperatuur stijgt t.o.v. 50-minuten gemiddelde  
- Negatief → temperatuur daalt  
- Ruisfilter: als `|recent − long| < 0.1 °C` → trend = 0  
- Vereist minstens één longterm-sample (duurt 5 minuten na opstart)

De trend van **setpoint** (niet inside) wordt gebruikt in de schakellogica.

---

## 2. Stooklijn / setpoint-berekening (HeatingCurve.cpp)

De gewenste watertemperatuur wordt elke 10 seconden (`ANALYSE_TIME`) herberekend.

### Parameters

| Constante | Waarde | Betekenis |
|-----------|--------|-----------|
| `factorA` (STOOKLIJN_FACTOR) | 0.65 | Basisvermenigvuldiger voor buiten-delta |
| `factorB` (INSIDE_FACTOR)    | 0.20 | Correctie per graad binnen-afwijking |
| `factorC` (CURVE_FACTOR)     | 0.00 | Kwadratisch curvedeel (uitgeschakeld) |

### Berekening stap voor stap

```
delta_outside = target − outside        # positief als het buiten kouder is dan target
delta_inside  = target − inside         # positief als het binnen kouder is dan target

factor = factorB × delta_inside         # bijdrage van binnentemperatuurafwijking
if delta_outside < 0:                   # het is buiten warmer dan doel (zomer)
    factor = -factor                    # keer om: minder koelbehoefte bij warme kamer
if factor < 0:
    factor = 0                          # natuur doet de rest; geen actieve correctie
factor += factorA                       # basisvermenigvuldiger altijd optellen
factor = min(factor, 1.175)            # bovengrens = W55 stooklijn

curve = delta_outside                   # (factorC = 0, dus lineair)

setpoint = target + factor × curve
```

**Vereenvoudigd:**

```
setpoint = target + (0.65 + 0.20 × (target − inside)) × (target − outside)
```

Voorbeeldwaarden bij target = 21, inside = 20, outside = 5:
```
delta_outside = 21 − 5  = 16
delta_inside  = 21 − 20 = 1
factor        = 0.65 + 0.20 × 1 = 0.85
setpoint      = 21 + 0.85 × 16  = 34.6 °C
```

---

## 3. Schakellogica (set_operating_mode)

Wordt elke 10 seconden geëvalueerd. Wijzigingen zijn alleen toegestaan als de anti-pendel timer voorbij is.

### Anti-pendel

```
ANTIPENDEL_TIMEFRAME = 30 minuten
```

Na elke schakelactie (aan of uit, verwarmen of koelen) wordt de timer opnieuw gezet. Geen schakeling mogelijk binnen 30 minuten na de vorige.

### Volgorde van controles (eerste match wint)

#### Koelen AAN
```
Cooling == OFF
&& inside.valid() && inside > 25 °C
&& outside.valid() && outside > 25 °C
```

#### Koelen UIT
```
Cooling == ON
&& inside.valid() && inside < 25 °C
```

#### Verwarmen AAN (predictief)
```
Heating == OFF
&& inside.valid() && inside < 25 °C
&& target.valid() && setpoint.valid()

error = round1(inside − target)          # positief = te warm
trend = round1(setpoint.trend())         # positief = meer verwarming nodig

→ AAN als: 2 × trend > error
```

Dit is een **feedforward + feedback predictor** — en het is niet toevallig dat hier de trend van het *setpoint* wordt gebruikt in plaats van de trend van *inside*:

- **inside.trend()** reageert alleen op een dalende binnentemperatuur — dat is puur feedback, dus altijd te laat.
- **setpoint.trend()** reageert op *zowel* een dalende binnentemperatuur *als* een dalende buitentemperatuur. Een buitentemperatuurdaling verhoogt de stooklijn direct (via `delta_outside`), nog voordat de kamer heeft afgekoeld.

Zo wordt de buitentemperatuur een **leading indicator**: de verwarming gaat aan zodra de warmtevraag toeneemt, niet pas nadat de kamer koud is geworden. Dit verklaart waarom het systeem twee jaar lang goed heeft gefunctioneerd.

#### Verwarmen UIT
```
Heating == ON
&& setpoint.valid() && inlet.valid()
&& round1(inlet − setpoint) > 0.1 °C    # retourtemperatuur 0.2 °C boven setpoint
```

---

## 4. Analyse: dezelfde predictieve logica voor koelen?

### Wat de stooklijn-formule doet in de zomer

De bestaande `HeatingCurve::calculate()` formule werkt ook correct in de zomer, zonder aanpassing. Wanneer `outside > target` (bijv. 30 °C buiten, target 23 °C):

```
delta_outside = 23 − 30 = −7          # negatief in zomer
delta_inside  = 23 − 24 = −1          # inside (24°C) iets boven target

factor = 0.20 × (−1) = −0.2
→ delta_outside < 0, dus factor = −(−0.2) = +0.2    # inversie voor koelen
→ factor += 0.65 = 0.85
setpoint = 23 + 0.85 × (−7) = 15.65 °C              # koelwater-setpoint
```

De formule levert dus automatisch een koelwater-temperatuur (onder target) op zodra het buiten warmer is dan target. De `factor *= −1` inversie zorgt ook dat een warme binnenkamer (`inside > target`) de setpoint verder verlaagt — meer koelvermogen — wat correct is.

### Trend van setpoint in de zomer

| Situatie | setpoint.trend() | Betekenis |
|---|---|---|
| Outside stijgt → setpoint daalt | **negatief** | meer koelvraag |
| Outside daalt → setpoint stijgt | **positief** | minder koelvraag |
| Inside stijgt → setpoint daalt | **negatief** | meer koelvraag |

De trend heeft dus de **omgekeerde polariteit** ten opzichte van de verwarmingssituatie.

### Predictieve schakelaar voor koelen (inverse logica)

De originele heating-ON conditie `2 × trend > error` werkt **niet** voor koelen: als de koelvraag stijgt is trend negatief, maar error = inside − target is positief (het is warm) — de conditie wordt dan altijd false.

De symmetrische inversie is:

```
Heating ON:  2 × trend > error           →  trend positief = meer warmte nodig
Cooling ON:  2 × trend < error           →  trend negatief = meer koeling nodig
```

Of identiek opgeschreven: `error > 2 × trend`

Verificatie met voorbeeldwaarden:

| inside | target | error | trend | 2×trend | cooling ON? | Uitleg |
|---|---|---|---|---|---|---|
| 24.0 | 23 | +1.0 | −0.3 | −0.6 | 1.0 > −0.6 ✓ | Ja, te warm + vraag stijgt |
| 22.8 | 23 | −0.2 | −0.3 | −0.6 | −0.2 > −0.6 ✓ | **Predictief aan**: buiten stijgt, inside nog onder target |
| 22.0 | 23 | −1.0 | −0.3 | −0.6 | −1.0 > −0.6 ✗ | Nee, inside duidelijk onder target |
| 23.1 | 23 | +0.1 | +0.05 | +0.1 | 0.1 > 0.1 ✗ | Nee, koelvraag neemt af |

Het derde geval is essentieel: als het buiten hard stijgt terwijl het binnen nog koel is, schakelt de koeling al preemptief in. Exact hetzelfde feedforward-gedrag als bij verwarming.

### Uitschakelconditie koelen (symmetrisch)

```
Heating OFF:  round1(inlet − setpoint) > 0.1    # retour stijgt tot boven aanvoer-setpoint
Cooling OFF:  round1(setpoint − inlet) > 0.1    # retour daalt tot onder koelwater-setpoint
```

Beide condities zeggen hetzelfde: het water dat terugkomt heeft de gewenste temperatuur bereikt — het systeem heeft genoeg geleverd.

### Conclusie

De predictieve logica is volledig symmetrisch toepasbaar op koelen, met één aanpassing: de ON-conditie inverteert van `2×trend > error` naar `error > 2×trend`. De stooklijn-formule, de trend-berekening en de OFF-conditie werken zonder wijziging. De huidige implementatie (drempel op 25 °C) mist daarmee de feedforward op buitentemperatuur die de verwarmingszijde wél heeft.

---

## 5. OpenTherm communicatie

Cyclisch script van 10 berichten, elke 2 seconden één:

| Richting | Data-ID | Omschrijving |
|----------|---------|--------------|
| R/W | Status | Operating flags sturen; flame/fault lezen |
| W | TrSet | Kamerinstelling sturen |
| W | Tr | Huidige kamertemperatuur sturen |
| W | TSet | Gewenste watertemperatuur (setpoint) sturen |
| R | Toutside | Buitentemperatuur lezen van ketel |
| R | Tret | Retourtemperatuur (inlet) lezen |
| R | Tboiler | Aanvoertemperatuur (outlet) lezen |
| R | RelModLevel | Modulatieniveau lezen |
| R | CHPressure | CV-druk lezen |
| R | SlaveVersion | Versie ketel lezen |

Na 5 mislukte pogingen voor een Data-ID wordt dat bericht overgeslagen. Na 15 minuten (`ERROR_RESETTER`) worden alle tellers gereset.

---


