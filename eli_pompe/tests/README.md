# eli_pompe — unit tests

Native (host) tests for the control logic in [`../pompe.h`](../pompe.h). They run
on your computer, **not** on the Controllino: the Arduino core is faked
([`stubs/Arduino.h`](stubs/Arduino.h)) so the float switches, amp clamps, buttons
and the `millis()` clock are just variables the tests set, and the relay/lamp
outputs are variables they read back. Whole scenarios play out in *simulated*
time, so a 10-minute cooldown takes microseconds.

## Run

```sh
cd eli_pompe/tests
make          # builds and runs
```

Expected tail:

```
64 checks, 0 failures
ALL TESTS PASSED
```

Exit code is non-zero if anything fails, so it drops straight into CI.

## What's covered

Every test also asserts the **hard interlock** invariant on every simulated
step: the two pump-contactor relays are *never* energised at the same time.

- normal fill→pump→drain→stop cycle, and lead-pump **alternation**
- **undercurrent** / **overcurrent** / **ineffective (1/2 not clearing)** faults → switch to the other pump
- **startup grace** (a low reading right after start does not fault)
- **fast hand-off**: a fault switches to the backup within the ~1 s dead time, not after the 15 s anti-short-cycle delay
- **anti-short-cycle** keeps the *same* pump off for `MIN_OFF_TIME` after it stops
- **auto-retry** after a cooldown, and **permanent lockout** after `MAX_CONSECUTIVE_FAULTS`, cleared by **RESET**
- **MIN-float fault** → drain-timer mode (beacon, no siren)
- **1/2-float fault at high water** → emergency (siren + beacon)
- **high-water override** runs a pump even during a cooldown
- **silence** mutes the siren only; the beacon stays on
- **Hand-Off-Auto**: manual run, both-hand interlock, OFF disables
- level lamps mirror the floats

## Important: this folder is invisible to the Arduino build

The Arduino IDE / `arduino-cli` only compiles the sketch root and an optional
`src/` subfolder. This `tests/` folder (and its stub `Arduino.h`) is ignored, so
it cannot interfere with building/flashing the real sketch. Do **not** rename it
to `src`.

## Adding a test

Write a `static void t_something()`, drive inputs with the helpers
(`setFloats`, `setCurrent`, `setMode`, `press`, `fillToHalf`, …), advance time
with `run(m, ms)`, assert with `EXPECT(cond, "msg")`, then add `RUN(t_something);`
to `main()`.
