# Akai MPK mini IV for OpenOrc

*English · [Italiano](akai-mpk-mini-iv.it.md)*

OpenOrc uses the **factory values of the MPK mini IV user presets**:
- pads on notes 36–51, channel 10;
- knobs on CC 24–31, Abs mode.

The file `akai-mpk-mini-iv.txt` contains these values, so usually **the keyboard needs no setup**.
Akai does not publish the MIDI implementation, so the values come from two community projects (a
REAPER/ReaLearn configuration and a Reason codec). For that reason this sheet shows how to check them
and, if needed, how to set them. The MPK has no editor software and cannot import files: it is set up
from its own panel only (user guide v1.0, pages 20-22; Akai FAQ).

OpenOrc's screens are in Italian: the labels you will see are quoted below, with a translation.

## 1. Quick check (2 minutes)
1. On the MPK: **SHIFT + PLUGIN/DAW** (User Presets), pick a **user** preset and press the encoder.
   The DAW and Plugin presets send their controls to DAW scripts.
2. Connect the MPK to the Flip's USB-C port and open OpenOrc.
3. Press **SELECT** until the **MIDI** page, then **Y**: "Tastiera: Akai MPK mini IV" appears.
4. Hit a pad and turn a knob. At the bottom, the "Ultimo messaggio" line (last message) must show
   "Nota 36…51 · canale 10" and "CC 24…31".

If it all matches, you are done. Otherwise go to step 2, or let OpenOrc learn the controls on the MIDI
page (A on a row, then move the control) and save with **R2**.

## 2. Setting up the MPK by hand (only if needed)
**Global menu (SHIFT + LOOP)**

| Setting | Value |
|---|---|
| MidiCh | 1 (keys and knobs) |
| PadCh | **10** |
| KnobM | **Abs** |
| Toggle | Off |

Leave with **PLUGIN/DAW**.

**Program Edit (SHIFT + OCT -)**: touch or move the control you want to edit, then set it with the
encoder.

For the pads: MidiCh **10**. **BANK A/B** switches the bank.

| | left | 2nd | 3rd | right |
|---|---|---|---|---|
| **Bank A, top row** | 40 Dim | 41 Min | 42 Maj | 43 Sus |
| **Bank A, bottom row** | 36 6 | 37 m7 | 38 M7 | 39 9 |
| **Bank B, top row** | 48 Key mode | 49 Next playing style | 50 Previous sound | 51 Next sound |
| **Bank B, bottom row** | 44 Drums | 45 Loop: record | 46 Loop: play/stop | 47 Loop: clear |

This is the MPC layout: the note is 35 + the pad number, counting from the bottom-left pad.

For the knobs: CC#, LoVal 0, HiVal 127, Mode **Abs**.

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| Voicing | Bass | Playing style amount | Tone | Chorus | Delay | Reverb | Volume |

The labels printed above the knobs are for the arpeggiator (ARP + knob), not for OpenOrc.

**Save:** **SHIFT + OCT +**, choose the user slot with the encoder and press it. From then on you recall
it with **SHIFT + PLUGIN/DAW**.

## 3. Good to know
- **Turn off the MPK's own Chords, Scales, ARP and Note Repeat.** OpenOrc builds the chords: the keyboard
  must send single notes.
- **Knobs in Rel mode:** OpenOrc understands them. On the MIDI page set "Tipo di manopole" (knob type) to
  **Relative**. Most likely the MPK sends 1–63 to go up and 65–127 to go down (not verified: if a knob
  runs backwards or jumps, try "Relative (64)"). This way values never jump.
- **Transport buttons:** the preset reads them as CC on port 2 (Play 76, Record 77, Loop 74, Undo 73),
  like the community scripts. The user guide says instead that in user presets they send notes (Trnspt
  in the global menu). If they do not respond, learn the "Tasti di trasporto" group (transport buttons)
  and save with R2.
- **Firmware:** update it once from a computer (inMusic Software Center) before use. With old firmware
  the ports may differ.
- Never send SysEx to the MPK at random. Some function codes put it into firmware update mode.

## Other keyboards
Copy `akai-mpk-mini-iv.txt` into `Saves/openorc/tastiere/` under another name and change the numbers:
Y finds it. Or let the MIDI page learn the controls and save with R2. Files in `Saves/openorc/tastiere/`
survive Cirmolo updates.
