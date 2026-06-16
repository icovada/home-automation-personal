# Sump Pump Station Controller — Installation & Operation Manual

This panel automatically runs **two sump pumps that share one discharge pipe**.
It runs only one pump at a time, alternates them so they wear evenly, watches
each pump's electrical current to detect a stuck or failed pump, switches to the
healthy pump automatically, and raises a local alarm when something needs
attention. It is a **standalone** controller (no network/Wi-Fi).

> ⚠️ **Safety.** Pump motors run on mains voltage. All mains wiring, switching
> relays and motor protection must be installed by a qualified electrician and
> comply with local regulations. The controller drives **relay coils and signal
> circuits only** — it does not switch motor current directly. Always isolate
> power before wiring.

---

## 1. How it works (in plain terms)

Three float switches sit at three heights in the sump:

- **MIN** (lowest) — "empty enough, stop the pump."
- **1/2** (middle) — "water's up, start a pump."
- **3/4** (highest) — "high-water ALARM." *(Optional — fit it when you're ready; the controller already supports it.)*

Normal cycle: water rises to the **1/2** float → a pump starts → water falls to
the **MIN** float → the pump stops. The next time, the *other* pump runs (they
take turns).

A **current clamp** (Seneca T201) on each pump tells the controller how many
amps the pump is drawing. If a running pump draws too little (not actually
pumping, dry, or tripped), too much (jammed), or fails to lower the water in a
reasonable time (clogged), the controller declares that pump **faulty**,
switches to the other pump, and tries the faulty one again later. If a pump
keeps failing it is **locked out** until someone investigates and presses RESET.

---

## 2. What's on the panel

**Indicator lamps**

| Lamp | Meaning |
|------|---------|
| Pump 1 / Pump 2 **RUN** | that pump is energised right now |
| Pump 1 / Pump 2 **FAULT** | **steady** = temporary fault, will retry automatically · **blinking** = locked out, needs RESET |
| Level **MIN / 1-2 / 3-4** | which float is currently wet · a **blinking** level lamp = that float looks broken |
| **ALARM** (panel) | blinks during an emergency |
| **PRE-EMPTY** | flashes while a manual pre-empty drain is running, and for ~5 s after you press the button (to confirm the press) |

**Remote signals**

- **Big red beacon** — turns on whenever there's *any* problem (a pump fault, a float fault, or an emergency). "Come and look."
- **Siren** — sounds only in a real **emergency** (high water, or both pumps unavailable). "Act now."

**Controls**

- **Manual–Off–Auto** selector per pump (see §6).
- **SILENCE** button — mutes the **siren**; the beacon and lamps stay on.
- **RESET** button — clears faults and re-enables a locked-out pump (after you've fixed the cause).
- **PRE-EMPTY** button — drains the tank down now to make room before a storm (see §6).

---

## 3. What you need

- CONTROLLINO MAXI, powered from 24 V DC.
- Two pump switching relays — **socket-mounted Finder relays** (plug-in, so a failed one swaps out without rewiring), sized for the pump's current. A hardware interlock between them is **optional** (see §5.1).
- 3 float switches (normally-open, closing on water rise). The 3/4 float is optional for now.
- 2 × **Seneca T201** current transducers (4-20 mA output), one clamped on each pump's supply.
- 2 × **precision burden resistors** (≈ 500 Ω, 0.1 %) — one per 4-20 mA loop (see §5.3).
- 24 V indicator lamps (incl. a PRE-EMPTY lamp), a 24 V beacon (self-flashing type recommended) and a siren.
- MOA (Manual, Off, Auto) selectors and three momentary push-buttons (Silence, Reset, Pre-empty).

---

## 4. Terminal / pin assignment

Wire to the CONTROLLINO MAXI terminals as below (matches the firmware pin map).

**Relay outputs**

| Terminal | Connect to |
|----------|------------|
| `R0` | Pump 1 switching-relay coil (Finder) |
| `R1` | Pump 2 switching-relay coil (Finder) |
| `R2` | Beacon (big red flashing lamp) |
| `R3` | Siren |

**Digital outputs (24 V lamps)**

| Terminal | Lamp |
|----------|------|
| `D0` / `D1` | Pump 1 / Pump 2 RUN |
| `D2` / `D3` | Pump 1 / Pump 2 FAULT |
| `D4` / `D5` / `D6` | Level MIN / 1-2 / 3-4 |
| `D7` | Panel ALARM |
| `D8` | PRE-EMPTY active (flashing) |

**Inputs**

| Terminal | Input |
|----------|-------|
| `A0` / `A1` | Pump 1 / Pump 2 current clamp (T201, 4-20 mA via burden resistor) |
| `A2` / `A3` / `A4` | Float MIN / 1-2 / 3-4 |
| `A5` | Silence button |
| `A6` | Reset button |
| `A7` / `A8` | Pump 1 MANUAL / AUTO (from its MOA selector) |
| `A9` / `IN0` | Pump 2 MANUAL / AUTO (from its MOA selector) |
| `IN1` | Pre-empty button |

All 12 inputs are now used — there are **no spare inputs** left.

---

## 5. Wiring notes

### 5.1 Pump switching relays — interlock is optional
Only one pump should run at a time (they share one discharge pipe). **The
firmware guarantees this in software**: it only ever commands one pump on, and
the test suite checks the two outputs are never energised together. Running both
at once only makes them fight over the pipe and move less water — it does not
damage anything — so a hardware interlock is **optional belt-and-suspenders**,
not required.

This build uses **socket-mounted Finder relays** on `R0` / `R1`, chosen so a
failed relay plugs out and back in without rewiring. Size each relay for the
pump's running **and inrush** current (a motor's start surge is several times its
running current).

If you ever want the extra insurance, the cheapest option is an **electrical
interlock**: wire each relay's NC auxiliary/spare contact into the other's coil
feed so one energising drops the other. (A mechanical interlock bar exists for
contactor-style devices but isn't applicable to plug-in relays.)

### 5.2 Floats
Use normally-open floats that **close to +24 V as water rises** (active-high).
Run the MIN, 1/2 and (optional) 3/4 floats to `A2`, `A3`, `A4`. The 3/4 input may
be left unconnected for now — it reads "dry" and will not cause a false alarm.
(If you only have normally-closed floats, set `FLOAT_ACTIVE_HIGH` to `false` in
the firmware.)

### 5.3 Current clamps (Seneca T201) — the only tricky part
Each T201 outputs **4-20 mA** proportional to its pump's RMS current (set the
T201's range to **0-10 A**, or adjust `AMP_SPAN_A` in firmware to match). The
CONTROLLINO MAXI analog inputs read **0-24 V** full scale (NOT 0-5 V), so put a
**burden resistor across the input to ground** to convert the loop current into a
voltage that fits inside that range:

- ≈ **500 Ω** gives ~**2-10 V** for 4-20 mA — a good default: good resolution,
  well inside the 24 V range, and within the T201's drive capability.
  (≈ 250 Ω → ~1-5 V also works but uses less of the input range.)
- **Do not** size the burden so that 20 mA approaches 24 V — leave headroom.
- If the loop wire breaks, the input reads ~0 V; the controller detects this as a
  "broken current loop" sensor fault.

Then **calibrate** (§7).

### 5.4 Beacon & siren
Put the **beacon** on `R2` and the **siren** on `R3` as shown so the SILENCE
button can mute the siren while the beacon stays lit. Use a **self-flashing**
beacon (it does its own flashing; the relay just powers it). If you prefer, you
can wire the siren in parallel with the beacon on one relay — but then SILENCE
will mute both.

---

## 6. Operating — Manual / Off / Auto

Each pump has a 3-position selector:

- **AUTO** — normal automatic operation (floats + current monitoring). Leave both here for unattended running.
- **OFF** — that pump is disabled and will not run, even in an emergency.
- **MANUAL** — force-runs that pump now, ignoring the floats (for testing/priming). The single-pump interlock still applies; if both are set to MANUAL, only Pump 1 runs.

### Pre-emptying before a storm
Press the **PRE-EMPTY** button (pumps in AUTO) to drain the tank down to the MIN
level **now**, even though the water hasn't reached the 1/2 start float — this
frees up buffer capacity before heavy rain.

- The **PRE-EMPTY lamp flashes** to confirm the press was accepted. If there's
  water to pump, a pump runs and drains to MIN, then stops normally (and the next
  cycle alternates as usual). If the tank is already low, nothing pumps but the
  lamp still flashes for ~5 s so you know the button worked.
- It's fully monitored (same fault/alarm protection as a normal cycle) and never
  runs dry (it stops at MIN).
- To cancel a standing request, press **RESET**. (A drain already in progress
  finishes down to MIN — that's harmless and is the point.)
- If both pumps are OFF or unavailable, nothing runs; the flashing lamp only
  means the request was registered.

---

## 7. Commissioning checklist

1. **Power up** with both selectors at **OFF**. Confirm no pump runs and lamps are sane.
2. **Calibrate the current clamps** (do this once, per pump):
   - Connect a laptop to the CONTROLLINO USB and open the Arduino Serial Monitor at **115200 baud**. A status line prints every ~10 s, including `A=` (the two pumps' measured amps).
   - With the pump **off**, the reading should sit near **0 A**. With the pump **running at a known load**, compare the displayed amps to a clamp meter.
   - If they don't match, adjust `ADC_AT_4MA` / `ADC_AT_20MA` in `pompe.h` (these map the raw analog reading to 4 mA and 20 mA). The simplest method: note the raw analog counts at a known 4 mA (pump off) and 20 mA (or full-scale) and enter them. Re-flash and re-check.
   - Set `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` to bracket your pump's real running current (default 2-8 A) with some margin.
3. **Set `DRAIN_TIMER_MS`** to roughly how long a healthy pump takes to drop the level from the 1/2 float to the MIN float — this is used as a backup if the MIN float ever fails.
4. **Test each pump in MANUAL**, confirm the correct relay pulls in, the RUN lamp lights, and the current reads sensibly.
5. **Test AUTO**: raise the floats by manual (or fill the sump) and confirm a pump starts at 1/2 and stops at MIN, and that the **lead pump alternates** each cycle.
6. **Test the alarm**: trip the 3/4 float (or its input) and confirm beacon **and** siren; press **SILENCE** and confirm the siren stops but the beacon stays.
7. **Test PRE-EMPTY**: with water between MIN and 1/2, press the button → the PRE-EMPTY lamp flashes and a pump drains to MIN then stops. Press it again with the tank empty → no pump, but the lamp still flashes ~5 s.
8. Leave both selectors at **AUTO**.

---

## 8. What to do when something signals

| You see | Meaning | Action |
|---------|---------|--------|
| **Beacon on, no siren**, one FAULT lamp steady | A pump faulted; the other is covering. It will auto-retry in ~10 min. | Investigate that pump when convenient (blockage, check valve, breaker). No rush. |
| **Beacon on**, a FAULT lamp **blinking** | That pump is **locked out** after repeated faults. | Fix the pump, then press **RESET**. |
| A **level lamp blinking** | That float looks broken (a higher float is wet but this one says dry). | Check/replace that float switch. The station keeps running meanwhile. |
| **Siren + beacon**, ALARM lamp blinking | **Emergency**: high water (3/4) and/or both pumps unavailable. | Attend **now**. Press SILENCE to quiet the siren while you work. Check pumps, power, and floats. |

**RESET procedure:** after fixing the cause, press **RESET** once. This clears
faults, re-enables any locked-out pump, and un-silences the alarm. If the
underlying problem isn't fixed, the pump will simply fault again.

---

## 9. Troubleshooting

| Symptom | Likely cause | Check |
|---------|--------------|-------|
| Pump faults immediately (undercurrent) every run | Pump not drawing current | Breaker/overload tripped, motor disconnected, clamp/burden wiring, T201 range |
| Pump faults on **overcurrent** | Jammed impeller / locked rotor | Mechanical blockage; motor condition |
| Pump faults as **"not draining"** | Pumps but level won't drop | Clogged intake/impeller, stuck check valve, discharge blocked |
| A **FAULT lamp blinks** and the pump won't restart | Locked out (too many faults) | Fix the pump, press **RESET** |
| Current reading wrong on Serial | Not calibrated | Redo §7 step 2 (`ADC_AT_4MA`/`ADC_AT_20MA`) |
| Pump won't start though water is high | Selector at OFF, or anti-short-cycle wait, or locked out | Check MOA selector and FAULT lamps |
| Both pumps idle and beacon/siren on | Both unavailable (faulted/locked/OFF) | Check both FAULT lamps and selectors |
| Level lamp blinking | Float-consistency fault | Replace the suspect float |
| Nothing responds / panel seems frozen | The controller self-resets via watchdog if it ever hangs | If it persists, power-cycle and check Serial log |

---

## 10. Maintenance

- Periodically confirm both pumps still run (the alternation does this for you, but verify in MANUAL occasionally).
- Keep float switches and the sump free of debris.
- Re-check the current calibration after any pump or clamp change.
- The default timings (10-min retry, 15-s anti-short-cycle, etc.) suit most
  installations; a technician can fine-tune them in `pompe.h` (see
  [README.md](README.md)) and must re-run the test suite afterwards.
