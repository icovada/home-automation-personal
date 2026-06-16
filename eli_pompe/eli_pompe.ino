/*
 * eli_pompe — sump-pump lift-station controller (CONTROLLINO MAXI, no network).
 *
 * Two pumps, one shared discharge pipe → only one runs at a time. Lead pump
 * alternates each cycle. Floats: MIN (stop) + 1/2 (start) wired now, 3/4
 * (high-water alarm) pre-wired for later. YHDC 0-10 V current sensors monitor
 * each pump. See pompe.h for the control logic and tunables.
 *
 * Most indication is the controller's own per-channel LEDs (pump relay LEDs =
 * RUN, float input LEDs = levels, beacon relay LED = alarm state); only the
 * beacon and siren are wired externally.
 *
 * Wiring notes:
 *  - R0/R1 drive socket-mounted Finder relays (swap without rewiring). Only one
 *    pump is ever commanded on (software interlock); a hardware interlock is
 *    optional — running both only wastes flow on the shared pipe (see MANUAL §5.1).
 *  - YHDC current sensors (split-core CT, 0-10 V output, 24 V powered) wire into
 *    A0/A1 — NO burden resistor. Calibrate ADC_AT_0A / ADC_AT_FS in pompe.h.
 *  - Outputs: R2 = beacon (SELF-FLASHING; relay stays steady), R3 = siren (own
 *    relay so Silence mutes it). 24 V lamps: D0/D1/D2 = level MIN/1-2/3-4,
 *    D3/D4 = pump 1/2 FAULT, D5 = PRE-EMPTY. (RUN = pump relay LEDs; alarm = beacon LED.)
 *  - Floats are normally-open, closing to +24 V on rise (FLOAT_ACTIVE_HIGH).
 *  - The 3/4 float input is safe to leave unconnected (reads low = no alarm).
 *  - MOA = maintained 3-position selector wired as 2 inputs/pump (Manual / Auto,
 *    center Off = neither input active).
 *  - PRE-EMPTY: momentary button on A7 drains the tank to MIN now (storm prep);
 *    the D5 lamp flashes while running / for 5 s after any press.
 *  - All 12 inputs used (A0-A9 + IN0/IN1) — no spare inputs.
 */

#include <Controllino.h>
#include <avr/wdt.h>
#include "pompe.h"

// ----- pin map (CONTROLLINO MAXI) -----
PompePins pumpPins = {
    /* pumpRelay  */ {CONTROLLINO_R0, CONTROLLINO_R1}, // pump 1 / pump 2 switching relays
    /* beaconRelay*/ CONTROLLINO_R2,                   // beacon (relay LED = alarm state)
    /* sirenRelay */ CONTROLLINO_R3,                    // siren (mutable)
    /* faultLamp  */ {CONTROLLINO_D3, CONTROLLINO_D4},  // pump 1 / pump 2 FAULT lamps
    /* lampMin    */ CONTROLLINO_D0,
    /* lampHalf   */ CONTROLLINO_D1,
    /* lampHigh   */ CONTROLLINO_D2,
    /* lampPreEmpty*/ CONTROLLINO_D5,                   // pre-empty active lamp (flashes)
    /* curPin     */ {CONTROLLINO_A0, CONTROLLINO_A1},  // YHDC 0-10 V current sensors
    /* floatMin   */ CONTROLLINO_A2,
    /* floatHalf  */ CONTROLLINO_A3,
    /* floatHigh  */ CONTROLLINO_A4,                   // 3/4 alarm float (future)
    /* silence    */ CONTROLLINO_A5,
    /* reset      */ CONTROLLINO_A6,
    /* preEmptyBtn*/ CONTROLLINO_A7,                    // storm pre-drain button
    /* manual     */ {CONTROLLINO_A8, CONTROLLINO_IN0}, // pump 1 / pump 2 MOA-Manual
    /* autom      */ {CONTROLLINO_A9, CONTROLLINO_IN1}  // pump 1 / pump 2 MOA-Auto
};

PompeManager pompe(pumpPins);

void setup()
{
  wdt_disable();
  Serial.begin(115200);
  Serial.println(F("Start eli_pompe"));

  pompe.begin();

  wdt_enable(WDTO_4S);
  Serial.println(F("End setup"));
}

void loop()
{
  pompe.check();
  wdt_reset();
}
