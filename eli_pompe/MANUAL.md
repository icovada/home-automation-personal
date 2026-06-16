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
take turns). To protect the motors, each pump is limited to **20 starts per hour**
(at least 3 minutes between its own starts) — so during very rapid cycling a start
may be held back briefly; that's normal.

**Auto-drain:** if water is sitting above the MIN float but never rises enough to
trigger a normal start, the station runs a pump **about once a day** to drain it
and keep both pumps exercised (it won't run if the sump is dry).

A **current sensor** (YHDC SCT010T-D) on each pump tells the controller how many
amps the pump is drawing. If a running pump draws too little (not actually
pumping, dry, or tripped) or too much (jammed), the controller declares that pump
**faulty**, switches to the other pump, and tries the faulty one again later. If a
pump keeps failing it is **locked out** until someone investigates and presses RESET.

If instead a pump is drawing **normal** current but the water still isn't going
down (the 1/2 float won't clear), the pump is fine — it's just **losing to the
inflow** (e.g. heavy rain) or slightly restricted. The controller does **not**
stop it (stopping a working pump would be the worst thing during a storm); it
keeps pumping and turns on the **beacon as a warning** — not the siren. If the
water keeps rising to the 3/4 float, that becomes a full emergency on its own.

---

## 2. What's on the panel

**Built-in lights on the controller** — the CONTROLLINO has a status LED for
every input and output. They show normal operation directly (no extra lamps):
- each **pump relay** LED = that pump is **running**;
- the three **float input** LEDs = the **water level** (and if a higher one is lit while a lower one is dark, that float is broken);
- the **beacon relay** LED = the **alarm** state.

**Panel lamps (wired to outputs)** — only the states that aren't obvious from the built-in LEDs:

| Lamp | Meaning |
|------|---------|
| Pump 1 / Pump 2 **FAULT** | **steady** = temporary fault, will retry automatically · **blinking** = locked out, needs RESET |
| Level **MIN / 1-2 / 3-4** | repeat of the water level for at-a-glance reading · a **blinking** level lamp = that float looks broken |
| **PRE-EMPTY** | flashes while a manual pre-empty drain is running, and for ~5 s after you press the button (to confirm the press) |

**Remote signals (the only external outputs)**

- **Big red beacon** — turns on whenever there's *any* problem (a pump fault, a float fault, or an emergency). "Come and look."
- **Siren** — sounds only in a real **emergency** (high water, or both pumps unavailable). "Act now."

**Controls**

- **Manual–Off–Auto** selector per pump (see §6).
- **SILENCE** button — mutes the **siren**; the beacon stays on.
- **RESET** button — clears faults and re-enables a locked-out pump (after you've fixed the cause).
- **PRE-EMPTY** button — drains the tank down now to make room before a storm (see §6).

---

## 3. What you need

- **CONTROLLINO MAXI**, powered from 24 V DC.
- Two pump switching relays — **socket-mounted Finder relays** (plug-in, so a failed one swaps out without rewiring), sized for the pump's current. A hardware interlock between them is **optional** (see §5.1).
- 3 float switches (normally-open, closing on water rise). The 3/4 float is optional for now.
- 2 × **YHDC SCT010T-D** split-core current sensors (10 A → 0-10 V output, 24 V powered), one clamped on each pump's supply. **No burden resistors** — the 0-10 V output goes straight to an analog input. (Pick the range to match the pump; see §5.3.)
- 6 × 24 V indicator lamps (3 level, 2 pump-fault, 1 pre-empty), a 24 V beacon (self-flashing type recommended) and a siren. (RUN and level are also shown on the controller's built-in LEDs.)
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

**Digital outputs (24 V lamps)** — RUN and the alarm state are on the controller's built-in relay LEDs, so no lamps for those.

| Terminal | Lamp |
|----------|------|
| `D0` / `D1` / `D2` | Level MIN / 1-2 / 3-4 |
| `D3` / `D4` | Pump 1 / Pump 2 FAULT |
| `D5` | PRE-EMPTY active (flashing) |

**Inputs**

| Terminal | Input |
|----------|-------|
| `A0` / `A1` | Pump 1 / Pump 2 current sensor (YHDC SCT010T-D, **0-10 V — no burden resistor**) |
| `A2` / `A3` / `A4` | Float MIN / 1-2 / 3-4 |
| `A5` | Silence button |
| `A6` | Reset button |
| `A7` | Pre-empty button |
| `A8` / `A9` | Pump 1 MANUAL / AUTO (from its MOA selector) |
| `IN0` / `IN1` | Pump 2 MANUAL / AUTO (from its MOA selector) |

All 12 inputs (A0–A9 + IN0/IN1) are used — there are no spare inputs.

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
Run the MIN, 1/2 and (optional) 3/4 floats to `A0`, `A1`, `A2`. The 3/4 input may
be left unconnected for now — it reads "dry" and will not cause a false alarm.
(If you only have normally-closed floats, set `FLOAT_ACTIVE_HIGH` to `false` in
the firmware.)

### 5.3 Current sensors (YHDC SCT010T-D)
Each sensor is a split-core (clamp-on) transducer that outputs **0-10 V**
proportional to its pump's RMS current — **10 A = 10 V** on the SCT010T-D. Clamp
one around each pump's live wire and run its 0-10 V output **straight to an analog
input** (`A0` / `A1`) — **no burden resistor**. It's a powered sensor: feed it the
panel supply (confirm 12 V vs 24 V on the unit). Set `AMP_SPAN_A` in firmware to
the sensor's full-scale amps (10 for the SCT010T-D); pick the sensor range so the
pump's running current sits around 30-50% of scale (a 10 A sensor suits a ~4 A
pump well). Note: a 0-10 V sensor has no "live zero", so a disconnected sensor
reads ~0 A and the pump simply faults on under-current (there's no separate
broken-wire detection).

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
   - If they don't match, adjust `ADC_AT_0A` / `ADC_AT_FS` in `pompe.h` (these map the raw analog reading to 0 A / full scale). The simplest method: note the raw analog count with the pump off (0 A) and at a known running current, and enter them. Re-flash and re-check.
   - Set `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` to bracket your pump's real running current (default 2-8 A) with some margin.
3. **Set `DRAIN_TIMER_MS`** to roughly how long a healthy pump takes to drop the level from the 1/2 float to the MIN float — this is used as a backup if the MIN float ever fails.
4. **Test each pump in MANUAL**, confirm the correct relay pulls in (its built-in relay LED lights) and the current reads sensibly.
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
| **Beacon on, no siren**, a pump **running**, no fault lamp | The pump can't keep up with the inflow (heavy rain) — it's working, not faulted. | Monitor. Not an emergency unless it escalates to the siren. |
| A **level lamp blinking** | That float looks broken (a higher float is wet but this one says dry). | Check/replace that float switch. The station keeps running meanwhile. |
| **Siren + beacon** | **Emergency**: high water (3/4) and/or both pumps unavailable. | Attend **now**. Press SILENCE to quiet the siren while you work. Check pumps, power, and floats. |

**RESET procedure:** after fixing the cause, press **RESET** once. This clears
faults, re-enables any locked-out pump, and un-silences the alarm. If the
underlying problem isn't fixed, the pump will simply fault again.

---

## 9. Troubleshooting

| Symptom | Likely cause | Check |
|---------|--------------|-------|
| Pump faults immediately (undercurrent) every run | Pump not drawing current, or sensor unplugged | Breaker/overload tripped, motor disconnected, sensor wiring/supply, sensor range |
| Pump faults on **overcurrent** | Jammed impeller / locked rotor | Mechanical blockage; motor condition |
| Beacon on, pump **running**, water staying high (no fault) | Pump can't keep up with inflow, or restricted | Usually just heavy rain — monitor. If it persists in dry weather: clogged intake/impeller, stuck check valve, or blocked discharge |
| A **FAULT lamp blinks** and the pump won't restart | Locked out (too many faults) | Fix the pump, press **RESET** |
| Current reading wrong on Serial | Not calibrated | Redo §7 step 2 (`ADC_AT_0A`/`ADC_AT_FS`) |
| Pump won't start though water is high | Selector at OFF, start-rate wait (≤20/hr), or locked out | Check MOA selector and FAULT lamps; brief wait is normal between rapid cycles |
| Both pumps idle and beacon/siren on | Both unavailable (faulted/locked/OFF) | Check both FAULT lamps and selectors |
| Level lamp blinking | Float-consistency fault | Replace the suspect float |
| Nothing responds / panel seems frozen | The controller self-resets via watchdog if it ever hangs | If it persists, power-cycle and check Serial log |

---

## 10. Maintenance

- Periodically confirm both pumps still run (the alternation does this for you, but verify in MANUAL occasionally).
- Keep float switches and the sump free of debris.
- Re-check the current calibration after any pump or clamp change.
- The default timings (10-min retry, 3-min start-rate limit, 24-h auto-drain, etc.) suit most
  installations; a technician can fine-tune them in `pompe.h` (see
  [README.md](README.md)) and must re-run the test suite afterwards.
