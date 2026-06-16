# eli_pompe — context for future sessions

Orientation + decision log so a fresh session (human or AI) doesn't re-derive
everything. The detailed docs are the source of truth; this is the map and the
"why". If something here conflicts with the code, the code wins — fix this file.

## What this is
Standalone **sump-pump lift-station controller** on a **CONTROLLINO MAXI**
(ATmega2560), **no network**. Two pumps share one discharge pipe → only one runs
at a time; they alternate; each pump's current is read via a **Seneca T201**
(4–20 mA) clamp to detect a stuck/failed pump; faults switch pumps, auto-retry,
then lock out; a local beacon/siren alarms. That's the whole job.

## Start here — don't re-derive
- [`pompe.h`](pompe.h) — `PompeManager`, **all** the logic + tunables. The real code.
- [`eli_pompe.ino`](eli_pompe.ino) — pin map + `setup()`/`loop()` (thin).
- [`README.md`](README.md) — architecture, `check()` flow, fault model, tunables table.
- [`MANUAL.md`](MANUAL.md) — install/wire/calibrate/operate. [`BOM.md`](BOM.md) — parts + spares. [`RECOVERY.md`](RECOVERY.md) — reflash. [`PANIC_CARD.md`](PANIC_CARD.md) — wall card.
- [`tests/`](tests/) — native host unit tests. **Verify with `cd tests && make`** (expect `64 checks, 0 failures`).

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
- **Two alarm tiers:** **beacon only = warning** (any single pump faulted/locked/unmonitored, or a MIN-float fault); **beacon + siren + blinking panel lamp = emergency** (3/4 high water, 1/2-float fault, or both pumps unavailable while demanding). A **single lockout is beacon-only, no siren** (owner confirmed). **SILENCE** mutes the siren only; beacon/lamp stay. Fault lamp **steady** = temporary, **blinking** = locked out.
- **Per-pump `MIN_OFF_TIME`** (not global). This fixed a real bug: a global anti-short-cycle delayed a *fault hand-off* to the backup pump by 15 s. Now the backup takes over within `SWITCH_DEADTIME_MS`. `t_undercurrent_fault_fast_handoff` guards it — don't reintroduce a global delay.
- **Terminology:** selector is **MANUAL / Off / Auto (MOA)**, not HAND/HOA (owner preference). Code uses `MODE_MANUAL`. "Hand-off" in fault context = handing pumping duty to the other pump (different concept; fine).
- **Switching device:** socket-mounted **Finder relays** on `R0`/`R1` (plug-swap, not contactors). **Hardware interlock is optional** — software guarantees single-pump operation and both-on only wastes flow on the shared pipe (no damage), per the owner. Don't re-flag it "mandatory".
- **Analog range:** CONTROLLINO MAXI inputs read **0–24 V full scale** (~42.6 counts/V), **not 0–5 V**. T201 4–20 mA via ~**500 Ω** burden → ~2–10 V. `ADC_AT_4MA`/`ADC_AT_20MA` are **placeholders** → calibrate on site and **commit** the measured values.
- **Resilience:** owner's strategy is "buy two of everything" cold spares; a **pre-flashed** spare Controllino + labelled wires makes a swap skill-free (see [BOM.md](BOM.md) "Spares").

## Invariants — do not break
1. **Single-pump interlock:** at most one `pumpRelay` energised, ever. It holds *by construction* (outputs driven purely from `activePump`). Every test asserts it on every simulated step. Never regress this.
2. **Run the tests after any `pompe.h` change** (`cd tests && make`) and keep them green. Add a test for any new behaviour.
3. **No external libraries** — keep the sketch self-contained (only the CONTROLLINO board package + `Controllino.h`). Don't pull in Ethernet/MQTT.
4. **Don't rename `tests/` to `src/`** — the Arduino build would then compile the stub `tests/stubs/Arduino.h` and break everything. Non-`src` subfolders are ignored by the Arduino build (that's why tests live there).
5. Anything flagged 🔧 in [BOM.md](BOM.md) must match its `pompe.h` constant (T201 range = `AMP_SPAN_A`, etc.).

## Status / open items
- **Not yet compiled in the Arduino IDE** here (no `arduino-cli` in this environment) — the owner verifies the build before flashing.
- **Calibration** (`ADC_AT_4MA/20MA`) still placeholders — measure on site, commit.
- Fill-in blanks remain: PANIC_CARD phone numbers, BOM part numbers, GitHub URL.

## Related memory
User auto-memory has: `controllino-maxi-analog-range` (0–24 V inputs) and
`cold-spare-resilience-preference` ("buy two", pre-flashed spare).
