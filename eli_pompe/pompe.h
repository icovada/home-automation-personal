#ifndef POMPE_H
#define POMPE_H

#include <Arduino.h>

/*
 * PompeManager — duplex sump-pump lift-station controller for CONTROLLINO MAXI.
 *
 * Two pumps share a single discharge pipe, so ONLY ONE may run at a time
 * (hard interlock). The lead pump alternates every successful cycle for even
 * wear. Two Seneca T201 amp clamps (4-20 mA, 0-10 A span) verify each pump is
 * actually drawing normal current. A pump is faulted if its current leaves the
 * normal band after startup, OR if the 1/2 float fails to clear within a max
 * time (level not dropping = clogged / stuck check valve). On fault we switch
 * to the other pump and auto-retry the faulted one after a cooldown. After
 * MAX_CONSECUTIVE_FAULTS faults without a good cycle in between, a pump locks
 * out permanently (blinking fault lamp) until a manual RESET. If both pumps are
 * unavailable, or water reaches the 3/4 float, the alarm sounds.
 *
 * Float-consistency faults (a higher float active while a lower one reads dry
 * is physically impossible): 3/4 without 1/2 → 1/2 float broken (alarm); 1/2
 * without MIN → MIN float broken (alarm + the running pump switches to a drain
 * timer since it can no longer trust MIN to tell it when to stop).
 *
 * Pre-empty (storm pre-drain): a button drains the tank to MIN now, even below
 * the 1/2 float, to free up buffer capacity before bad weather. It's a second
 * "demand" source (never runs dry: only engages while MIN is wet, stops at MIN).
 * A dedicated lamp flashes while engaged, and for PREEMPTY_ACK_MS after any
 * press to confirm the press registered.
 *
 * Self-contained on purpose: no network / MQTT, only <Controllino.h> at the
 * sketch level. All timing is millis()-based; nothing blocks the loop.
 */

// ------------------------------------------------------------------ tunables
// All times in milliseconds. Adjust on site.
#define FLOAT_DEBOUNCE_MS   2000UL    // float must hold a level this long (kills ripple/chatter)
#define BTN_DEBOUNCE_MS       50UL    // buttons / MOA selector debounce
#define STARTUP_GRACE_MS    5000UL    // ignore current band for inrush/priming after start
#define MAX_CLEAR_HALF_MS 120000UL    // pump must drop level below the 1/2 float within this
#define DRAIN_TIMER_MS     30000UL    // MIN-float-fault mode: keep running this long after 1/2 opens
                                      // (≈ normal 1/2→MIN drain time; tune on site)
#define MAX_RUN_MS        900000UL    // absolute safety cap on a single run (15 min, catches a stuck-high MIN float)
#define MIN_OFF_TIME_MS    15000UL    // anti-short-cycle: min off time before a normal auto start
#define SWITCH_DEADTIME_MS  1000UL    // dead time between one pump off and the next on
#define FAULT_COOLDOWN_MS 600000UL    // auto-retry a faulted pump after this (10 min)
#define MAX_CONSECUTIVE_FAULTS  3     // consecutive faults before a pump locks out (manual RESET required)
#define ALARM_BLINK_MS       500UL    // panel alarm-lamp blink half-period
#define PREEMPTY_ACK_MS     5000UL    // pre-empty button: confirmation-blink window after a press
#define STATUS_PRINT_MS    10000UL    // periodic status line on Serial

// Current monitoring — Seneca T201: 4-20 mA over a 0-10 A span.
#define AMP_SPAN_A          10.0f     // amps at 20 mA (full scale)
#define NORMAL_AMP_MIN       2.0f     // below this (after grace) = not running / dry / tripped
#define NORMAL_AMP_MAX       8.0f     // above this = jammed / locked rotor
#define SENSOR_FAULT_AMPS   -1.0f     // reading well below the 4 mA zero = broken current loop

// The CONTROLLINO MAXI analog inputs read 0-24 V full scale (10-bit, ~42.6
// counts/V) — NOT 0-5 V. Size the burden resistor for that range: ~500 ohm turns
// 4-20 mA into ~2-10 V (≈85-426 counts), a good span well inside 24 V and within
// the T201's drive limit (~250 ohm → ~1-5 V works too but uses less of the range;
// never size it so 20 mA approaches 24 V). The values below are PLACEHOLDERS for a
// 500 ohm burden — CALIBRATE: read the raw ADC at a known 4 mA and 20 mA, enter here.
#define ADC_AT_4MA            85      // PLACEHOLDER — measure! (~2 V across 500 ohm)
#define ADC_AT_20MA          426      // PLACEHOLDER — measure! (~10 V across 500 ohm)

// Float input electrical sense. true = contact closes to +24 V when water rises
// (normally-open). Set false for normally-closed floats.
#define FLOAT_ACTIVE_HIGH   true

// ----------------------------------------------------------------- pin bundle
struct PompePins
{
  uint8_t pumpRelay[2];               // pump switching-relay coils (only one on at a time, enforced in software)
  uint8_t beaconRelay;                // remote flashing beacon (use a self-flashing one)
  uint8_t sirenRelay;                 // siren (muted by the silence button)
  uint8_t runLamp[2];                 // per-pump RUN indicator (24 V)
  uint8_t faultLamp[2];               // per-pump FAULT indicator (24 V)
  uint8_t lampMin, lampHalf, lampHigh;// level indicators (24 V)
  uint8_t lampAlarm;                  // panel alarm lamp (24 V, blinks)
  uint8_t lampPreEmpty;               // pre-empty active lamp (24 V, flashes)
  uint8_t curPin[2];                  // analog inputs from the T201 amp clamps
  uint8_t floatMin, floatHalf, floatHigh; // float switch inputs
  uint8_t silence, reset;             // momentary buttons
  uint8_t preEmptyBtn;                // momentary button: pre-empty the tank now
  uint8_t manual[2], autom[2];        // MOA selector: 2 inputs/pump (Manual / Auto, neither = Off)
};

enum PumpMode  { MODE_OFF, MODE_MANUAL, MODE_AUTO };
enum FaultReason { F_NONE, F_UNDERCURRENT, F_OVERCURRENT, F_INEFFECTIVE };

// ---------------------------------------------------------- debounced input
class DebInput
{
  uint8_t pin = 255;
  bool activeHigh = true;
  bool lastRaw = false;
  unsigned long lastChange = 0;

public:
  bool stable = false;   // current debounced state
  bool justRose = false; // true for the one update where it transitioned 0->1

  void begin(uint8_t p, bool ah = FLOAT_ACTIVE_HIGH)
  {
    pin = p;
    activeHigh = ah;
    pinMode(pin, INPUT);
  }

  bool update(unsigned long now, unsigned long debounce)
  {
    justRose = false;
    bool raw = digitalRead(pin);
    if (!activeHigh)
      raw = !raw;

    if (raw != lastRaw)
    {
      lastRaw = raw;
      lastChange = now;
    }
    else if (raw != stable && (now - lastChange) >= debounce)
    {
      if (raw)
        justRose = true;
      stable = raw;
    }
    return stable;
  }
};

// --------------------------------------------------------------- the manager
class PompeManager
{
  PompePins pins;

  DebInput minFloat, halfFloat, highFloat;
  DebInput silenceBtn, resetBtn, preEmptyInput;
  DebInput manualInput[2], autoInput[2];

  PumpMode mode[2] = {MODE_OFF, MODE_OFF};

  int activePump = -1;                 // -1 none, else 0/1 (single source of truth for the interlock)
  int leadPump = 0;                    // alternates after each successful cycle
  bool faulted[2] = {false, false};
  bool sensorFault[2] = {false, false};
  bool lockedOut[2] = {false, false}; // permanent fault — needs a manual RESET
  byte retryCount[2] = {0, 0};        // consecutive faults since the last good cycle
  FaultReason faultReason[2] = {F_NONE, F_NONE};
  unsigned long faultUntil[2] = {0, 0};
  unsigned long startMillis[2] = {0, 0};
  unsigned long lastStop = 0;             // any pump stop → drives SWITCH_DEADTIME
  unsigned long pumpLastStop[2] = {0, 0}; // per-pump stop time → drives MIN_OFF_TIME
  bool everStopped[2] = {false, false};   // no anti-short-cycle delay before a pump's first run

  bool alarmActive = false;
  bool alarmSilenced = false;
  bool halfFloatBroken = false; // 3/4 reached but 1/2 says dry → 1/2 float stuck/broken
  bool prevHalfBroken = false;
  bool minFloatBroken = false;  // 1/2 reached but MIN says dry → MIN float stuck/broken
  bool prevMinBroken = false;
  bool timerMode = false;       // this run can't trust MIN → stop on a drain timer instead
  unsigned long drainStart = 0; // when the 1/2 float opened during a timer-mode run
  bool preEmpty = false;        // storm pre-drain requested → drain to MIN even below the 1/2 float
  bool preEmptyAck = false;     // confirmation-blink window active after a button press
  unsigned long preEmptyAckStart = 0;
  bool blinkState = false;
  unsigned long lastBlink = 0;
  unsigned long lastStatus = 0;

public:
  PompeManager(const PompePins &p) : pins(p) {}

  void begin()
  {
    for (int p = 0; p < 2; p++)
    {
      pinMode(pins.pumpRelay[p], OUTPUT); digitalWrite(pins.pumpRelay[p], LOW);
      pinMode(pins.runLamp[p], OUTPUT);   digitalWrite(pins.runLamp[p], LOW);
      pinMode(pins.faultLamp[p], OUTPUT); digitalWrite(pins.faultLamp[p], LOW);
    }
    pinMode(pins.beaconRelay, OUTPUT); digitalWrite(pins.beaconRelay, LOW);
    pinMode(pins.sirenRelay, OUTPUT);  digitalWrite(pins.sirenRelay, LOW);
    pinMode(pins.lampMin, OUTPUT);     digitalWrite(pins.lampMin, LOW);
    pinMode(pins.lampHalf, OUTPUT);    digitalWrite(pins.lampHalf, LOW);
    pinMode(pins.lampHigh, OUTPUT);    digitalWrite(pins.lampHigh, LOW);
    pinMode(pins.lampAlarm, OUTPUT);   digitalWrite(pins.lampAlarm, LOW);
    pinMode(pins.lampPreEmpty, OUTPUT); digitalWrite(pins.lampPreEmpty, LOW);

    minFloat.begin(pins.floatMin);
    halfFloat.begin(pins.floatHalf);
    highFloat.begin(pins.floatHigh);
    silenceBtn.begin(pins.silence, true); // buttons close to +24 V
    resetBtn.begin(pins.reset, true);
    preEmptyInput.begin(pins.preEmptyBtn, true);
    for (int p = 0; p < 2; p++)
    {
      manualInput[p].begin(pins.manual[p], true);
      autoInput[p].begin(pins.autom[p], true);
    }
    Serial.println(F("PompeManager ready"));
  }

  // Read RMS current (A) for a pump from its 4-20 mA loop. May go slightly
  // negative below the 4 mA zero (used to detect a broken loop).
  float readAmps(int p)
  {
    long sum = 0;
    for (int i = 0; i < 8; i++)
      sum += analogRead(pins.curPin[p]);
    int adc = sum / 8;
    return AMP_SPAN_A * (float)(adc - ADC_AT_4MA) / (float)(ADC_AT_20MA - ADC_AT_4MA);
  }

  void check()
  {
    unsigned long now = millis();

    // --- 1. read & debounce all inputs ---
    bool fMin  = minFloat.update(now, FLOAT_DEBOUNCE_MS);
    bool fHalf = halfFloat.update(now, FLOAT_DEBOUNCE_MS);
    bool fHigh = highFloat.update(now, FLOAT_DEBOUNCE_MS);

    silenceBtn.update(now, BTN_DEBOUNCE_MS);
    resetBtn.update(now, BTN_DEBOUNCE_MS);
    preEmptyInput.update(now, BTN_DEBOUNCE_MS);
    for (int p = 0; p < 2; p++)
    {
      manualInput[p].update(now, BTN_DEBOUNCE_MS);
      autoInput[p].update(now, BTN_DEBOUNCE_MS);
      if (manualInput[p].stable)      mode[p] = MODE_MANUAL;
      else if (autoInput[p].stable) mode[p] = MODE_AUTO;
      else                       mode[p] = MODE_OFF;
    }

    // --- 1b. PRE-EMPTY button (storm pre-drain) ---
    // Every press flashes the lamp for PREEMPTY_ACK_MS to confirm it registered;
    // it only engages a drain cycle if there's water above MIN to pump.
    if (preEmptyInput.justRose)
    {
      preEmptyAck = true;
      preEmptyAckStart = now;
      if (fMin)
      {
        preEmpty = true;
        Serial.println(F("PRE-EMPTY requested"));
      }
      else
      {
        Serial.println(F("PRE-EMPTY: nothing to drain (tank already at/below MIN)"));
      }
    }
    if (!fMin)
      preEmpty = false; // drained to MIN (or nothing to do) → request satisfied/void
    if (preEmptyAck && (now - preEmptyAckStart) >= PREEMPTY_ACK_MS)
      preEmptyAck = false;

    // --- 2. RESET button: clear faults / cooldowns / silence ---
    if (resetBtn.justRose)
    {
      for (int p = 0; p < 2; p++)
      {
        faulted[p] = false;
        sensorFault[p] = false;
        lockedOut[p] = false;
        retryCount[p] = 0;
        faultReason[p] = F_NONE;
        faultUntil[p] = 0;
      }
      alarmSilenced = false;
      preEmpty = false; // RESET also aborts an in-progress pre-empty
      Serial.println(F("RESET: faults cleared"));
    }

    // --- 3. cooldown expiry → auto-retry (locked-out pumps stay down) ---
    for (int p = 0; p < 2; p++)
    {
      if (faulted[p] && !lockedOut[p] && (long)(now - faultUntil[p]) >= 0)
      {
        faulted[p] = false;
        faultReason[p] = F_NONE;
        Serial.print(F("Pump "));
        Serial.print(p + 1);
        Serial.println(F(": cooldown over, re-enabled"));
      }
    }

    // --- 4. OFF pumps must not run ---
    for (int p = 0; p < 2; p++)
      if (mode[p] == MODE_OFF && activePump == p)
      {
        stop(now);
        Serial.print(F("Pump "));
        Serial.print(p + 1);
        Serial.println(F(": switched OFF"));
      }

    // --- 5. MANUAL override (manual run; interlock still enforced) ---
    int manualPump = (mode[0] == MODE_MANUAL) ? 0 : (mode[1] == MODE_MANUAL ? 1 : -1);

    if (manualPump != -1)
    {
      if (activePump != manualPump)
      {
        if (activePump != -1)
          stop(now);                    // shut the other one down first
        else
          tryStart(manualPump, now, false); // start after dead time
      }
    }
    else
    {
      // --- 6. AUTO control ---
      if (activePump != -1 && mode[activePump] == MODE_AUTO)
      {
        int p = activePump;
        unsigned long run = now - startMillis[p];

        // 1/2 reached while MIN reads dry is impossible → MIN float broken.
        // This run can't trust MIN, so fall back to a drain timer.
        if (fHalf && !fMin)
          timerMode = true;

        // ---- decide when to stop ----
        bool stopNow = false;
        if (timerMode)
        {
          // keep running until the 1/2 float opens, then a fixed extra drain time
          if (fHalf)
            drainStart = 0;
          else
          {
            if (drainStart == 0)
              drainStart = now;
            if (now - drainStart >= DRAIN_TIMER_MS)
              stopNow = true;
          }
        }
        else if (!fMin)
        {
          // healthy: water drained below the MIN float
          stopNow = true;
        }
        if (!stopNow && run >= MAX_RUN_MS)
          stopNow = true; // safety backstop (e.g. a MIN float stuck high)

        if (stopNow)
        {
          stop(now);
          leadPump = 1 - p;   // alternate after the cycle
          retryCount[p] = 0;  // good cycle → clear the consecutive-fault count
          Serial.print(F("Pump "));
          Serial.print(p + 1);
          Serial.println(F(": stop (alternating)"));
        }
        else if (run > STARTUP_GRACE_MS)
        {
          if (fHalf && run > MAX_CLEAR_HALF_MS)
          {
            // level not dropping below 1/2 in time → ineffective / stuck
            raiseFault(p, F_INEFFECTIVE, now);
          }
          else
          {
            float amps = readAmps(p);
            if (amps < SENSOR_FAULT_AMPS)
            {
              // broken 4-20 mA loop: can't trust current, lean on the level timer
              if (!sensorFault[p])
              {
                sensorFault[p] = true;
                Serial.print(F("Pump "));
                Serial.print(p + 1);
                Serial.println(F(": current loop fault (check wiring)"));
              }
            }
            else if (amps < NORMAL_AMP_MIN)
              raiseFault(p, F_UNDERCURRENT, now);
            else if (amps > NORMAL_AMP_MAX)
              raiseFault(p, F_OVERCURRENT, now);
            else
              sensorFault[p] = false;
          }
        }
      }

      // demand = water reached the 1/2 float, OR a pre-empty drain was requested
      if (activePump == -1 && (fHalf || preEmpty))
      {
        int p = pickPump(false);
        if (p != -1)
          tryStart(p, now, true); // normal start honours MIN_OFF_TIME
      }
    }

    // --- 7. alarm evaluation ---
    bool bothFaulted = faulted[0] && faulted[1];
    // Water above the 3/4 float while the 1/2 float still reads dry is
    // physically impossible → the 1/2 float is stuck/broken.
    halfFloatBroken = fHigh && !fHalf;
    if (halfFloatBroken && !prevHalfBroken)
      Serial.println(F("ALARM: 1/2 float fault (3/4 reached but 1/2 not triggered)"));
    prevHalfBroken = halfFloatBroken;

    // 1/2 reached while MIN reads dry → MIN float stuck/broken (running on the drain timer).
    minFloatBroken = fHalf && !fMin;
    if (minFloatBroken && !prevMinBroken)
      Serial.println(F("WARNING: MIN float fault (1/2 reached but MIN not triggered) — timer mode"));
    prevMinBroken = minFloatBroken;

    // EMERGENCY tier → siren + beacon + panel-lamp blink (the "drop everything" cases:
    // high water, a 1/2 float fault while water is already at 3/4, or both pumps unavailable).
    bool newAlarm = fHigh || halfFloatBroken || (bothFaulted && fHalf);

    // high-water emergency: try any non-overcurrent pump, ignoring cooldown
    if (fHigh && activePump == -1)
    {
      int p = pickPump(true);
      if (p != -1)
        tryStart(p, now, false);
    }

    if (newAlarm && !alarmActive)
    {
      alarmActive = true;
      alarmSilenced = false;
      Serial.println(F("ALARM raised"));
    }
    else if (!newAlarm && alarmActive)
    {
      alarmActive = false;
      alarmSilenced = false;
      Serial.println(F("ALARM cleared"));
    }
    if (silenceBtn.justRose && alarmActive && !alarmSilenced)
    {
      alarmSilenced = true;
      Serial.println(F("ALARM silenced (siren muted)"));
    }

    // --- 8. drive outputs (relays derived from activePump → interlock by construction) ---
    // pre-empty lamp flashes while a cycle is engaged or during the post-press ack window
    bool preEmptyLamp = preEmpty || preEmptyAck;
    // advance the shared blink timer while anything that blinks is active
    if (alarmActive || lockedOut[0] || lockedOut[1] || minFloatBroken || preEmptyLamp)
    {
      if (now - lastBlink >= ALARM_BLINK_MS)
      {
        lastBlink = now;
        blinkState = !blinkState;
      }
    }
    else
      blinkState = false;

    digitalWrite(pins.pumpRelay[0], activePump == 0 ? HIGH : LOW);
    digitalWrite(pins.pumpRelay[1], activePump == 1 ? HIGH : LOW);

    digitalWrite(pins.runLamp[0], activePump == 0 ? HIGH : LOW);
    digitalWrite(pins.runLamp[1], activePump == 1 ? HIGH : LOW);
    for (int p = 0; p < 2; p++)
    {
      bool lamp;
      if (lockedOut[p])
        lamp = blinkState; // permanent fault → blink (needs manual reset)
      else
        lamp = faulted[p] || sensorFault[p]; // temporary fault / cooldown → steady
      digitalWrite(pins.faultLamp[p], lamp ? HIGH : LOW);
    }

    // blink a level lamp when its float is flagged broken, else steady = its level
    digitalWrite(pins.lampMin, minFloatBroken ? (blinkState ? HIGH : LOW) : (fMin ? HIGH : LOW));
    digitalWrite(pins.lampHalf, halfFloatBroken ? (blinkState ? HIGH : LOW) : (fHalf ? HIGH : LOW));
    digitalWrite(pins.lampHigh, fHigh ? HIGH : LOW);

    // WARNING tier → big red beacon only (an issue worth seeing, but not "run"):
    // any pump faulted/locked/unmonitored, or a MIN float fault.
    bool warning = faulted[0] || faulted[1] || sensorFault[0] || sensorFault[1] || minFloatBroken;
    digitalWrite(pins.lampAlarm, alarmActive ? (blinkState ? HIGH : LOW) : LOW);  // panel lamp: emergency only, blinks
    digitalWrite(pins.beaconRelay, (alarmActive || warning) ? HIGH : LOW);        // beacon: warning OR emergency
    digitalWrite(pins.sirenRelay, (alarmActive && !alarmSilenced) ? HIGH : LOW);  // siren: emergency only, mutable
    digitalWrite(pins.lampPreEmpty, preEmptyLamp ? (blinkState ? HIGH : LOW) : LOW); // pre-empty: flashes when engaged / acknowledged

    // --- 9. periodic status ---
    if (now - lastStatus >= STATUS_PRINT_MS)
    {
      lastStatus = now;
      printStatus(fMin, fHalf, fHigh);
    }
  }

private:
  // Start pump p if the interlock allows it. requireMinOff adds the
  // anti-short-cycle delay (normal starts); fault hand-off / emergency pass false.
  bool tryStart(int p, unsigned long now, bool requireMinOff)
  {
    if (activePump != -1)
      return false;
    if (now - lastStop < SWITCH_DEADTIME_MS)
      return false; // dead time between any two energisations (interlock spacing)
    if (requireMinOff && everStopped[p] && (now - pumpLastStop[p] < MIN_OFF_TIME_MS))
      return false; // anti-short-cycle, per pump — does not delay a fault hand-off to the other pump

    activePump = p;
    startMillis[p] = now;
    sensorFault[p] = false;
    timerMode = false;
    drainStart = 0;
    Serial.print(F("Pump "));
    Serial.print(p + 1);
    Serial.println(F(": START"));
    return true;
  }

  void stop(unsigned long now)
  {
    if (activePump >= 0)
    {
      pumpLastStop[activePump] = now;
      everStopped[activePump] = true;
    }
    activePump = -1;
    lastStop = now;
  }

  void raiseFault(int p, FaultReason r, unsigned long now)
  {
    stop(now);
    faulted[p] = true;
    faultReason[p] = r;
    faultUntil[p] = now + FAULT_COOLDOWN_MS;
    retryCount[p]++;
    if (retryCount[p] >= MAX_CONSECUTIVE_FAULTS)
      lockedOut[p] = true;
    Serial.print(F("Pump "));
    Serial.print(p + 1);
    Serial.print(F(": FAULT ("));
    switch (r)
    {
    case F_UNDERCURRENT: Serial.print(F("undercurrent")); break;
    case F_OVERCURRENT:  Serial.print(F("overcurrent")); break;
    case F_INEFFECTIVE:  Serial.print(F("not draining")); break;
    default:             Serial.print(F("?")); break;
    }
    Serial.print(F(") #"));
    Serial.print(retryCount[p]);
    if (lockedOut[p])
      Serial.println(F(" — LOCKED OUT, manual reset required"));
    else
      Serial.println(F(" — switching pump"));
  }

  // Choose a pump to start, lead first. Normal: must be AUTO and not faulted.
  // Emergency (high water): ignore cooldown but skip an overcurrent pump.
  int pickPump(bool emergency)
  {
    for (int i = 0; i < 2; i++)
    {
      int p = (leadPump + i) % 2;
      if (mode[p] != MODE_AUTO)
        continue;
      if (!emergency)
      {
        if (faulted[p])
          continue;
      }
      else if (faulted[p] && faultReason[p] == F_OVERCURRENT)
        continue;
      return p;
    }
    return -1;
  }

  void printStatus(bool fMin, bool fHalf, bool fHigh)
  {
    Serial.print(F("[status] active="));
    Serial.print(activePump);
    Serial.print(F(" lead="));
    Serial.print(leadPump);
    Serial.print(F(" floats(min/half/high)="));
    Serial.print(fMin); Serial.print(fHalf); Serial.print(fHigh);
    Serial.print(F(" mode="));
    Serial.print(mode[0]); Serial.print('/'); Serial.print(mode[1]);
    Serial.print(F(" fault="));
    Serial.print(faulted[0]); Serial.print('/'); Serial.print(faulted[1]);
    Serial.print(F(" locked="));
    Serial.print(lockedOut[0]); Serial.print('/'); Serial.print(lockedOut[1]);
    Serial.print(F(" retries="));
    Serial.print(retryCount[0]); Serial.print('/'); Serial.print(retryCount[1]);
    Serial.print(F(" timerMode="));
    Serial.print(timerMode);
    Serial.print(F(" preEmpty="));
    Serial.print(preEmpty);
    Serial.print(F(" A="));
    Serial.print(readAmps(0), 1); Serial.print('/'); Serial.print(readAmps(1), 1);
    Serial.print(F(" alarm="));
    Serial.println(alarmActive);
  }
};

#endif
