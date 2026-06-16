# eli_pompe — context for future sessions

Orientation + decision log so a fresh session (human or AI) doesn't re-derive
everything. The detailed docs are the source of truth; this is the map and the
"why". If something here conflicts with the code, the code wins — fix this file.

## What this is
Standalone **sump-pump lift-station controller** on a **CONTROLLINO MAXI**
(ATmega2560), **no network**. Two pumps share one discharge pipe → only one runs
at a time; they alternate; each pump's current is read via a **YHDC SCT010T-D**
(0–10 V) clamp to detect a stuck/failed pump; faults switch pumps, auto-retry,
then lock out; a local beacon/siren alarms. That's the whole job.

## Start here — don't re-derive
- [`pompe.h`](pompe.h) — `PompeManager`, **all** the logic + tunables. The real code.
- [`eli_pompe.ino`](eli_pompe.ino) — pin map + `setup()`/`loop()` (thin).
- [`README.md`](README.md) — architecture, `check()` flow, fault model, tunables table.
- [`MANUAL.md`](MANUAL.md) — install/wire/calibrate/operate. [`BOM.md`](BOM.md) — parts + spares. [`RECOVERY.md`](RECOVERY.md) — reflash. [`PANIC_CARD.md`](PANIC_CARD.md) — wall card.
- [`tests/`](tests/) — native host unit tests. **Verify with `cd tests && make`** (expect `78 checks, 0 failures`).
- **Italian copies:** the four user-facing docs have `*.it.md` courtesy translations (PANIC_CARD/MANUAL/BOM/RECOVERY). **English is authoritative** — when you change a user-facing doc, edit the English `.md` first; the `.it.md` may lag (re-translate it after, see how it was done in chat history / banner says "non autoritativa"). README.md and this CLAUDE.md stay English-only.

## Repo context
Multi-controller repo (`quadro/`, `salotto/`, `garage/`, …), each a sketch.
Shared [`../libs/`](../libs/) (`OutputPin`, `ButtonManager`, `controllino_common.h`)
is **coupled to ArduinoHA/Ethernet** — `eli_pompe` deliberately does **not** use
it (it's networkless and safety-ish). It mirrors the per-feature-class-header
pattern instead (cf. [`../garage/basculante.h`](../garage/basculante.h)).

## Decisions & rationale (the expensive-to-rediscover bits)
- **Floats:** MIN (stop / dry-run) + 1/2 (start) wired now; 3/4 (high-water alarm) pre-wired, input read but may be unconnected (reads "dry"). Active-high NO contacts (`FLOAT_ACTIVE_HIGH`).
- **Stuck detection:** amps out of band after `STARTUP_GRACE_MS`, **OR** the 1/2 float not clearing within `MAX_CLEAR_HALF_MS`. The effectiveness timer is measured against the **1/2 float clearing**, not full drain to MIN (the owner's explicit choice).
- **Broken-float consistency:** a higher float wet while a lower reads dry is impossible. `1/2 && !MIN` → MIN float broken → **timer mode** (run `DRAIN_TIMER_MS` past the 1/2 opening since MIN can't be trusted) → **warning**. `3/4 && !1/2` → 1/2 float broken → **emergency** (water's already high).
- **Recovery:** fault → switch to other pump → **cooldown** (`FAULT_COOLDOWN_MS`) auto-retry → **permanent lockout** after `MAX_CONSECUTIVE_FAULTS` (manual RESET clears it). A completed good cycle resets the consecutive-fault counter.
- **Two alarm tiers:** **beacon only = warning** (any single pump faulted/locked/unmonitored, or a MIN-float fault); **beacon + siren = emergency** (3/4 high water, 1/2-float fault, or both pumps unavailable while demanding). A **single lockout is beacon-only, no siren** (owner confirmed). **SILENCE** mutes the siren only; beacon stays. No separate panel ALARM lamp — the beacon relay's own LED is the alarm-state indicator. Fault lamp **steady** = temporary, **blinking** = locked out.
- **Per-pump `MIN_OFF_TIME`** (not global). This fixed a real bug: a global anti-short-cycle delayed a *fault hand-off* to the backup pump by 15 s. Now the backup takes over within `SWITCH_DEADTIME_MS`. `t_undercurrent_fault_fast_handoff` guards it — don't reintroduce a global delay.
- **Pre-empty (storm pre-drain):** a momentary button on `A7` latches a `preEmpty` flag that's a **second demand source** (`fHalf || preEmpty`) — modelled explicitly, **not** as a faked 1/2 float (a fake float would light the 1/2 lamp and trip the float-consistency logic). Drains to MIN reusing all the normal run/stop/fault/alternation machinery; never runs dry (only engages while MIN wet, stops at MIN). Cleared at MIN and by **RESET** (abort — a drain in progress still finishes to MIN). Confirmation lamp on `D5` flashes while engaged **and** for `PREEMPTY_ACK_MS` (5 s) after *any* press, so even an ignored press (empty tank) is acknowledged.
- **Indication / output budget:** only **beacon + siren** are wired externally. Lamps (24 V outputs): level MIN/1-2/3-4 (`D0–D2`), per-pump FAULT (`D3`/`D4`), PRE-EMPTY (`D5`) = 6 of the MAXI's 12 digital outputs. **RUN** is read off the pump relay LEDs and **levels** off the float input LEDs (no dedicated lamps) — and a broken float is directly visible as an inconsistency between input LEDs. Inputs: clamps on `A0`/`A1`, floats `A2–A4`, buttons `A5`/`A6`/`A7`, MOA `A8`/`A9`/`IN0`/`IN1` — all 12 inputs used, no spares.
- **Terminology:** selector is **MANUAL / Off / Auto (MOA)**, not HAND/HOA (owner preference). Code uses `MODE_MANUAL`. "Hand-off" in fault context = handing pumping duty to the other pump (different concept; fine).
- **Switching device:** socket-mounted **Finder relays** on `R0`/`R1` (plug-swap, not contactors). **Hardware interlock is optional** — software guarantees single-pump operation and both-on only wastes flow on the shared pipe (no damage), per the owner. Don't re-flag it "mandatory".
- **Board / analog:** **CONTROLLINO MAXI** + **YHDC SCT010T-D** voltage-output current sensors (split-core, 10 A → 10 V, ±2%) on `A0`/`A1`. The 0–10 V output goes straight into the MAXI's standard analog inputs (0–24 V range), **no burden resistor**. `analogRead` 0–1023 over 0–24 V → 0 V≈0, 10 V≈426 counts. `ADC_AT_0A`/`ADC_AT_FS` are **placeholders** → calibrate on site and **commit**; `AMP_SPAN_A` = the sensor's full-scale amps (10 for the SCT010T-D). **History:** briefly considered MAXI Automation for native 0–20 mA inputs (to use a 4–20 mA Seneca T201 without a resistor), but its current-INPUT capability was unconfirmed — a 0–10 V sensor on the standard MAXI is simpler and the board matches the owner's existing units. **Trade-off of voltage vs 4–20 mA:** no live zero, so a disconnected sensor reads 0 A → pump faults on under-current (no separate broken-wire detection; `sensorFault` was removed).
- **Resilience:** owner's strategy is "buy two of everything" cold spares; a **pre-flashed** spare Controllino + labelled wires makes a swap skill-free (see [BOM.md](BOM.md) "Spares").

## Invariants — do not break
1. **Single-pump interlock:** at most one `pumpRelay` energised, ever. It holds *by construction* (outputs driven purely from `activePump`). Every test asserts it on every simulated step. Never regress this.
2. **Run the tests after any `pompe.h` change** (`cd tests && make`) and keep them green. Add a test for any new behaviour.
3. **No external libraries** — keep the sketch self-contained (only the CONTROLLINO board package + `Controllino.h`). Don't pull in Ethernet/MQTT.
4. **Don't rename `tests/` to `src/`** — the Arduino build would then compile the stub `tests/stubs/Arduino.h` and break everything. Non-`src` subfolders are ignored by the Arduino build (that's why tests live there).
5. Anything flagged 🔧 in [BOM.md](BOM.md) must match its `pompe.h` constant (sensor full-scale = `AMP_SPAN_A`, etc.).

## Status / open items
- **Not yet compiled in the Arduino IDE** here (no `arduino-cli` in this environment) — the owner verifies the build before flashing.
- **Calibration** (`ADC_AT_0A` / `ADC_AT_FS`) still placeholders — measure on site, commit.
- Fill-in blanks remain: PANIC_CARD phone numbers, BOM part numbers, GitHub URL.

## Future direction — Home Assistant / MQTT (someday, not now)
The owner wants to network this eventually to monitor it in Home Assistant. The
MAXI has the Ethernet hardware and the other controllers already do this
(ArduinoHA + Ethernet, see [`../libs/controllino_common.h`](../libs/controllino_common.h)
and e.g. [`../salotto/salotto.ino`](../salotto/salotto.ino)). Keep it **additive
and report-only** so the safety logic stays network-free and testable:
- **Do NOT touch `PompeManager`'s control flow.** Add a thin reporting layer in
  the `.ino` (or a separate header) that *reads* state and publishes it.
- Add a few `const` getters to `PompeManager` (currently most state is private):
  `activePump`, per-pump `faulted`/`lockedOut`/amps, the three float levels,
  `alarmActive`, `leadPump`. That's the only change to `pompe.h` needed.
- Expose as HA entities: per-pump RUN + FAULT/LOCKED (binary sensors), 3 level
  binary sensors, ALARM binary sensor, per-pump current `HASensorNumber` (A),
  optional lead-pump sensor. Optionally remote **SILENCE/RESET** as `HAButton`s.
- **Never let a network command bypass the single-pump interlock or fault
  logic** — remote commands are conveniences, the local logic stays authoritative.
- Reuse the EEPROM JSON network-config pattern from `controllino_common.h`.
- Adding this pulls in Ethernet/ArduinoHA — so guard it (e.g. a `#define
  ENABLE_NETWORK`) and keep the host tests building without those libs.

## Related memory
User auto-memory has: `controllino-maxi-analog-range` (0–24 V inputs) and
`cold-spare-resilience-preference` ("buy two", pre-flashed spare).
