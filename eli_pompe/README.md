# eli_pompe — developer documentation

Duplex sump-pump lift-station controller for a **CONTROLLINO MAXI**. No network
(no Ethernet/MQTT) — it runs standalone. This document is for whoever maintains
the code. For wiring, calibration and day-to-day operation see
[MANUAL.md](MANUAL.md).

## What it does

- Two pumps share **one** discharge pipe, so **only one ever runs at a time** (hard interlock).
- The **lead pump alternates** every completed cycle, so both wear evenly and stay exercised.
- Float switches: **MIN** (stop / dry-run protection) and **1/2** (start) are wired today; **3/4** (high-water alarm) is pre-wired for later.
- Two **Seneca T201** clamp transducers (4-20 mA, 0-10 A) measure each pump's current to tell whether it's actually pumping.
- A stuck/failed pump is detected and the controller **switches to the other pump**, retries the bad one after a cooldown, and **locks it out** after repeated failures.
- A two-tier local alarm (beacon + siren) signals problems; no network needed.

## Files

| File | Purpose |
|------|---------|
| [`eli_pompe.ino`](eli_pompe.ino) | Sketch entry point: pin map, `setup()`/`loop()`, watchdog. Thin. |
| [`pompe.h`](pompe.h) | `PompeManager` — all the control logic and tunables. The real code. |
| [`tests/`](tests/) | Native host unit tests (run on your PC, not the board). See [tests/README.md](tests/README.md). |

The sketch deliberately does **not** reuse the repo's shared `libs/`
(`OutputPin`, `ButtonManager`, `controllino_common.h`): those are coupled to
ArduinoHA/Ethernet, which a non-networked safety controller shouldn't drag in.
It mirrors the repo's conventions instead (per-feature class header with a
`check()` method like [`../garage/basculante.h`](../garage/basculante.h),
`millis()` timers, watchdog).

## Architecture

`loop()` is just:

```cpp
void loop() { pompe.check(); wdt_reset(); }
```

All state lives in one `PompeManager` instance. `check()` runs start-to-finish
every loop (a few hundred µs), never blocks, and uses `millis()` for all timing.
A small `DebInput` helper debounces each digital input (floats use a long 2 s
window to kill ripple; buttons/MOA use 50 ms and expose a rising edge).

### `check()` flow

1. **Read & debounce** floats, MOA selectors, buttons, and the per-pump current.
2. **RESET** button → clear all faults, lockouts, cooldowns and the silence latch.
3. **Cooldown expiry** → re-enable a temporarily-faulted pump (locked-out pumps stay down).
4. **OFF** (MOA) → make sure that pump isn't running.
5. **MANUAL** (MOA) → force that pump on (interlock still enforced; if both are in manual, pump 1 wins). Auto/fault logic is bypassed.
6. **AUTO** control:
   - decide when to **stop** (see "Stop logic");
   - if running and past the startup grace, run the **fault checks**;
   - if idle and the 1/2 float is up, **start** the lead pump (or the other if the lead is unavailable).
7. **Alarm evaluation** (emergency tier) + high-water emergency override.
8. **Drive outputs** — pump relays are written purely from `activePump`, so "both off or exactly one on" holds *by construction*.
9. Periodic **status** line on Serial (115200) for field debugging.

### Stop logic (step 6)

- **Healthy:** stop when the **MIN** float clears, then flip the lead pump.
- **MIN-float fault** (`1/2` wet while `MIN` reads dry — physically impossible): latch **timer mode** for the run; can't trust MIN, so once the 1/2 float opens, run a further `DRAIN_TIMER_MS` and stop.
- **Safety backstop:** any single run is capped at `MAX_RUN_MS` (catches e.g. a MIN float stuck *high*).

### Fault model

A running pump is faulted when, after `STARTUP_GRACE_MS`:

| Reason | Condition | Meaning |
|--------|-----------|---------|
| `F_UNDERCURRENT` | amps `< NORMAL_AMP_MIN` | not running / dry / breaker tripped |
| `F_OVERCURRENT`  | amps `> NORMAL_AMP_MAX` | jammed / locked rotor |
| `F_INEFFECTIVE`  | 1/2 float still up after `MAX_CLEAR_HALF_MS` | clogged impeller / stuck check valve |

Plus a non-faulting **sensor fault**: a reading below the 4 mA point
(`< SENSOR_FAULT_AMPS`) means a broken current loop — it's flagged (fault lamp +
log) and the controller falls back to the level/timer logic for that pump.

On a fault the pump stops, the other takes over within the dead time, and the
faulted pump enters a **cooldown** (`FAULT_COOLDOWN_MS`). When the cooldown ends
it auto-retries. After **`MAX_CONSECUTIVE_FAULTS`** faults *without a good cycle
in between* the pump **locks out permanently** (requires a manual RESET). A
completed normal cycle resets that counter.

### Alarm tiers

| Tier | Drives | Triggers |
|------|--------|----------|
| **Warning** | big red **beacon** only | any pump faulted / locked out / unmonitored, or a MIN-float fault |
| **Emergency** | beacon **+ siren +** blinking panel lamp | 3/4 high-water float, a 1/2-float fault (which only happens at high water), or **both** pumps unavailable while water is demanding |

The **Silence** button mutes the **siren only**; the beacon and panel lamp stay
on until the condition clears. A **single** pump fault is a warning, not an
emergency, because the other pump still covers demand.

Indicator nuance: a pump's fault lamp is **steady** while it's in a temporary
fault/cooldown and **blinks** when it's permanently locked out (needs a reset).

### Float-consistency / sensor sanity

A higher float can't be wet while a lower one reads dry. The controller uses
this to spot broken floats: `3/4 && !1/2` → the 1/2 float is broken (emergency,
since water is already high); `1/2 && !MIN` → the MIN float is broken (warning +
timer mode). The level lamp of the suspect float **blinks**.

## I/O — pin map (CONTROLLINO MAXI)

Defined in [`eli_pompe.ino`](eli_pompe.ino).

| Function | Pin | Type |
|----------|-----|------|
| Pump 1 contactor | `R0` | relay |
| Pump 2 contactor | `R1` | relay |
| Alarm beacon (remote) | `R2` | relay |
| Siren | `R3` | relay |
| Pump 1 / 2 RUN lamp | `D0` / `D1` | 24 V out |
| Pump 1 / 2 FAULT lamp | `D2` / `D3` | 24 V out |
| Level MIN / 1-2 / 3-4 lamp | `D4` / `D5` / `D6` | 24 V out |
| Panel ALARM lamp (blinks) | `D7` | 24 V out |
| Pump 1 / 2 amp clamp (T201) | `A0` / `A1` | analog in (4-20 mA via burden R) |
| Float MIN / 1-2 / 3-4 | `A2` / `A3` / `A4` | digital in |
| Silence / Reset button | `A5` / `A6` | digital in |
| Pump 1 MANUAL / AUTO | `A7` / `A8` | digital in |
| Pump 2 MANUAL / AUTO | `A9` / `IN0` | digital in |
| spare | `IN1` | — |

Floats/buttons are **active-high** (contact closes to +24 V). MOA is a maintained
3-position selector wired as two inputs per pump (Manual / Auto; centre Off = both open).

## Tunables

All at the top of [`pompe.h`](pompe.h). Times in ms.

| Constant | Default | Notes |
|----------|---------|-------|
| `FLOAT_DEBOUNCE_MS` | 2000 | float stability window |
| `BTN_DEBOUNCE_MS` | 50 | button/MOA debounce |
| `STARTUP_GRACE_MS` | 5000 | ignore current during inrush/priming |
| `MAX_CLEAR_HALF_MS` | 120000 | 1/2 float must clear within this, else ineffective fault |
| `DRAIN_TIMER_MS` | 30000 | MIN-fault timer-mode run past the 1/2 opening (≈ normal 1/2→MIN drain time) |
| `MAX_RUN_MS` | 900000 | absolute single-run cap (safety) |
| `MIN_OFF_TIME_MS` | 15000 | per-pump anti-short-cycle |
| `SWITCH_DEADTIME_MS` | 1000 | dead time between energisations |
| `FAULT_COOLDOWN_MS` | 600000 | auto-retry delay (10 min) |
| `MAX_CONSECUTIVE_FAULTS` | 3 | faults before permanent lockout |
| `ALARM_BLINK_MS` | 500 | blink half-period |
| `AMP_SPAN_A` | 10.0 | full-scale amps at 20 mA |
| `NORMAL_AMP_MIN` / `MAX` | 2.0 / 8.0 | healthy running band (A) |
| `SENSOR_FAULT_AMPS` | -1.0 | below this ⇒ broken loop |
| `ADC_AT_4MA` / `ADC_AT_20MA` | 85 / 426 | raw counts at 4/20 mA — **calibrate** (MAXI inputs are 0-24 V, ~42.6 counts/V; see MANUAL) |
| `FLOAT_ACTIVE_HIGH` | true | set false for normally-closed floats |

Current scaling: `amps = AMP_SPAN_A * (adc - ADC_AT_4MA) / (ADC_AT_20MA - ADC_AT_4MA)`.

## Build & flash

Arduino IDE (or `arduino-cli`), board **CONTROLLINO MAXI**, same toolchain as the
other sketches in this repo. Open `eli_pompe.ino`, select the MAXI, upload over USB.

```sh
# arduino-cli equivalent
arduino-cli compile --fqbn CONTROLLINO_Boards:avr:controllino_maxi eli_pompe
arduino-cli upload  --fqbn CONTROLLINO_Boards:avr:controllino_maxi -p <port> eli_pompe
```

## Tests

The control logic has a native unit-test suite that runs on your PC (no board
needed):

```sh
cd tests && make
```

It fakes the Arduino core and plays scenarios in simulated time, asserting the
right relays/lamps fire — and that the two pump relays are **never** on together.
Run it after any change to `pompe.h`. See [tests/README.md](tests/README.md).

## Extending

- **Add the 3/4 float:** it's already read (`A4`) and wired into the alarm/override logic; just connect the float. Until then the input reads low (no false alarm).
- **Change fault sensitivity:** tune `NORMAL_AMP_MIN/MAX`, `STARTUP_GRACE_MS`, `MAX_CLEAR_HALF_MS`.
- **Make lockout sound the siren**, or the MIN-fault stay lamp-only, etc.: the warning/emergency split is in step 7 / step 8 of `check()`. Add a test for whatever you change.
