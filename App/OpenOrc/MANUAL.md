# OpenOrc — manual

*English · [Italiano](MANUAL.it.md)* · **[Quick reference](QUICKREF.md)**: every control on one page, to keep in view while you play.

OpenOrc is a chord synthesizer for the Miyoo Flip, inspired by the Orchid by Telepathic Instruments. You
play it with the Flip's buttons or with a USB MIDI keyboard. It is designed for the **Akai MPK mini IV**,
but it learns the controls of any keyboard. One key (or button) plays a whole chord, with a bass line, in
the chosen style: block chord, strum, arpeggio, harp and more. It has six sound engines, 25 factory sounds
in stereo, your own saved sounds, drums and a layered looper.

The app itself is in Italian; this manual gives the on-screen names in Italian with a translation. It
describes the `cirmolo` branch as of October 2026.

## At a glance

| You want to... | Do this |
|---|---|
| play right away | **Suona** (Play) page: the d-pad and A B X Y play the chords of the key |
| change sound on the fly | **hold SELECT** and press up/down; on the MPK, bank B pads 50 and 51 |
| change key on the fly | **hold SELECT** and press left/right |
| record a loop | **hold SELECT** and press A (or pad 45) |
| save a sound of your own | **Suono** (Sound) page, "Salva come nuovo suono" (save as new sound) row, **A** |
| change page | **tap SELECT** (Play → Chords → Sound → Rhythm → MIDI) |
| start drums and tempo-synced arpeggios | **START** |
| silence every note | **L3** (press the left stick) |
| quit | **MENU**, then A |

## 1. Starting, quitting and saving
- Open OpenOrc from Cirmolo's **App** area. While it is open the console does not go to sleep.
- **MENU** quits after a confirmation. **A** or down quits, **B** or up stays.
- Settings save themselves to `Saves/openorc/stato.txt` about 8 seconds after the last change and on
  exit: current sound, key, tempo, loop settings and MIDI map.
- The **loop** is saved on exit to `Saves/openorc/loop.txt`. Next time it is there, stopped: press B on
  the Rhythm page, or SELECT + B, to play it.
- **Your sounds** live in `Saves/openorc/suoni/`, one file each (chapter 8).
- The drums always start switched off.

## 2. Pages
The top bar shows the app name, the five tabs, the **MIDI** light and the transport symbol.
- The MIDI light is green when a keyboard is connected and blinks on every message.
- The transport symbol is a square when stopped. When running it is a gold triangle pulsing on the beat.

| Page | Purpose |
|---|---|
| **Suona** (Play) | playing: current chord, oscilloscope, buttons and pads, knobs |
| **Accordi** (Chords) | key and scale, voicing and spread, play mode |
| **Suono** (Sound) | choosing, saving and editing sounds: timbre, bass, stereo, effects |
| **Ritmo** (Rhythm) | tempo, drums, looper |
| **MIDI** | connecting and setting up the keyboard |

The bottom strip lists the page's controls.

### Controls on every page

| Button | Function |
|---|---|
| SELECT, tapped | next page |
| SELECT, held | sound list and quick functions (chapter 3) |
| START | start or stop the transport: drums, synced arpeggios and patterns, looper |
| MENU | quit, with confirmation |
| L3 (left stick pressed) | all notes off ("panic") |
| left stick, horizontal | pitch bend, up to ±2 semitones |
| left stick, up | vibrato (modulation) |
| right stick, vertical | brightness: up brighter, down darker |

## 3. Holding SELECT: sounds and functions on the fly
Hold **SELECT**: after a moment the **sound list** appears, with the current sound highlighted and its
category on the right. While you hold SELECT, the other buttons become quick functions. If you press
another button straight away there is no need to wait for the list.

| SELECT + | Function |
|---|---|
| up / down | previous / next sound |
| L1 / R1 | previous / next category (Atmosfere, Tastiere, Corde, Arpeggi, Campane, Ottoni, Tuoi = yours) |
| left / right | key one semitone down / up |
| L2 | major or minor scale |
| R2 | key mode on or off |
| A | loop: record / overdub |
| B | loop: play or stop |
| Y | loop: undo |
| X | drums on or off |

- The sound changes immediately: with the MPK you can keep playing while you scroll.
- The key is shown at the top right, the loop state at the bottom.
- When you let go of SELECT the list closes and you stay on the same page.
- It works on every page. With the Flip alone, it is how you change sound, key and loop without leaving
  the Play page.

## 4. Playing with the Flip (Play page)
Without a keyboard you play with the console's buttons. Each button plays a degree of the chosen key (C
major at first). On screen each button shows the name of the chord it would play now, laid out like the
console.

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

**R3** (right stick pressed) moves to the next play mode (chapter 7).

If you hold two buttons and release one, the other plays again: the last one pressed always wins.

## 5. Playing with the Akai MPK mini IV
Plug the keyboard into the Flip's USB-C port, even with OpenOrc already open: "Tastiera collegata"
(keyboard connected) appears. The MPK works with its user presets' factory values, with no setup: see
the [MPK sheet](tastiere/akai-mpk-mini-iv.md). Turn **Chords, Scales, ARP and Note Repeat** off on the
MPK: OpenOrc builds the chords itself.

With the keyboard connected, the Play page shows the 8 chord pads on the left with the 8 knobs below
them. The Flip's buttons still work, drawn smaller on the right.

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
| **bottom row** | Drums | Loop: record | Loop: play or stop | Loop: undo |

With pads 50 and 51 the new sound shows up briefly at the top, between the previous and the next one.

### Knobs

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| Voicing | Bass | Play amount | Tone | Chorus | Delay | Reverb | Volume |

The knobs appear on the Play page below the pads, as eight small bars: the one you just turned lights up
in gold, and its name and value show briefly. The MPK mini IV's knobs are relative, so values never jump.

### Other controls
- **Pitch wheel:** pitch bend, ±2 semitones.
- **Mod wheel:** vibrato.
- **Sustain pedal (CC 64):** holds chords and notes until you release it.
- **Program change:** picks the sound by its number in the chapter 8 table; your sounds come after the
  25 factory ones.
- **Transport buttons** (Play, Rec, Loop, Undo): start/stop, record, play or stop the loop, undo. On the
  MPK mini IV they may not respond: see the keyboard sheet.
- **MIDI clock:** when it comes from another device, OpenOrc follows its tempo ("Segui il clock MIDI"
  on the Rhythm page). Start and Stop messages start and stop the transport.

## 6. The Play page screen
- **Status line**, at the top: sound, play mode, key, tempo and drums, loop state.
  - An **asterisk** after the sound name means you changed it and have not saved it.
  - The key is gold when key mode is on.
  - The loop state is red while recording; "+2" means two overdubs on top of the first pass.
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

## 7. Chords page
A list: **up/down** picks the row, **left/right** changes the value, **L1/R1** take big steps (on the
key they jump by fifths). **Holding A** plays the I chord.

| Group | Row | What it does |
|---|---|---|
| **Tonalità** (key) | Tonalità, Scala | key (C…B) and major/minor, for the Flip buttons and key mode |
| | Modo tonalità | MPK keys without pads play the chords of the scale |
| **Voicing** | Voicing | chord register (like K1) |
| | Apertura (spread) | close, open or wide: how far apart the chord notes are |
| **Esecuzione** (play mode) | Esecuzione, Quantità | how the chord starts (table below) |
| | Cambia con il suono (changes with the sound) | **Sì**: each sound brings its own play mode, amount and spread. **No**: yours stay when you change sound |

**Voicing.** OpenOrc picks the chord inversion by itself, near the chosen register: left low, right
high. Moving between chords it picks the inversion that moves the voices least, so progressions sound
smooth.

**Play mode.** How the chord starts. The "Quantità" (amount, knob K3) depends on the mode:

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

## 8. Sound page
Same controls as the Chords page. In addition, on the **Suono** row left/right change sound and
**L1/R1** jump by category; on the rows of the Suono group, **A** runs the action.

### Choosing and saving sounds

| Row | What A does |
|---|---|
| **Suono** (sound) | the current sound (left/right to change it); the asterisk means it is edited |
| **Salva come nuovo suono** (save as new) | saves the sound as it is under a new name, shown on the right: "Nebbia" becomes "Nebbia 2" |
| **Aggiorna il tuo suono** (update yours) | saves the edits over your current sound (not for factory sounds) |
| **Ripristina** (restore) | drops the edits and goes back to the saved sound |
| **Variazione casuale** (random variation) | nudges timbre, envelope and effects; press again for another, Restore to go back |
| **Rinomina** (rename) | renames your sound, typed on a **USB keyboard** plugged into the Flip; Enter or A confirms, B cancels |
| **Elimina** (delete) | deletes your sound: press A twice |

To make a new sound: start from the closest one, change it on the Sound page (or with the knobs, or with
"Variazione casuale") and choose **Salva come nuovo suono**. Your sounds appear at the end of the list,
in the **Tuoi** (yours) category.

Edits to the current sound survive quitting: they stay, with the asterisk, until you load another sound.
Changing sound without saving discards them.

A sound includes timbre, bass, effects and (when "Cambia con il suono" is Sì) play mode, amount and
spread. The **volume** does not: it stays where you set it from one sound to the next.

### Timbre, bass and effects

| Group | Setting | What it does |
|---|---|---|
| **Timbro** (timbre) | Motore (engine) | Analog, FM, Electric piano, Organ, Plucked string, Ensemble |
| | Tono (tone) | brightness: filter, FM index, harmonics or pluck |
| | Carattere (character) | depends on the engine (below) |
| | Attack, Decay, Sustain, Release | volume envelope |
| | Vibrato | vibrato; on the electric piano it becomes **Tremolo** |
| **Basso** (bass) | Level, Tone | mono bass playing the root (C2–B2); 0 turns it off |
| **Effetti** (effects) | Ampiezza stereo (stereo width) | how wide the sound spreads left and right (below) |
| | Chorus, Delay, Riverbero | effects; the delay is ping-pong at a dotted eighth, synced to tempo |
| | Volume | master volume |

| Engine | What it is | What Character changes |
|---|---|---|
| Analogico (analog) | two sawtooth oscillators with a filter | up to halfway, detune; beyond that, a square wave comes in |
| FM | two operators, from electric piano to bells | shown as **Rapporto** (ratio): 1, 2, 3, 4, 1/2, 3.5, 7 |
| Piano elettrico (electric piano) | FM with saturation, Rhodes and Wurlitzer style | how hard the saturation is driven |
| Organo (organ) | summed harmonics, like drawbars | shown as **Registri** (drawbars): how many harmonics come in |
| Corda pizzicata (plucked string) | a simulated real string (Karplus-Strong): guitar, harp, koto | shown as **Durata** (length): how long the string rings, 0.25 to 8 seconds |
| Ensemble | five detuned oscillators, wide in stereo: strings, pads, brass | shown as **Desintonia** (detune): how far apart the oscillators are |

With the plucked string, **Tone** sets how bright the pluck is and how long the high harmonics last. To
let the string ring as long as it vibrates, keep **Sustain** at maximum.

**Stereo width.** At 0 everything is in the centre. Turning it up spreads the chord notes from the lowest
on the left to the highest on the right, and arpeggios, strums and harp sweeps travel across. Also:
- the **Ensemble** also spreads its oscillators, like a wide choir;
- on the **Electric piano** the tremolo pans from side to side, like suitcase pianos;
- the bass always stays in the centre.

### Factory sounds
The number is the program change number. The sounds added in October 2026 are in stereo.

| # | Sound | Category | Engine | Play mode |
|---|---|---|---|---|
| 0 | Lungolago | Atmosfere | Analog | Chord |
| 1 | Nebbia | Atmosfere | Analog | Chord |
| 2 | Psichedelia | Atmosfere | Analog | Slop |
| 3 | Aurora | Atmosfere | Ensemble | Chord |
| 4 | Archi d'inverno | Atmosfere | Ensemble | Chord |
| 5 | Respiro di vetro | Atmosfere | FM | Slop |
| 6 | Tramonto FM | Tastiere | FM | Chord |
| 7 | Wurli di notte | Tastiere | Electric piano | Slop |
| 8 | Valigia stereo | Tastiere | Electric piano | Chord |
| 9 | Organo della pieve | Tastiere | Organ | Chord |
| 10 | Organo rotante | Tastiere | Organ | Chord |
| 11 | Chitarra di carta | Corde | Analog | Strum |
| 12 | Dodici corde | Corde | Plucked string | Strum |
| 13 | Koto del lago | Corde | Plucked string | Slop |
| 14 | Arpa celtica | Corde | Plucked string | Harp |
| 15 | Arpa di vetro | Corde | FM | Harp |
| 16 | Pizzicato | Corde | Plucked string | Chord |
| 17 | Cascata | Arpeggi | Analog | Arpeggio |
| 18 | Pioggia | Arpeggi | Plucked string | Arpeggio |
| 19 | Ritmo in levare | Arpeggi | Analog | Pattern |
| 20 | Treno notturno | Arpeggi | Ensemble | Pattern |
| 21 | Campane lunari | Campane | FM | Strum |
| 22 | Carillon | Campane | FM | Arpeggio |
| 23 | Ottoni | Ottoni | Analog | Strum |
| 24 | Fanfara | Ottoni | Ensemble | Strum |

### Your sound files
Each sound of yours is a text file in `Saves/openorc/suoni/`, for example `nebbia-2.txt`:

```
nome=Nebbia 2
engine=0
tone=0.3
...
stereo=0.5
spread=2
```

You can copy them to another console, share them and tweak them in a text editor. OpenOrc reads them at
start-up and keeps up to 64.

## 9. Rhythm page
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
| Y | loop: undo |

### The looper
It works like a looper pedal and records what you play: chords and notes. The loop plays with the
current sound, so changing sound or knobs changes it too. The same controls are on SELECT + A, B, Y and
on pads 45, 46, 47.

1. **Record** (A). With the transport stopped it starts at once. While running it waits for the next
   bar; pressing just after a downbeat starts from that bar.
   - The hi-hat plays a click while recording.
   - Pressing again before it starts cancels.
2. **Record again** to close the loop: the length rounds to the nearest bar and the loop starts playing.
   At the maximum length the loop closes by itself.
3. While it plays, **Record** switches to **overdub**, to add more on top. Press again to go back to
   listening. Each overdub is a separate **layer**.
4. **Play/stop** (B) stops and restarts it.
5. **Undo** (Y) steps back, like a looper pedal:
   - during an overdub, it removes the one in progress;
   - then it removes overdubs, from the last to the first;
   - with only the first pass left, it clears the loop.

Quantize aligns what you record to the chosen grid. The loop holds up to 1024 events; beyond that
"pieno: niente altre note" (full) appears. The loop and your live playing each have their own chord; the
bass follows the most recent chord. On exit the loop is saved and next time you find it stopped; an
overdub left half-way is not saved.

## 10. MIDI page
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
[MPK sheet](tastiere/akai-mpk-mini-iv.md). Presets written before October 2026 with a "Loop: cancella"
line still work: it is now called "Loop: annulla".

## 11. Files

| File | Contents |
|---|---|
| `Saves/openorc/stato.txt` | settings and current sound; delete it to go back to factory values |
| `Saves/openorc/loop.txt` | the loop from the last session |
| `Saves/openorc/suoni/*.txt` | your sounds |
| `Saves/openorc/tastiere/*.txt` | your keyboard presets |
| `App/OpenOrc/tastiere/` | MPK mini IV preset and sheet |
| `Saves/spruce/openorc.log` | messages from the last run, useful if something goes wrong |

## 12. Troubleshooting

| Problem | What to do |
|---|---|
| No sound | Check the console volume and the Volume row (or K8). If the oscilloscope moves, there is sound. |
| A note hangs | **L3**, or press and release the pedal. |
| The keyboard does nothing | MIDI page: the light must be green. If "Ultimo messaggio" changes, messages arrive: learn the controls again. |
| MPK chords sound odd or doubled | Turn off Chords, Scales, ARP and Note Repeat on the MPK. |
| A knob runs backwards or in jumps | Try another knob type on the MIDI page. |
| Arpeggio or pattern drifts against another device | Send it MIDI clock and turn on "Segui il clock MIDI". |
| The tempo will not change | A MIDI clock is arriving and sets the tempo. |
| Changing sound also changes the arpeggio | Chords page, "Cambia con il suono": No. |
| Rename types nothing | It needs a USB keyboard plugged into the Flip; without one the automatic name stays. |
| The plucked string dies at once | Raise Character (Length) and keep Sustain at maximum. |

## 13. Current limits
- The four drum styles are fixed.
- There is a single loop; you cannot keep several named loops.
- Volume is not part of a sound: a sound saved quieter or louder does not remember its volume.
- A sound's name can only be typed on a USB keyboard.
