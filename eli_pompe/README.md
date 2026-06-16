# eli_pompe — developer documentation

Duplex sump-pump lift-station controller for a **CONTROLLINO MAXI**. No network
(no Ethernet/MQTT) — it runs standalone. This document is for whoever maintains
the code. For wiring, calibration and day-to-day operation see
[MANUAL.md](MANUAL.md).

## What it does

- Two pumps share **one** discharge pipe, so **only one ever runs at a time** (hard interlock).
- The **lead pump alternates** every completed cycle, so both wear evenly and stay exercised.
- Float switches: **MIN** (stop / dry-run protection) and **1/2** (start) are wired today; **3/4** (high-water alarm) is pre-wired for later.
- Two **YHDC SCT010T-D** split-core current sensors (0-10 A → 0-10 V) wire straight into standard analog inputs (no burden resistors) to tell whether each pump is actually pumping.
- A stuck/failed pump is detected and the controller **switches to the other pump**, retries the bad one after a cooldown, and **locks it out** after repeated failures.
- A two-tier local alarm (beacon + siren) signals problems; no network needed.
- A **PRE-EMPTY button** drains the tank to MIN on demand (storm prep), even below the 1/2 float; a dedicated lamp flashes to confirm.

## Files

| File | Purpose |
|------|---------|
| [`eli_pompe.ino`](eli_pompe.ino) | Sketch entry point: pin map, `setup()`/`loop()`, watchdog. Thin. |
| [`pompe.h`](pompe.h) | `PompeManager` — all the control logic and tunables. The real code. |
| [`tests/`](tests/) | Native host unit tests (run on your PC, not the board). See [tests/README.md](tests/README.md). |
| [`MANUAL.md`](MANUAL.md) | Installer/operator manual: wiring, calibration, operation, troubleshooting. |
| [`PANIC_CARD.md`](PANIC_CARD.md) | One-page wall card: what the lights/siren mean and who to call. |
| [`RECOVERY.md`](RECOVERY.md) | Step-by-step reflash of a replacement Controllino (for an Arduino novice). |
| [`BOM.md`](BOM.md) | Parts list + the hardware↔firmware settings that must match. |
| [`CLAUDE.md`](CLAUDE.md) | Orientation + decision log + invariants — read first when picking this up cold. |

The four user-facing docs have Italian courtesy copies — [`PANIC_CARD.it.md`](PANIC_CARD.it.md),
[`MANUAL.it.md`](MANUAL.it.md), [`BOM.it.md`](BOM.it.md), [`RECOVERY.it.md`](RECOVERY.it.md).
**English is authoritative**; edit the English first, the `.it.md` copies may lag.

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
1b. **PRE-EMPTY** button → latch a drain-to-MIN request if water is above MIN; always start a 5 s lamp-ack window.
2. **RESET** button → clear all faults, lockouts, cooldowns, the silence latch, and any pre-empty request.
3. **Cooldown expiry** → re-enable a temporarily-faulted pump (locked-out pumps stay down).
4. **OFF** (MOA) → make sure that pump isn't running.
5. **MANUAL** (MOA) → force that pump on (interlock still enforced; if both are in manual, pump 1 wins). Auto/fault logic is bypassed.
6. **AUTO** control:
   - decide when to **stop** (see "Stop logic");
   - if running and past the startup grace, run the **fault checks**;
   - if idle and the 1/2 float is up **or a pre-empty is requested**, **start** the lead pump (or the other if the lead is unavailable).
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

Note: the YHDC 0-10 V sensor has no "live zero", so a **disconnected sensor reads
~0 A** and is caught by the under-current fault (the pump faults either way) — but
broken-sensor and not-actually-pumping can't be told apart, and there's no
separate broken-wire detection (unlike a 4-20 mA loop).

On a fault the pump stops, the other takes over within the dead time, and the
faulted pump enters a **cooldown** (`FAULT_COOLDOWN_MS`). When the cooldown ends
it auto-retries. After **`MAX_CONSECUTIVE_FAULTS`** faults *without a good cycle
in between* the pump **locks out permanently** (requires a manual RESET). A
completed normal cycle resets that counter.

### Alarm tiers

| Tier | Drives | Triggers |
|------|--------|----------|
| **Warning** | big red **beacon** only | any pump faulted / locked out / unmonitored, or a MIN-float fault |
| **Emergency** | beacon **+ siren** | 3/4 high-water float, a 1/2-float fault (which only happens at high water), or **both** pumps unavailable while water is demanding |

The **Silence** button mutes the **siren only**; the beacon stays on until the
condition clears. A **single** pump fault is a warning, not an emergency, because
the other pump still covers demand. (There's no separate panel ALARM lamp — the
beacon relay's own on-board LED is the alarm-state indicator.)

Indicator nuance: a pump's fault lamp is **steady** while it's in a temporary
fault/cooldown and **blinks** when it's permanently locked out (needs a reset).

### Float-consistency / sensor sanity

A higher float can't be wet while a lower one reads dry. The controller uses
this to spot broken floats: `3/4 && !1/2` → the 1/2 float is broken (emergency,
since water is already high); `1/2 && !MIN` → the MIN float is broken (warning +
timer mode). The level lamp of the suspect float **blinks**.

## I/O — pin map (CONTROLLINO MAXI)

Defined in [`eli_pompe.ino`](eli_pompe.ino). Most indication is the controller's
own per-channel LEDs — only the beacon and siren are external devices.

| Function | Pin | Type |
|----------|-----|------|
| Pump 1 / 2 relay (drives Finder coil; **relay LED = RUN**) | `R0` / `R1` | relay |
| Alarm beacon (remote; **relay LED = alarm state**) | `R2` | relay |
| Siren | `R3` | relay |
| Level MIN / 1-2 / 3-4 lamp (blink = float broken) | `D0` / `D1` / `D2` | 24 V out |
| Pump 1 / 2 FAULT lamp (steady=fault, blink=lockout) | `D3` / `D4` | 24 V out |
| PRE-EMPTY active lamp (flashes) | `D5` | 24 V out |
| Pump 1 / 2 current sensor (YHDC SCT010T-D) | `A0` / `A1` | analog in (0-10 V; no burden resistor) |
| Float MIN / 1-2 / 3-4 | `A2` / `A3` / `A4` | digital in |
| Silence / Reset / PRE-EMPTY button | `A5` / `A6` / `A7` | digital in |
| Pump 1 MANUAL / AUTO | `A8` / `A9` | digital in |
| Pump 2 MANUAL / AUTO | `IN0` / `IN1` | digital in |

RUN and water levels are read off the pump-relay / float-input LEDs (no dedicated
lamps). All 12 inputs (A0–A9 + IN0/IN1) are used — no spare inputs.

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
| `PREEMPTY_ACK_MS` | 5000 | pre-empty lamp blink window after a button press |
| `AMP_SPAN_A` | 10.0 | full-scale amps at 20 mA |
| `NORMAL_AMP_MIN` / `MAX` | 2.0 / 8.0 | healthy running band (A) |
| `ADC_AT_0A` / `ADC_AT_FS` | 0 / 426 | raw counts at 0 A (0 V) and full scale (10 V) — **calibrate** (see MANUAL) |
| `FLOAT_ACTIVE_HIGH` | true | set false for normally-closed floats |

Current scaling: `amps = AMP_SPAN_A * (adc - ADC_AT_0A) / (ADC_AT_FS - ADC_AT_0A)`.

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
