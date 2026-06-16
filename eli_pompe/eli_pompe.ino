/*
 * eli_pompe — sump-pump lift-station controller (CONTROLLINO MAXI, no network).
 *
 * Two pumps, one shared discharge pipe → only one runs at a time. Lead pump
 * alternates each cycle. Floats: MIN (stop) + 1/2 (start) wired now, 3/4
 * (high-water alarm) pre-wired for later. Seneca T201 amp clamps (4-20 mA,
 * 0-10 A) monitor each pump. See pompe.h for the control logic and tunables.
 *
 * Wiring notes:
 *  - R0/R1 drive socket-mounted Finder relays (swap without rewiring). Only one
 *    pump is ever commanded on (software interlock); a hardware interlock is
 *    optional — running both only wastes flow on the shared pipe (see MANUAL §5.1).
 *  - The MAXI analog inputs read 0-24 V full scale (NOT 0-5 V). Feed the T201
 *    4-20 mA loops into A0/A1 through a burden resistor to ground (~500 ohm →
 *    ~2-10 V); calibrate ADC_AT_4MA / ADC_AT_20MA in pompe.h on the bench.
 *  - Beacon relay drives a SELF-FLASHING beacon (relay stays steady); siren on
 *    its own relay so the Silence button can mute it while the beacon/lamp stay.
 *  - Floats are normally-open, closing to +24 V on rise (FLOAT_ACTIVE_HIGH).
 *  - The 3/4 float input is safe to leave unconnected (reads low = no alarm).
 *  - MOA = maintained 3-position selector wired as 2 inputs/pump (Manual / Auto,
 *    center Off = neither input active).
 *  - PRE-EMPTY: momentary button on IN1 drains the tank to MIN now (storm prep);
 *    the lamp on D8 flashes while running / for 5 s after any press. After this,
 *    NO spare inputs remain (A0-A9 + IN0 + IN1 all used).
 */

#include <Controllino.h>
#include <avr/wdt.h>
#include "pompe.h"

// ----- pin map (CONTROLLINO MAXI) -----
PompePins pumpPins = {
    /* pumpRelay  */ {CONTROLLINO_R0, CONTROLLINO_R1}, // pump 1 / pump 2 switching relays
    /* beaconRelay*/ CONTROLLINO_R2,                   // remote flashing beacon
    /* sirenRelay */ CONTROLLINO_R3,                   // siren (mutable)
    /* runLamp    */ {CONTROLLINO_D0, CONTROLLINO_D1}, // pump RUN lamps
    /* faultLamp  */ {CONTROLLINO_D2, CONTROLLINO_D3}, // pump FAULT lamps
    /* lampMin    */ CONTROLLINO_D4,
    /* lampHalf   */ CONTROLLINO_D5,
    /* lampHigh   */ CONTROLLINO_D6,
    /* lampAlarm  */ CONTROLLINO_D7,                   // panel alarm lamp (blinks)
    /* lampPreEmpty*/ CONTROLLINO_D8,                  // pre-empty active lamp (flashes)
    /* curPin     */ {CONTROLLINO_A0, CONTROLLINO_A1}, // T201 amp clamps (4-20 mA)
    /* floatMin   */ CONTROLLINO_A2,
    /* floatHalf  */ CONTROLLINO_A3,
    /* floatHigh  */ CONTROLLINO_A4,                   // 3/4 alarm float (future)
    /* silence    */ CONTROLLINO_A5,
    /* reset      */ CONTROLLINO_A6,
    /* preEmptyBtn*/ CONTROLLINO_IN1,                  // storm pre-drain button (last spare input)
    /* manual     */ {CONTROLLINO_A7, CONTROLLINO_A9}, // pump 1 / pump 2 MOA-Manual
    /* autom      */ {CONTROLLINO_A8, CONTROLLINO_IN0} // pump 1 / pump 2 MOA-Auto
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
