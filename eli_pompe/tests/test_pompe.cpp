// Native (host) unit tests for the eli_pompe sump-station logic in ../pompe.h.
//
// These run on your computer, NOT on the Controllino: the Arduino core is faked
// (tests/stubs/Arduino.h) so the float switches, amp clamps, buttons and the
// millis() clock are just variables the test pokes, and the relay/lamp outputs
// are variables it reads back. This lets us drive whole scenarios in simulated
// time and assert the controller drives the right pins.
//
// Build & run:   make            (from this directory)
//          or:   c++ -std=c++17 -I stubs -I .. test_pompe.cpp -o test_pompe && ./test_pompe
#include <Arduino.h> // the stub in tests/stubs
#include "pompe.h"
#include <cstdio>
#include <cmath>

// ----------------------------------------------------------- virtual pin map
// Arbitrary distinct pin numbers into the stub's g_din/g_dout/g_ain arrays.
static const PompePins TP = {
    /* pumpRelay  */ {20, 21},
    /* beaconRelay*/ 22,
    /* sirenRelay */ 23,
    /* faultLamp  */ {26, 27},
    /* lampMin    */ 28,
    /* lampHalf   */ 29,
    /* lampHigh   */ 30,
    /* lampPreEmpty*/ 32,
    /* curPin     */ {0, 1},
    /* floatMin   */ 2,
    /* floatHalf  */ 3,
    /* floatHigh  */ 4,
    /* silence    */ 5,
    /* reset      */ 6,
    /* preEmptyBtn*/ 11,
    /* manual     */ {7, 9},
    /* autom      */ {8, 10}};

enum { OFF = 0, MANUAL = 1, AUTO = 2 };

// ----------------------------------------------------------- tiny test framework
static int g_failures = 0;
static int g_checks = 0;
static const char *g_curtest = "";
static bool g_interlockOk = true;

#define EXPECT(cond, msg)                                                  \
  do                                                                       \
  {                                                                        \
    g_checks++;                                                            \
    if (!(cond))                                                           \
    {                                                                      \
      g_failures++;                                                        \
      printf("  FAIL [%s]: %s  (line %d)\n", g_curtest, msg, __LINE__);    \
    }                                                                      \
  } while (0)

// --------------------------------------------------------------- helpers
static void resetAll()
{
  g_millis = 0;
  g_interlockOk = true;
  for (int i = 0; i < 64; i++)
  {
    g_din[i] = 0;
    g_dout[i] = 0;
    g_ain[i] = 0;
    g_pinmode[i] = 0;
  }
}

static int active() // which pump relay is energised, or -1
{
  if (g_dout[TP.pumpRelay[0]])
    return 0;
  if (g_dout[TP.pumpRelay[1]])
    return 1;
  return -1;
}

// Advance simulated time, calling check() each step, and enforce the hard
// interlock invariant continuously: the two pump relays are never both on.
static void run(PompeManager &m, unsigned long ms, unsigned long step = 20)
{
  for (unsigned long t = 0; t < ms; t += step)
  {
    g_millis += step;
    m.check();
    if (g_dout[TP.pumpRelay[0]] && g_dout[TP.pumpRelay[1]])
      g_interlockOk = false;
  }
}

static void setFloats(int mn, int hf, int hi)
{
  g_din[TP.floatMin] = mn;
  g_din[TP.floatHalf] = hf;
  g_din[TP.floatHigh] = hi;
}

static void setMode(int p, int mode)
{
  g_din[TP.manual[p]] = (mode == MANUAL) ? 1 : 0;
  g_din[TP.autom[p]] = (mode == AUTO) ? 1 : 0;
}

static int ampsToAdc(float a)
{
  return (int)lround(ADC_AT_0A + a / AMP_SPAN_A * (ADC_AT_FS - ADC_AT_0A));
}
static void setCurrent(int p, float amps) { g_ain[TP.curPin[p]] = ampsToAdc(amps); }

static void press(PompeManager &m, uint8_t pin)
{
  g_din[pin] = 1;
  run(m, 140); // hold past the 50 ms button debounce → one rising edge
  g_din[pin] = 0;
  run(m, 140);
}

// Boot a fresh controller: both pumps in AUTO, healthy current, no water.
static void boot(PompeManager &m)
{
  resetAll();
  m.begin();
  setMode(0, AUTO);
  setMode(1, AUTO);
  setCurrent(0, 5.0f);
  setCurrent(1, 5.0f);
  run(m, 400); // settle MOA + clear power-on dead time
}

static void fillToHalf(PompeManager &m) { setFloats(1, 1, 0); run(m, 2600); }
static void fillToHigh(PompeManager &m) { setFloats(1, 1, 1); run(m, 2600); }
static void drainBelowMin(PompeManager &m) { setFloats(0, 0, 0); run(m, 2600); }

// ----------------------------------------------------------------- tests

static void t_basic_cycle_and_interlock()
{
  PompeManager m(TP);
  boot(m);
  fillToHalf(m);
  EXPECT(active() == 0, "lead pump (0) starts on the 1/2 float");
  EXPECT(g_dout[TP.sirenRelay] == 0 && g_dout[TP.beaconRelay] == 0, "no alarm during a normal run");
  drainBelowMin(m);
  EXPECT(active() == -1, "stops when drained below MIN");
}

static void t_alternation()
{
  PompeManager m(TP);
  boot(m);
  fillToHalf(m);
  EXPECT(active() == 0, "cycle 1 → pump 0");
  drainBelowMin(m);
  fillToHalf(m);
  EXPECT(active() == 1, "cycle 2 → pump 1 (alternation)");
}

static void t_undercurrent_fault_fast_handoff()
{
  PompeManager m(TP);
  boot(m);
  fillToHalf(m);
  EXPECT(active() == 0, "pump 0 running");
  setCurrent(0, 0.5f); // pump 0 drawing almost nothing → not actually pumping
  run(m, 6000);        // pass the startup grace, fault, manual off
  EXPECT(active() == 1, "undercurrent → switched to pump 1 within ~1s (NOT after the 15s min-off)");
  EXPECT(g_dout[TP.beaconRelay] == 1, "beacon (warning) on for the faulted pump");
  EXPECT(g_dout[TP.sirenRelay] == 0, "siren stays off — one pump fault is not an emergency");
}

static void t_overcurrent_fault()
{
  PompeManager m(TP);
  boot(m);
  fillToHalf(m);
  setCurrent(0, 9.5f); // jammed / locked rotor
  run(m, 6000);
  EXPECT(active() == 1, "overcurrent → switched to pump 1");
}

static void t_startup_grace()
{
  PompeManager m(TP);
  boot(m);
  setFloats(1, 1, 0);
  setCurrent(0, 0.5f); // low from the very start (inrush/priming window)
  run(m, 2600);
  EXPECT(active() == 0, "pump 0 not faulted while still within the startup grace");
  run(m, 7000); // past the grace, plus the fault manual-off dead time
  EXPECT(active() == 1, "faults once the grace has elapsed");
}

static void t_overwhelmed_warns_not_faults()
{
  // Healthy current, but the 1/2 float never clears (inflow >= outflow, e.g. a
  // storm). The working pump must NOT be faulted/stopped — only a warning.
  PompeManager m(TP);
  boot(m);
  setFloats(1, 1, 0); // MIN + 1/2 wet, stays that way
  setCurrent(0, 5.0f); // pump draws normal current → it IS pumping
  run(m, 2600);
  EXPECT(active() == 0, "pump 0 running");
  run(m, 125000, 500); // well beyond MAX_CLEAR_HALF, 1/2 still up, current normal
  EXPECT(active() == 0, "working pump fighting inflow is NOT faulted or stopped");
  EXPECT(g_dout[TP.beaconRelay] == 1, "beacon warns (level not dropping)");
  EXPECT(g_dout[TP.sirenRelay] == 0, "not an emergency — siren stays off");
}

static void t_start_rate_limit()
{
  // ≤20 starts/hour per pump = at least 180 s between a pump's OWN starts.
  PompeManager m(TP);
  boot(m);
  setMode(1, OFF); // isolate pump 0 so nothing else can run
  run(m, 200);
  fillToHalf(m);
  EXPECT(active() == 0, "pump 0 runs");
  drainBelowMin(m);
  EXPECT(active() == -1, "pump 0 stopped");
  setFloats(1, 1, 0); // water comes straight back
  run(m, 120000, 1000); // < 180 s since pump 0's start
  EXPECT(active() == -1, "pump 0 held off — not yet 180 s since its last start (rate limit)");
  run(m, 90000, 1000); // now well past 180 s since that start
  EXPECT(active() == 0, "pump 0 restarts once the start-rate interval has elapsed");
}

static void t_exercise_autodrain()
{
  // MIN wet but water never reaches 1/2; after 24 h idle the station auto-drains.
  PompeManager m(TP);
  boot(m);
  setFloats(1, 0, 0); // MIN wet, below the 1/2 float → no normal demand
  run(m, 50000, 5000);
  EXPECT(active() == -1, "no normal start below the 1/2 float");
  run(m, 86500000, 10000); // > 24 h with MIN wet and no pump having run
  EXPECT(active() == 0, "auto-drain exercises a pump after 24 h idle with MIN wet");
}

static void t_cooldown_autoretry()
{
  PompeManager m(TP);
  boot(m);
  setMode(1, OFF); // only pump 0
  run(m, 200);
  setFloats(1, 1, 0);
  setCurrent(0, 0.5f);
  run(m, 8000); // pump 0 faults
  EXPECT(active() == -1, "pump 0 faulted, no backup available");
  setCurrent(0, 5.0f);  // pretend the fault was transient
  run(m, 60000, 500);   // still within the 10-min cooldown
  EXPECT(active() == -1, "stays off during cooldown");
  run(m, 560000, 1000); // total elapsed now exceeds FAULT_COOLDOWN_MS
  EXPECT(active() == 0, "auto-retried after the cooldown");
}

static void t_lockout_after_repeated_faults_and_reset()
{
  PompeManager m(TP);
  boot(m);
  setMode(1, OFF); // only pump 0
  run(m, 200);
  setFloats(1, 1, 0);
  setCurrent(0, 0.5f);    // keeps faulting on undercurrent
  run(m, 8000);           // fault #1
  run(m, 620000, 1000);   // cooldown → retry → fault #2
  run(m, 620000, 1000);   // cooldown → retry → fault #3 → LOCKED OUT
  run(m, 620000, 1000);   // a further cooldown elapses...
  EXPECT(active() == -1, "locked out: no auto-retry after MAX_CONSECUTIVE_FAULTS");

  // the locked-out fault lamp should be blinking (not steady)
  bool blinked = false;
  int base = g_dout[TP.faultLamp[0]];
  for (int i = 0; i < 80; i++) // ~1.6s, longer than one blink period
  {
    g_millis += 20;
    m.check();
    if (g_dout[TP.faultLamp[0]] != base)
      blinked = true;
  }
  EXPECT(blinked, "locked-out pump's fault lamp blinks");

  setCurrent(0, 5.0f); // pump 'repaired'
  press(m, TP.reset);
  run(m, 4000);
  EXPECT(active() == 0, "manual RESET clears the lockout and the pump runs again");
}

static void t_min_float_fault_timer_mode()
{
  PompeManager m(TP);
  boot(m);
  setFloats(0, 1, 0); // 1/2 wet but MIN dry → MIN float broken
  run(m, 2600);
  EXPECT(active() == 0, "still starts on the 1/2 float");
  EXPECT(g_dout[TP.beaconRelay] == 1, "MIN-float fault lights the beacon (warning)");
  EXPECT(g_dout[TP.sirenRelay] == 0, "but not the siren");
  setFloats(0, 0, 0); // level drops below 1/2 (MIN still reads dry)
  run(m, 2600);
  EXPECT(active() == 0, "keeps running past the 1/2 opening (timer mode)");
  run(m, 8000);
  EXPECT(active() == 0, "still draining within DRAIN_TIMER_MS");
  run(m, 30000, 500);
  EXPECT(active() == -1, "stops after the drain timer expires");
}

static void t_half_float_fault_is_emergency()
{
  PompeManager m(TP);
  boot(m);
  setFloats(1, 0, 1); // 3/4 wet but 1/2 dry → 1/2 float broken + genuine high water
  run(m, 2600);
  EXPECT(g_dout[TP.sirenRelay] == 1, "high water + 1/2-float fault is an EMERGENCY → siren on");
  EXPECT(g_dout[TP.beaconRelay] == 1, "beacon on");
  EXPECT(active() != -1, "emergency override runs a pump");
}

static void t_high_water_override_ignores_cooldown()
{
  PompeManager m(TP);
  boot(m);
  setMode(1, OFF);
  run(m, 200);
  setFloats(1, 1, 0);
  setCurrent(0, 0.5f);
  run(m, 8000); // pump 0 faults → cooling down
  EXPECT(active() == -1, "pump 0 cooling down");
  setFloats(1, 1, 1); // water reaches 3/4
  run(m, 2600);
  EXPECT(active() == 0, "high-water emergency overrides the cooldown to run pump 0");
  EXPECT(g_dout[TP.sirenRelay] == 1, "siren on at 3/4");
}

static void t_silence_mutes_siren_only()
{
  PompeManager m(TP);
  boot(m);
  fillToHigh(m); // emergency
  EXPECT(g_dout[TP.sirenRelay] == 1, "siren on");
  press(m, TP.silence);
  run(m, 200);
  EXPECT(g_dout[TP.sirenRelay] == 0, "silence mutes the siren");
  EXPECT(g_dout[TP.beaconRelay] == 1, "beacon stays on after silence");
}

static void t_hoa_manual_and_interlock()
{
  PompeManager m(TP);
  boot(m);
  setMode(0, MANUAL);
  run(m, 1600); // clear dead time
  EXPECT(active() == 0, "MANUAL runs pump 0 with no water demand");
  setMode(1, MANUAL); // both in manual
  run(m, 1600);
  EXPECT(active() == 0, "both MANUAL → only pump 0 runs (interlock)");
  setMode(0, OFF); // pump 0 off, pump 1 still in manual
  run(m, 2500);
  EXPECT(active() == 1, "pump 1 runs in manual once pump 0 is off");
}

static void t_off_disables()
{
  PompeManager m(TP);
  boot(m);
  fillToHalf(m);
  EXPECT(active() == 0, "pump 0 running in auto");
  setMode(0, OFF);
  setMode(1, OFF);
  run(m, 2500);
  EXPECT(active() == -1, "both OFF → no pump runs even with water present");
}

static void t_level_lamps_mirror_floats()
{
  PompeManager m(TP);
  boot(m);
  setFloats(1, 1, 0);
  run(m, 2600);
  EXPECT(g_dout[TP.lampMin] == 1, "MIN lamp on");
  EXPECT(g_dout[TP.lampHalf] == 1, "1/2 lamp on");
  EXPECT(g_dout[TP.lampHigh] == 0, "3/4 lamp off");
}

// sample the pre-empty lamp over `ms` and report whether it ever toggled
static bool preEmptyLampFlashed(PompeManager &m, unsigned long ms)
{
  bool flashed = false;
  int base = g_dout[TP.lampPreEmpty];
  for (unsigned long t = 0; t < ms; t += 20)
  {
    g_millis += 20;
    m.check();
    if (g_dout[TP.lampPreEmpty] != base)
      flashed = true;
  }
  return flashed;
}

static void t_preempty_drains_to_min()
{
  PompeManager m(TP);
  boot(m);
  setFloats(1, 0, 0); // water above MIN but below the 1/2 float → no normal demand
  run(m, 2600);
  EXPECT(active() == -1, "no auto start below the 1/2 float");
  press(m, TP.preEmptyBtn);
  run(m, 800);
  EXPECT(active() == 0, "PRE-EMPTY starts a pump even below the 1/2 float");
  EXPECT(preEmptyLampFlashed(m, 1200), "pre-empty lamp flashes while the cycle runs");
  setFloats(0, 0, 0); // drained below MIN
  run(m, 2600);
  EXPECT(active() == -1, "pre-empty drains to MIN then stops");
  run(m, 5000, 200); // let the 5 s ack window expire
  EXPECT(g_dout[TP.lampPreEmpty] == 0, "pre-empty lamp off after drain + ack window");
  run(m, 2000);
  EXPECT(active() == -1, "does not restart after a completed pre-empty");
}

static void t_preempty_ack_blink_when_empty()
{
  PompeManager m(TP);
  boot(m); // tank empty (all floats low)
  press(m, TP.preEmptyBtn);
  EXPECT(preEmptyLampFlashed(m, 2000), "lamp blinks to acknowledge the press even with an empty tank");
  EXPECT(active() == -1, "no pump runs when there's nothing to drain");
  run(m, 6000, 200); // ack window (~5 s) expires
  EXPECT(g_dout[TP.lampPreEmpty] == 0, "ack blink stops after ~5 s");
  EXPECT(active() == -1, "still no pump");
}

static void t_preempty_reset_aborts()
{
  PompeManager m(TP);
  boot(m);
  setFloats(1, 0, 0); // water above MIN, below 1/2
  run(m, 2600);
  setMode(0, OFF);
  setMode(1, OFF); // disable both so the request can't run a pump yet
  run(m, 300);
  press(m, TP.preEmptyBtn); // request latches but nothing can run
  run(m, 500);
  EXPECT(active() == -1, "both pumps OFF → pre-empty cannot run yet");
  press(m, TP.reset); // abort the standing request
  run(m, 300);
  setMode(0, AUTO);
  setMode(1, AUTO); // pumps available again
  run(m, 2000);
  EXPECT(active() == -1, "RESET cleared the pre-empty request → no pump starts");
}

// ----------------------------------------------------------------- runner
#define RUN(fn)            \
  do                       \
  {                        \
    g_curtest = #fn;       \
    fn();                  \
    EXPECT(g_interlockOk, "INTERLOCK: both pump relays were never energised together"); \
  } while (0)

int main()
{
  RUN(t_basic_cycle_and_interlock);
  RUN(t_alternation);
  RUN(t_undercurrent_fault_fast_handoff);
  RUN(t_overcurrent_fault);
  RUN(t_startup_grace);
  RUN(t_overwhelmed_warns_not_faults);
  RUN(t_start_rate_limit);
  RUN(t_exercise_autodrain);
  RUN(t_cooldown_autoretry);
  RUN(t_lockout_after_repeated_faults_and_reset);
  RUN(t_min_float_fault_timer_mode);
  RUN(t_half_float_fault_is_emergency);
  RUN(t_high_water_override_ignores_cooldown);
  RUN(t_silence_mutes_siren_only);
  RUN(t_hoa_manual_and_interlock);
  RUN(t_off_disables);
  RUN(t_preempty_drains_to_min);
  RUN(t_preempty_ack_blink_when_empty);
  RUN(t_preempty_reset_aborts);
  RUN(t_level_lamps_mirror_floats);

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0)
    printf("ALL TESTS PASSED\n");
  return g_failures == 0 ? 0 : 1;
}
