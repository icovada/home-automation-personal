# Recovery — reflashing the controller from scratch

For when the CONTROLLINO has to be **replaced** or **reprogrammed** and the
person doing it has **never used Arduino**. Allow ~1 hour the first time. You
need a Windows/Mac/Linux computer and a **USB cable** (the MAXI uses USB-B, the
square "printer" type — confirm on the unit).

> This only reprograms the **controller logic**. It does **not** touch the
> mains/pump wiring. Anything on the power side is an **electrician's** job.

> ✅ **Fastest path — swap, don't flash.** If there's a **pre-flashed spare
> Controllino** in the parts bag (see [BOM.md](BOM.md) → "Spares"), you don't need
> any of this: power off, move the **labelled** wires from the dead unit to the
> spare, power on, and run the checks in [MANUAL.md](MANUAL.md) §7. The steps
> below are only for **preparing** that spare, or if no pre-flashed spare exists.
> After using the spare, flash and bag a new one so you're never without.

---

## 0. Get the source code

It lives in two places (see the documentation drawer):

- **USB stick** in the documentation drawer — copy the whole `eli_pompe` folder to the computer.
- **GitHub:** _________________________________________________ → "Code" → "Download ZIP", unzip it.

You need the folder **`eli_pompe`** containing at least `eli_pompe.ino` and
`pompe.h`. (The `tests/` folder is not needed for flashing.) The folder name
**must** stay `eli_pompe` and must contain `eli_pompe.ino`.

---

## 1. Install the Arduino IDE

Download from <https://www.arduino.cc/en/software> and install the **Arduino IDE
2.x**. Open it.

## 2. Add CONTROLLINO board support

1. **File → Preferences.**
2. In **"Additional boards manager URLs"**, paste the CONTROLLINO package URL.
   Get the current URL from <https://www.controllino.com> (search "Arduino IDE
   board installation"). At time of writing it is:
   `https://raw.githubusercontent.com/CONTROLLINO-PLC/CONTROLLINO_Library/master/Boards/package_ControllinoHardware_index.json`
   *(If that link is dead, the CONTROLLINO website has the up-to-date one.)*
3. **OK.**
4. Open **Tools → Board → Boards Manager**, search **CONTROLLINO**, click **Install**.

## 3. Install the CONTROLLINO library

Open **Tools → Manage Libraries** (or **Sketch → Include Library → Manage
Libraries**), search **CONTROLLINO**, and **Install**. This provides
`Controllino.h`, which the sketch needs.

> This sketch is **self-contained**: it needs **no other libraries** (no
> Ethernet, no MQTT). If the IDE complains a different library is missing, you
> have the wrong sketch — make sure you opened `eli_pompe`, not one of the other
> controllers in the repo.

## 4. Open the sketch

**File → Open**, browse to the `eli_pompe` folder, open **`eli_pompe.ino`**.
(`pompe.h` opens automatically as a second tab — it must be in the same folder.)

## 5. Select the board and port

- **Tools → Board → CONTROLLINO → CONTROLLINO MAXI.**
- Plug the controller into the computer by USB.
- **Tools → Port →** pick the port that appears when you plug it in
  (Windows: `COMx`; Mac: `/dev/cu.usbmodem…` or `/dev/cu.usbserial…`).
  - If no port appears, you may need the USB-serial driver. The MAXI uses an
    **ATmega2560 / 16U2** USB chip — the same as an Arduino Mega; installing the
    Arduino IDE normally provides the driver. On older boards it may be an
    **FTDI** or **CH340** chip — install that vendor's driver if Windows shows an
    unknown device.

## 6. Compile (Verify)

Click **✓ Verify** (top-left). It should end with **"Done compiling."** If it
errors, re-check steps 2–4 (board package, CONTROLLINO library, right sketch).

## 7. Upload

Click **→ Upload**. Wait for **"Done uploading."** The board's TX/RX LEDs blink
during transfer.

## 8. Confirm it's alive

Open **Tools → Serial Monitor**, set **115200 baud**. You should see:

```
Start eli_pompe
PompeManager ready
End setup
```

and then a `[status] …` line every ~10 seconds.

## 9. Re-check calibration (important)

The current-clamp calibration values (`ADC_AT_4MA` / `ADC_AT_20MA` in `pompe.h`)
are part of the source, so a correct copy already has the right numbers. **But
verify them:** with a pump running, the Serial `A=` figure should match a clamp
meter within ~0.5 A. If it's off, redo **MANUAL.md §7 step 2** and **commit the
new values back to the repo** so the next person inherits them.

## 10. Back in service

Put both selectors to **AUTO** and run through the relevant parts of the
commissioning checklist (**MANUAL.md §7**): test each pump in **MANUAL**, then a
full **AUTO** fill/drain cycle, then trip the alarm and confirm beacon + siren.

---

## (Optional) Prove the logic on a PC without the hardware

If a programmer wants to confirm the control logic is intact before flashing:

```sh
cd eli_pompe/tests
make
```

Expect `64 checks, 0 failures · ALL TESTS PASSED`. See `tests/README.md`.

---

## Quick reference

| Thing | Value |
|-------|-------|
| Board | **CONTROLLINO MAXI** |
| Sketch | `eli_pompe/eli_pompe.ino` (+ `pompe.h` in the same folder) |
| Serial baud | **115200** |
| Extra libraries | **none** (only the CONTROLLINO board package + CONTROLLINO library) |
| `arduino-cli` build | `arduino-cli compile --fqbn CONTROLLINO_Boards:avr:controllino_maxi eli_pompe` |
| `arduino-cli` upload | `arduino-cli upload --fqbn CONTROLLINO_Boards:avr:controllino_maxi -p <port> eli_pompe` |
