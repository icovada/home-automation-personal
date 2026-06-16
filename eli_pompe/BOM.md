# Bill of materials — sump pump station

Fill in the blanks with the **actual** parts you installed (model numbers,
ratings, settings). A successor replacing a part must be able to buy/configure an
identical one. **Keep this committed to the repo and printed in the drawer.**

> ⚠️ Some settings **must match the firmware** (`eli_pompe/pompe.h`). Those rows
> are flagged 🔧 — get them wrong and the controller misreads or misbehaves.

## Controller & power

| Item | Spec / model (FILL IN) | Qty | Notes |
|------|------------------------|-----|-------|
| Controllino PLC | **CONTROLLINO MAXI** (___ part no.) | 1 | 24 V version. Firmware: `eli_pompe`. |
| 24 V DC power supply | ___ V/A, model ___ | 1 | Sized for board + all 24 V lamps + relay coils + beacon/siren. |
| Main fuse / MCB (control 24 V) | ___ A | 1 | |
| Enclosure | ___ | 1 | |

## Pumps & switching (mains side — electrician)

| Item | Spec / model (FILL IN) | Qty | Notes |
|------|------------------------|-----|-------|
| Sump pumps | ___ kW, ___ A nominal | 2 | Running current sets `NORMAL_AMP_MIN/MAX` 🔧 |
| Pump switching relays | **Finder**, socket-mounted, model ___ , coil ___ V, contacts ___ A | 2 | Plug-in (swap without rewiring). Coils driven by `R0` / `R1`. Size contacts for pump running **+ inrush** current. |
| Relay sockets + retainers | Finder ___ | 2 | So a failed relay plugs out/in without rewiring. |
| Interlock (optional) | electrical via NC aux, or none | 0–1 | **Not required** — software guarantees single-pump operation (MANUAL §5.1). Both-on only wastes flow, no damage. Mechanical interlock bars don't fit plug-in relays. |
| Motor overload / protection | ___ A per pump | 2 | |
| Pump circuit fuses/MCBs | ___ A | 2 | |

## Sensing

| Item | Spec / model (FILL IN) | Qty | Notes |
|------|------------------------|-----|-------|
| Float switches | ___ , **normally-open** (closes on rise) | 2–3 | MIN + 1/2 now; 3/4 optional. NO type ⇒ `FLOAT_ACTIVE_HIGH = true` 🔧 |
| Current transducers | **Seneca T201**, variant ___ | 2 | Output **4–20 mA**. |
| → T201 range setting | **set to 0–10 A** | — | **Must equal `AMP_SPAN_A` (10.0)** in firmware 🔧 |
| Burden resistors | **≈ 500 Ω**, 0.1 %, ≥ 0.5 W | 2 | One per 4–20 mA loop → ~2–10 V into A0/A1. Value feeds the `ADC_AT_4MA/20MA` calibration 🔧 |

## Indication & alarm

| Item | Spec / model (FILL IN) | Qty | Notes |
|------|------------------------|-----|-------|
| Pump RUN lamps | 24 V, ___ (green?) | 2 | `D0` / `D1` |
| Pump FAULT lamps | 24 V, ___ (red?) | 2 | `D2` / `D3` |
| Level lamps (MIN/1-2/3-4) | 24 V, ___ | 3 | `D4` / `D5` / `D6` |
| Panel ALARM lamp | 24 V, ___ (red?) | 1 | `D7`, blinks |
| PRE-EMPTY lamp | 24 V, ___ | 1 | `D8`, flashes while a pre-empty drain runs / ~5 s after a press |
| Remote beacon | 24 V, **self-flashing** | 1 | `R2`. Self-flashing so the relay stays steady (MANUAL §5.4). |
| Siren / sounder | 24 V, ___ | 1 | `R3` (separate relay so SILENCE mutes it). |

## Controls

| Item | Spec / model (FILL IN) | Qty | Notes |
|------|------------------------|-----|-------|
| MOA selector switch | 3-position **maintained** | 2 | Manual / Off / Auto, wired as 2 inputs/pump (A7+A8, A9+IN0). |
| Push-buttons | momentary, **normally-open** | 3 | SILENCE (`A5`), RESET (`A6`), PRE-EMPTY (`IN1`). |
| Terminal blocks, wiring, ferrules | — | — | |

---

## Firmware ↔ hardware cross-check (🔧 rows above)

If any of these change, update the matching constant in `eli_pompe/pompe.h` and
re-run the tests (`cd eli_pompe/tests && make`):

| Hardware | Firmware constant | Current value |
|----------|-------------------|---------------|
| T201 full-scale range | `AMP_SPAN_A` | 10.0 A |
| Pump normal running current | `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` | 2.0 / 8.0 A |
| Burden resistor + 4–20 mA zero/span | `ADC_AT_4MA` / `ADC_AT_20MA` | **calibrate on site, then commit** |
| Float contact type (NO/NC) | `FLOAT_ACTIVE_HIGH` | `true` (NO) |

Pin assignments are in `eli_pompe/eli_pompe.ino`; wiring detail in
[MANUAL.md](MANUAL.md) §4–§5.

---

## Spares — the "buy two" bag

Buy a second of everything and keep it in a labelled bag **next to the panel**.
On failure, swap rather than diagnose — far quicker and needs far less skill.
Quantities below are *spares to stock* (on top of what's installed).

| Spare | Qty | Swap difficulty | Notes |
|-------|-----|-----------------|-------|
| **CONTROLLINO MAXI — PRE-FLASHED** | 1 | medium (re-land wires) | **This is the important one.** Keep it already programmed with this firmware + committed calibration, so swapping needs **no laptop and no Arduino skills** — just move the wires. See below. |
| Finder pump relay | 2 | **easy** (plug-in) | Socketed → unplug/replug, no rewiring. |
| Float switch | 2–3 | easy | A few terminals. |
| Seneca T201 + burden resistor | 2 | easy | Set the new T201 to the **same 0–10 A range**; same burden value. |
| Indicator lamp / beacon / siren | a few | easy | |
| MOA selector, push-button | 1–2 | easy | |
| Fuses | several | easy | |

### Make the spare Controllino a true plug-in spare
A blank Controllino in a bag is useless to a non-programmer. To make the swap
skill-free:

1. **Pre-flash the spare** now with this firmware (see [RECOVERY.md](RECOVERY.md)),
   including the **calibrated** `ADC_AT_4MA`/`ADC_AT_20MA` values, and label it
   *"eli_pompe — flashed <date>"*.
2. **Label every wire** at the Controllino terminals (ferrules/tags) so re-landing
   them on the spare is mechanical — match the printed electrical diagram.
3. Then a failure is: power off → move the labelled wires to the spare → power on
   → run the quick checks in [MANUAL.md](MANUAL.md) §7. No PC needed.

When you use the spare, **buy and pre-flash another** so the bag is never empty.
`RECOVERY.md` is the fallback for when no pre-flashed spare exists.
