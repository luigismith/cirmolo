# OpenOrc — manual

*English · [Italiano](MANUAL.it.md)*

OpenOrc is a chord synthesizer for the Miyoo Flip, inspired by the Orchid by Telepathic Instruments. You
play it with the Flip's buttons or with a USB MIDI keyboard. It is designed for the **Akai MPK mini IV**,
but it learns the controls of any keyboard. One key (or button) plays a whole chord, with a bass line, in
the chosen style: block chord, strum, arpeggio, harp and more.

The app itself is in Italian; this manual gives the on-screen names in Italian with a translation. It
describes the `cirmolo` branch as of October 2026.

## At a glance

| You want to... | Do this |
|---|---|
| play right away | **Suona** (Play) page: the d-pad and A B X Y play the chords of the key |
| change sound | **Suono** (Sound) page, first row, left/right; on the MPK, pads 50 and 51 (bank B) |
| change page | **SELECT** (Play → Sound → Rhythm → MIDI → Play) |
| start drums and tempo-synced arpeggios | **START** |
| silence every note | **L3** (press the left stick) |
| quit | **MENU**, then A |

## 1. Starting, quitting and saving
- Open OpenOrc from Cirmolo's **App** area. While it is open the console does not go to sleep.
- **MENU** quits after a confirmation. **A** or down quits, **B** or up stays.
- Settings save themselves to `Saves/openorc/stato.txt`:
  - about 8 seconds after the last change, and on exit;
  - they cover the current sound, key, tempo, loop settings and MIDI map.
- The drums always start switched off.
- The recorded loop is **not** saved: it is lost on exit.

## 2. Pages
The top bar shows the app name, the four tabs, the **MIDI** light and the transport symbol.
- The MIDI light is green when a keyboard is connected and blinks on every message.
- The transport symbol is a square when stopped. When running it is a gold triangle pulsing on the beat.

| Page | Purpose |
|---|---|
| **Suona** (Play) | playing: current chord, oscilloscope, buttons and pads |
| **Suono** (Sound) | sound, key, voicing, play mode, timbre, bass, effects |
| **Ritmo** (Rhythm) | tempo, drums, looper |
| **MIDI** | connecting and setting up the keyboard |

The bottom strip lists the page's controls.

### Controls on every page

| Button | Function |
|---|---|
| SELECT | next page |
| START | start or stop the transport: drums, synced arpeggios and patterns, looper |
| MENU | quit, with confirmation |
| L3 (left stick pressed) | all notes off ("panic") |
| left stick, horizontal | pitch bend, up to ±2 semitones |
| left stick, up | vibrato (modulation) |
| right stick, vertical | brightness: up brighter, down darker |

## 3. Playing with the Flip (Play page)
Without a keyboard you play with the console's buttons. Each button plays a degree of the key chosen on
the Sound page (C major at first). On screen each button shows the name of the chord it would play now,
laid out like the console.

| Button | Degree (major) | In C major | Degree (minor) | In A minor |
|---|---|---|---|---|
| ← left | I | C | i | Am |
| ↑ up | IV | F | iv | Dm |
| → right | V | G | v | Em |
| ↓ down | vi | Am | VI | F |
| Y | ii | Dm | ii° | B° |
| X | iii | Em | III | C |
| B | vii° | B° | VII | G |
| A | flat VII (borrowed) | B♭ | major V | E |

The shoulder buttons change the chord while you hold them. They combine and also apply to the chord
already sounding.

| Shoulder | Effect |
|---|---|
| L1 | swaps major and minor (diminished becomes major) |
| R1 | sus4: the third becomes a fourth |
| L2 | adds the scale's seventh |
| R2 | adds the ninth |

**R3** (right stick pressed) moves to the next play mode (see chapter 6).

If you hold two buttons and release one, the other plays again: the last one pressed always wins.

## 4. Playing with the Akai MPK mini IV
Plug the keyboard into the Flip's USB-C port, even with OpenOrc already open: "Tastiera collegata"
(keyboard connected) appears. The MPK works with its user presets' factory values, with no setup: see
the [MPK sheet](tastiere/akai-mpk-mini-iv.md). Turn **Chords, Scales, ARP and Note Repeat** off on the
MPK: OpenOrc builds the chords itself.

With the keyboard connected, the Play page shows the 8 chord pads on the left. The Flip's buttons still
work, drawn smaller on the right.

### Bank A pads: chords

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **top row** (quality) | Dim | Min | Maj | Sus |
| **bottom row** (extensions) | 6 | m7 | M7 | 9 |

- **Hold a quality pad and play a key:** you get that quality rooted on the key. Example: Min + D = Dm.
- **Add a bottom-row pad** for the extension: Min + m7 + D = Dm7.
- Special combinations:
  - Dim + Maj give an **augmented** chord;
  - Min + Sus give **sus2**;
  - 6 together with a seventh gives a **thirteenth**.
- With several quality pads held, the last one pressed wins.
- Only the note name matters, not the octave: the register comes from the voicing (knob K1).
- **With no pad held**, keys play **single notes** (melody) with the same sound, over the chords.
- With **key mode** on (pad 48), keys without pads play the chords of the key. Each scale note gives
  its chord; notes outside the scale take the degree just below.
- Velocity matters: playing harder makes the chord louder and brighter.

### Bank B pads: functions

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **top row** | Key mode | Next play mode | Previous sound | Next sound |
| **bottom row** | Drums | Loop: record | Loop: play or stop | Loop: clear |

### Knobs

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| Voicing | Bass | Play amount | Tone | Chorus | Delay | Reverb | Volume |

Turning a knob briefly shows its name and value. The MPK mini IV's knobs are relative, so values never
jump.

### Other controls
- **Pitch wheel:** pitch bend, ±2 semitones.
- **Mod wheel:** vibrato.
- **Sustain pedal (CC 64):** holds chords and notes until you release it.
- **Program change:** picks the sound (0 = first sound, 1 = second...).
- **Transport buttons** (Play, Rec, Loop, Undo): start/stop, record, play or stop the loop, clear the
  loop. On the MPK mini IV they may not respond: see the keyboard sheet.
- **MIDI clock:** when it comes from another device, OpenOrc follows its tempo ("Segui il clock MIDI"
  on the Rhythm page). Start and Stop messages start and stop the transport.

## 5. The Play page screen
- **Status line**, at the top: sound, play mode, key, tempo and drums, loop state.
  - The key is gold when key mode is on.
  - The loop state is red while recording.
- **Chord band**, in the middle:
  - the chord name in large type, e.g. "Lam7" (Italian note names: Do Re Mi = C D E);
  - below it, the English symbol ("Am7") and the notes;
  - then the degree in the key ("ii7 in Sol maggiore") or the bass note.
- At the bottom left of the band are the played notes with their octave. That is the voicing chosen
  by K1.
- Behind the name runs the output **oscilloscope**. Its colour tells who is playing:

| Colour | Who is playing |
|---|---|
| gold | the live chord |
| violet | the melody |
| teal | the loop |

  When nothing sounds, "silenzio" (silence) appears.

## 6. Voicing, spread and play mode
**Voicing (K1).** Picks the chord inversion by itself. It keeps the chord near the knob's register: left
low, right high. Moving between chords it picks the inversion that moves the voices least, so
progressions sound smooth.

**Spread ("Apertura").** Close, open or wide: how far apart the chord notes are.

**Play mode ("Esecuzione").** How the chord starts. The "Quantità" (amount, knob K3) depends on the mode:

| Mode | What it does | Amount |
|---|---|---|
| Accordo (chord) | all notes together | — |
| Strum | like a guitar, alternating down and up strokes | gap between notes, 8–98 ms |
| Slop | notes slightly off in time and level, like a human player | how loose |
| Arpeggio | up and down over two octaves while you hold the chord | rate: 1/4, 1/8, 1/8 triplet, 1/16, 1/32 |
| Pattern | the chord repeated in a rhythm | one of 4 rhythms |
| Arpa (harp) | a glissando over the chord notes, across octaves | gap between notes, 25–115 ms |

Arpeggio and Pattern follow the Rhythm page tempo. With the transport running (START) they lock to the
drum and looper grid.

## 7. Sound page
A list: **up/down** picks the row, **left/right** changes the value. Holding a direction keeps it
scrolling. **L1/R1** take big steps; on the Key row they jump by fifths. **Holding A** plays the I chord,
so you can hear changes as you make them.

| Group | Setting | What it does |
|---|---|---|
| | **Suono** (sound) | loads a factory sound |
| **Accordi** (chords) | Tonalità, Scala | key (C…B) and major/minor, for the Flip buttons and key mode |
| | Modo tonalità | MPK keys without pads play the chords of the scale |
| | Voicing, Apertura | see chapter 6 |
| | Esecuzione, Quantità | see chapter 6 |
| **Timbro** (timbre) | Motore (engine) | Analog, FM, Electric piano, Organ |
| | Tono (tone) | brightness: filter, FM index or harmonics |
| | Carattere (character) | depends on the engine (below) |
| | Attack, Decay, Sustain, Release | volume envelope |
| | Vibrato | vibrato; on the electric piano it becomes **Tremolo** |
| **Basso** (bass) | Level, Tone | mono bass playing the root (C2–B2); 0 turns it off |
| **Effetti** (effects) | Chorus, Delay, Riverbero | effects; the delay is ping-pong at a dotted eighth, synced to tempo |
| | Volume | master volume |

**Character** depends on the engine:

| Engine | What Character changes |
|---|---|
| Analog | up to halfway, detune between two saws; beyond that, a square wave comes in |
| FM | shown as **Rapporto** (ratio): 1, 2, 3, 4, 1/2, 3.5, 7 between modulator and carrier |
| Electric piano | how hard the saturation is driven |
| Organ | shown as **Registri** (drawbars): how many harmonics come in |

### Factory sounds

| # | Sound | Engine | Play mode |
|---|---|---|---|
| 0 | Lungolago | Analog | Chord |
| 1 | Tramonto FM | FM | Chord |
| 2 | Wurli di notte | Electric piano | Slop |
| 3 | Arpa di vetro | FM | Harp |
| 4 | Cascata | Analog | Arpeggio |
| 5 | Psichedelia | Analog | Slop |
| 6 | Organo della pieve | Organ | Chord |
| 7 | Ottoni | Analog | Strum |
| 8 | Ritmo in levare | Analog | Pattern |
| 9 | Campane lunari | FM | Strum |
| 10 | Chitarra di carta | Analog | Strum |
| 11 | Nebbia | Analog | Chord |

The number is the program change number. You can also step through sounds from the MPK, with bank B
pads 50 and 51.

A sound also includes the play mode, amount and spread, so changing sound changes those too. The volume
stays where you set it.

> **Note:** edits to the current sound are kept, even after quitting, until you load another sound.
> Loading one discards them: there is no way to save your own sounds yet.

## 8. Rhythm page
Three boxes at the top:
- the **tempo**, with "clock MIDI" when an external clock arrives;
- the **drums**, with the 16-step pattern and the current step;
- the **loop**, with its timeline, bars, chords (gold bars), notes (violet dots) and position.

Below them is the settings list:

| Setting | Values |
|---|---|
| Batteria (drums) | Yes / No; switching on starts the transport |
| Stile (style) | Disco, Psychedelic, Trap, Bossa nova |
| Livello (level) | drum volume |
| Tempo | 30–300 BPM |
| Segui il clock MIDI | follow MIDI clock: Yes / No |
| Lunghezza massima del loop | maximum loop length: 1, 2, 4, 8 bars |
| Quantizzazione del loop | loop quantize: No, 1/16, 1/8, 1/4 |

| Button | Function |
|---|---|
| up/down, left/right | pick and change |
| L1 / R1 | tempo −10 / +10, from any row |
| X | tap tempo: tap at least twice |
| A | loop: record / overdub |
| B | loop: play or stop |
| Y | loop: clear |

### The looper
It works like a looper pedal and records what you play: chords and notes. The loop plays with the
current sound, so changing sound or knobs changes it too.

1. **Record** (A, or pad 45). With the transport stopped it starts at once. While running it waits for
   the next bar; pressing just after a downbeat starts from that bar.
   - The hi-hat plays a click while recording.
   - Pressing again before it starts cancels.
2. **Record again** to close the loop: the length rounds to the nearest bar and the loop starts playing.
   At the maximum length the loop closes by itself.
3. While it plays, **Record** switches to **overdub**, to add more on top. Press again to go back to
   listening.
4. **Play/stop** (B, or pad 46) stops and restarts it.
5. **Clear** (Y, or pad 47) empties it.

Quantize aligns what you record to the chosen grid. The loop holds up to 1024 events; beyond that
"pieno: niente altre note" (full) appears. The loop and your live playing each have their own chord; the
bass follows the most recent chord.

## 9. MIDI page
It lists every control OpenOrc receives from the keyboard and what it is assigned to, in four groups:
chord pads, function pads, knobs, transport buttons.

| Button | Function |
|---|---|
| up/down | pick a row |
| L1 / R1 | jump to the previous or next group |
| A on a row | **learn**: the row shows "in ascolto..." (listening), then press or move the control |
| A on a group header | learn the whole group, one control after another |
| B | cancel learning |
| X | clear the row's assignment |
| Y | load the next **keyboard preset** |
| R2 | save the current map to `Saves/openorc/tastiere/mia-tastiera.txt` |
| left/right or A on "Tipo di manopole" | knob type: Absolute, Relative, Relative (64) |

- At the bottom, **Ultimo messaggio** (last message) shows the last message received, with note or CC,
  channel and port. It is the quickest way to see what a keyboard sends.
- A control assigned to a function is removed from the one that used it before: no duplicates.
- Knobs learn only CCs. Pads learn notes or CCs.
- If no keyboard preset works, Y falls back to the MPK mini IV factory map.

**Knob types:**
- **Absolute:** the knob sends 0–127 and the setting jumps to its position.
- **Relative:** 1–63 goes up, 65–127 goes down. This is the MPK mini IV's type.
- **Relative (64):** 64 is still; above goes up, below goes down.

Keyboard presets are readable text files. They are looked up in `Saves/openorc/tastiere/` (yours, kept
across updates) and in the app's `tastiere` folder. The format is described in the
[MPK sheet](tastiere/akai-mpk-mini-iv.md).

## 10. Files

| File | Contents |
|---|---|
| `Saves/openorc/stato.txt` | settings and current sound; delete it to go back to factory values |
| `Saves/openorc/tastiere/*.txt` | your keyboard presets |
| `App/OpenOrc/tastiere/` | MPK mini IV preset and sheet |
| `Saves/spruce/openorc.log` | messages from the last run, useful if something goes wrong |

## 11. Troubleshooting

| Problem | What to do |
|---|---|
| No sound | Check the console volume and the Volume row (or K8). If the oscilloscope moves, there is sound. |
| A note hangs | **L3**, or press and release the pedal. |
| The keyboard does nothing | MIDI page: the light must be green. If "Ultimo messaggio" changes, messages arrive: learn the controls again. |
| MPK chords sound odd or doubled | Turn off Chords, Scales, ARP and Note Repeat on the MPK. |
| A knob runs backwards or in jumps | Try another knob type on the MIDI page. |
| Arpeggio or pattern drifts against another device | Send it MIDI clock and turn on "Segui il clock MIDI". |
| The tempo will not change | A MIDI clock is arriving and sets the tempo. |

## 12. Current limits
- You cannot save your own sounds; loading a sound discards edits to the previous one.
- Sounds change one step at a time, with no list to pick from.
- The loop is not saved and has no real undo: Undo clears the whole loop.
- The four drum styles are fixed.
- From the Play page you cannot change key or sound with the Flip's buttons alone.
