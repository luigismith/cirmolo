# Cirmolo Synth — manual

*English · [Italiano](MANUAL.it.md)* · Screenshots of every page are on the [illustrated page](https://luigismith.github.io/cirmolo/synth/) of the website (Italian).

Cirmolo Synth is a groovebox for the Miyoo Flip: a drum machine with a powerful sequencer, an acid bass and a melody synth, all in one console. It uses no samples: every sound is synthesized on the fly. The interface is in Italian; the names in brackets below are what you see on screen. You find it on the Home screen, under **Music** (*Musica*).

**14 tracks** play together:

- **12 drum tracks**, one instrument each: kicks, snares, claps, hi-hats, cymbals, toms, congas, cowbells, 8-bit sounds and more. Each track uses one of **34 instrument models**; a **kit** (there are **26**, from the 808 to the Game Boy) picks the 12 instruments and tunes them together;
- **Acid** (*Acido*), a monophonic 303-style bass: saw or square, resonant 4-pole filter, accent and slide;
- **Piega** ("fold"), a 4-voice West Coast synth: a complex oscillator with a wavefolder and a low-pass gate, for soft or percussive notes.

Every track has up to 64 steps with its own length and speed; every step can have a probability, a condition, ratchets, micro-timing and locked values. There are 16 patterns you can chain, **33 ready-made grooves**, a mixer with delay and reverb, a **Live** page where the Flip's 12 buttons become pads, a DJ filter and stutter on the sticks, recording and WAV export. With a USB MIDI keyboard or pad controller you can play and tweak everything from the controller, and the Flip sends MIDI clock to connected instruments.

This manual describes the version on the `cirmolo` branch, October 2026.

## At a glance

| You want to... | Do this |
|---|---|
| hear something right away | **START**: on first launch pattern 1 already has a house beat, an acid line and a few Piega notes |
| change page | **SELECT** (Rhythm → Sounds → Mixer → Live → Pattern → Rhythm) |
| load another groove | **Pattern** page, right until the **ready-made grooves** list (*Ritmi pronti*), pick one and press **A** |
| turn a step on or off | **Rhythm** page (*Ritmo*), D-pad to the step, **A** |
| change how hard a step hits | **Rhythm** page, hold **B** + up/down |
| edit a step in detail | **Rhythm** page, **X** opens the step sheet |
| change kit | **Sounds** page (*Suoni*), **L2/R2** |
| play the drums with the buttons | **Live** page: the 12 buttons are 12 pads |
| DJ filter, stutter, fill | left stick sideways; right stick up (stutter) or down (fill), on every page |
| record what you hear | **R3** (click the right stick); R3 again stops |
| export the pattern to WAV | **Pattern** page, **R1** |
| quit | **MENU**, then A |

## 1. Starting, quitting and saving

- While Cirmolo Synth is open the console does not go to sleep.
- Quit with **MENU**: it asks "Quit the synth?" (*Uscire dal synth?*). **A** or down quits, **B** or up stays.
- Everything saves by itself: sounds, kit, mixer, the 16 patterns, the chain and the page you were on. It saves on exit and, if you changed something, every 30 seconds. Next time you find everything as you left it, stopped.
- On first launch (or if the state file is missing) pattern 1 holds the "Cassa dritta house" groove (four-on-the-floor house) with the House kit at 124 BPM, a 16-step Acid line and four long Piega notes. The other 15 patterns are empty.
- The state of the old Cirmolo Synth (the previous eight-voice synthesizer) is not imported: the first time, the new synth starts from scratch and replaces it.

## 2. Pages and common controls

The five pages are at the top; the active one is highlighted. On the right, **P1 124** tells you which pattern is playing and the tempo; the green dot pulses on every beat while playing, the grey square means stopped. The bottom line always lists the page's buttons.

| Page | What it is for |
|---|---|
| **Rhythm** (*Ritmo*) | the grid: 14 tracks by 16 steps, the sheet of each step |
| **Sounds** (*Suoni*) | the kit and the instrument parameters of each track |
| **Mixer** | volume, pan, delay and reverb sends, mute and solo; the master with tempo, swing and effects |
| **Live** | the Flip's 12 buttons play, record or mute the 12 drum tracks |
| **Pattern** | 16 patterns, chain, copy and paste, ready-made grooves, WAV export |

### Controls on every page

| Button | What it does |
|---|---|
| **SELECT** | next page |
| **START** | play or stop; always starts from the beginning of the pattern (or chain) |
| **R3** (click the right stick) | records the output to WAV; R3 again stops (chapter 10) |
| **L3** (click the left stick) | stops all notes; on the Live page it changes mode |
| **MENU** | quits, with confirmation |
| left stick, sideways | DJ filter (chapter 9) |
| right stick up / down | stutter / fill (chapter 9) |

The selected track is shared by Rhythm, Sounds and Mixer: move to the Acid row in the grid and Sounds already shows the Acid.

## 3. First steps

1. Open Cirmolo Synth and press **START**: pattern 1 plays. Press START again to stop.
2. With the music running, press **SELECT** until the **Pattern** page. Press **right** until the focus moves to the **ready-made grooves** list on the right, pick a groove with up/down (the number next to it is the tempo) and press **A**: the groove goes into the pattern you are editing, with its kit, tempo and swing. Left goes back to the pattern grid.
3. **SELECT** to **Rhythm**: you see the groove's steps. Move the cursor with the D-pad and press **A** to turn a step on or off. Hold **B** and press up or down to change how hard it hits.
4. Go down to the **Acido** row, turn on a few steps with **A** and, holding **B**, raise or lower the note (up/down) or the octave (left/right).
5. On the **Live** page play over the beat with the buttons and try the sticks: the left one filters, the right one stutters (up) or fills (down).
6. When you like it, **R3** records what you hear; on the Pattern page **R1** exports the pattern to WAV.

## 4. Rhythm page (*Ritmo*)

At the top: the number of the pattern you are editing, its name (the groove's, if you loaded one), the page of steps (for example 1/2) and the kit. If another pattern is playing you see "(another one is playing)" (*suona un altro*).

### Reading the grid

One row per track: the 12 drums, a line, then Acid and Piega. On the left the instrument name, with a dot that lights up on every hit (grey if the track is muted). The columns are 16 steps in groups of four; the brighter column is the one playing, the amber frame is the cursor.

- The **colour** tells the kind of instrument: red kicks, amber snares, rims and claps, light blue hi-hats, cymbals, shakers and tambourines, green toms, congas, bongos and tabla, pink cowbells, claves and wood blocks, violet the effects (zap, glitch, noise sweep). Acid is lime, Piega violet.
- The **brightness** of an active step is its velocity: the brighter, the louder. On the melodic tracks an accented step is even brighter.
- **Dots** at the bottom: ratchets (2, 3 or 4 hits in the step).
- **Clipped corner** top left: probability below 100%.
- **Dot** top right: there is a condition; amber if it is "Fill only".
- **Bar** at the bottom: the step has locked values.
- **Tick** at the top, off centre: the step is nudged early or late.
- On **Acid** and **Piega** the step shows the note name; a dash on the right is a slide (Acid), a small bar on the right a note longer than one step (Piega).
- Darker cells are past the end of the track. If a track is not 16 steps long, or runs at another speed, its length appears on the right of the row with a sign: **»** double speed, **«** half, **³** triplets.

The grid shows as many 16-step pages as the longest track of the pattern needs (up to 4, that is 64 steps).

### Buttons

| Button | What it does |
|---|---|
| D-pad | moves the cursor (step and track); wraps around at the edges |
| **A** | turns the step on or off; when stopped it also plays the sound |
| hold **B** + D-pad | quick edit of the step (below) |
| **X** | opens the step and track sheet |
| **Y** | plays the track (on melodic tracks, the step's note) |
| L1 / R1 | previous / next page of 16 steps |
| L2 / R2 | previous / next pattern to edit |

Turning on an Acid or Piega step puts in the last note you used on that track. If you turn on a step past the end of the track, the track grows to the end of that page.

Without a chain, the pattern you edit is also the one that plays: switching pattern with L2/R2 while playing takes effect at the end of the current loop.

### Hold B + D-pad

The fastest way to work on steps. If the step is off, it turns it on.

| | Drums | Acid and Piega |
|---|---|---|
| B + up / down | louder / softer | note up / down a semitone |
| B + right / left | one more / one fewer ratchet (1 to 4) | an octave up / down |

While you hold B the bottom line reminds you what the D-pad does.

### The step sheet (X)

**X** opens the sheet of the step under the cursor on the right, with the values of the whole track at the bottom.

| Button | What it does |
|---|---|
| up / down | picks the row |
| left / right | changes the value |
| L1 / R1 (pressed, or held with left/right) | changes in big steps |
| **A** | runs the rows marked (A); on Accent and Slide toggles them |
| **Y** | plays the step |
| **B** or **X** | closes the sheet |

Changing a step value turns the step on if it was off (except Nudge).

**Step rows**

| Row | Values | What it does |
|---|---|---|
| Note (*Nota*) | C1 to C7 (Acid and Piega) | the step's note; L1/R1 an octave |
| Accent (*Accento*) | yes / no (Acid and Piega) | louder note; on the Acid it also opens the filter, as much as "Accent" in Sounds says |
| Slide to next step (*Slide verso il passo dopo*) | yes / no (Acid) | the note glides legato into the next one, as on the 303 |
| Length in steps (*Durata (passi)*) | 1 to 16 (Piega) | how many steps the note lasts |
| Velocity (*Dinamica*) | 1 to 127 | how hard it hits (steps of 4; L1/R1 16) |
| Probability (*Probabilità*) | 1 to 100% | how many times in a hundred the step plays (steps of 5; L1/R1 25) |
| Condition (*Condizione*) | see below | which loops the step plays in |
| Ratchets (*Ripetizioni*) | 1 to 4 | fast hits inside the step |
| Nudge (*Spostamento*) | −50% to +50% | moves the step early (−) or late (+), as a percentage of a step |
| Pitch here only (*Altezza solo qui*) | ±24 semitones (drums) | tuning of this hit only |
| Decay here only (*Durata solo qui*) | 0–100% | decay of this hit only |
| Tone here only (*Colore solo qui*) | 0–100% | the tone of this hit; on the Acid it becomes **Cutoff here only** (*Taglio solo qui*, the filter), on Piega **Folds here only** (*Pieghe solo qui*) |

The three "here only" values start at "as the track" (*come la traccia*): the first press right sets a value (0 semitones, or 50%), and going below the minimum returns to "as the track".

**Track rows** (under the word *Traccia*, they apply to the whole row of the pattern)

| Row | What it does |
|---|---|
| Length (*Lunghezza*) | 1 to 64 steps (L1/R1 by 16) |
| Speed (*Velocità*) | normal (sixteenths), double (32nds), half (eighths), triplets (12 steps per bar) |
| Euclidean: hits (*Euclideo: colpi*) | how many hits to spread over the track length |
| Euclidean: rotation (*Euclideo: rotazione*) | how many steps to shift them right |
| Apply Euclidean (A) (*Applica l'euclideo*) | rewrites the track: the hits as evenly spread as possible, with normal values |
| Shift all (left/right) (*Sposta tutto*) | rotates all the track's steps one step left or right |
| Clear track (A) (*Svuota la traccia*) | turns off all the track's steps and resets them to normal values |

### What they are for

- **Probability.** A hi-hat at 50% plays one loop yes and one no, at random: the beat never repeats exactly. Low probabilities on ghost notes give a living groove with no effort.
- **Conditions.** They decide which loops a step plays in:

  | Condition | The step plays... |
  |---|---|
  | Always (*Sempre*) | always |
  | Fill only (*Solo nel fill*) | only while you hold the fill (right stick down, or pad 13 of the MPK mini Plus) |
  | Not in fill (*Non nel fill*) | always, except while you hold the fill |
  | 1st of 2 loops (*1° giro su 2*) | on odd loops of the pattern |
  | 2nd of 2 loops (*2° giro su 2*) | on even loops |
  | 1 loop in 4 (*1 giro su 4*) | on the first loop of every four |
  | First loop only (*Solo il 1° giro*) | only on the first loop after starting (or after a pattern change) |

  Loops are counted from when the pattern started; moving to another pattern restarts the count (in a chain, to hear "2nd of 2 loops" put the same pattern twice in a row). With "1st/2nd of 2 loops" a one-bar pattern becomes a two-bar phrase; with "Fill only" you prepare drum rolls you hear only when you ask for them.
- **Ratchets.** They split the step into 2, 3 or 4 equal hits: trap hi-hat rolls, snare rolls.
- **Nudge.** A snare 10–20% late feels lazier, a hi-hat early more urgent. It goes beyond swing, which moves all the even sixteenths the same way.
- **Locked values** ("here only"). The same instrument changes from step to step: a tom rising over four steps, an open hi-hat that is longer only at the end of the bar, an Acid cutoff that opens on one note only.
- **Euclidean.** Spreads N hits over a length as evenly as possible: 3 in 8 is the tresillo, 5 in 8 the cinquillo, 5 or 7 in 16 give African and Latin rhythms. Rotation moves the start.
- **Different lengths.** Every track loops with its own length: a 12-step row against a 16-step one meets at the start again only every 48 steps, and the beat keeps changing. The pattern loop is the longest track among those with active steps: tracks that fit a whole number of times restart together, the others run free. The "Poliritmo 3 su 4" groove (3-against-4 polyrhythm) is an example.
- **Speed.** Double for 32nd-note rolls, half for a slow track lasting two bars with 16 steps, triplets for shuffle and jazz (swing does not affect triplets).

## 5. Sounds page (*Suoni*)

On the left the kit and the 14 tracks, on the right the parameters of the selected track, with the model name at the top and the output oscilloscope below.

| Button | What it does |
|---|---|
| up / down | picks the parameter |
| left / right | changes it; with **B** held in big steps |
| L1 / R1 | previous / next track |
| L2 / R2 | previous / next kit |
| **A** | plays the track |
| **Y** | variation (*Variazione*): nudges the sound's values a little, at random (not volume, pitch and pan) |

### Kits

A kit picks and sets up the 12 drum instruments. Changing kit replaces the 12 instruments with all their values, including the ones you had changed; it does not touch patterns, mixer, Acid and Piega. The kit applies to all 16 patterns. The full list is in chapter 13.

In the kits the tracks almost always follow the same order: 1 kick, 2 snare, 3 rim, 4 clap, 5 closed hi-hat, 6 open hi-hat, 7 low tom, 8 high tom, 9 cymbal or ride, 10–12 percussion. So a beat also works with another kit.

### Drum parameters

| Parameter | What it does |
|---|---|
| Instrument (*Strumento*) | one of the 34 models; changing it renames the track after the model and plays it |
| Pitch (*Altezza*) | ±24 semitones from the model's usual pitch |
| Decay (*Durata*) | from very short to very long, for that model |
| Tone (*Colore*) | filter and brightness, or the balance between body and noise |
| Attack (*Attacco*) | the initial click, snap or pitch drop (the meaning depends on the model) |
| Drive (*Spinta*) | saturation |
| Volume | the instrument volume |
| Pan | left to right (it is the same pan as in the mixer) |
| Choke group (*Gruppo che zittisce*) | 1 to 4, or none (*nessuno*): tracks in the same group cut each other, like the closed hi-hat cutting the open one |

**The 34 models**

| Family | Models (Italian names) |
|---|---|
| Kicks | Cassa 808, Cassa 909, Cassa acustica (acoustic), Cassa FM, Cassa gabber |
| Snares | Rullante 808, Rullante 909, Rullante acustico (acoustic), Spazzola (brush) |
| Short hits | Rim, Battimani (clap), Schiocco di dita (finger snap) |
| Hi-hats and cymbals | Charleston chiuso / aperto (closed / open hi-hat), Charleston FM, Piatto crash, Piatto ride |
| Toms | Tom, Tom elettronico (electronic) |
| Skins | Conga, Bongo, Tabla |
| Percussion | Campanaccio (cowbell), Claves, Legnetto (wood block), Shaker, Tamburello (tambourine), Triangolo (triangle) |
| 8-bit | Cassa 8 bit, Rullante 8 bit, Charleston 8 bit |
| Effects | Zap, Glitch, Soffio (noise sweep) |

### Acid (*Acido*)

A 303-style monophonic bass. In the sequencer the slide matters: two active steps with a slide on the first become one note that glides.

| Parameter | What it does |
|---|---|
| Wave (*Onda*) | sawtooth (*Dente di sega*) or square (*Quadra*) |
| Pitch (*Altezza*) | ±24 semitones for the whole track |
| Cutoff (*Taglio*) | the frequency of the 4-pole filter |
| Resonance (*Risonanza*) | the filter's squelch |
| Envelope (*Inviluppo*) | how much the envelope opens the filter on every note |
| Decay (*Durata*) | how long the envelope lasts |
| Accent (*Accento*) | how much accents weigh, on filter and volume |
| Drive (*Spinta*) | saturation |
| Volume, Pan | |

### Piega

A 4-voice West Coast synth: an oscillator modulated by a second oscillator, folded by the wavefolder and closed by a low-pass gate. With few folds it is soft and round, with many it becomes bright and metallic; with a short gate it sounds like a tuned percussion.

| Parameter | What it does |
|---|---|
| Pitch (*Altezza*) | ±24 semitones |
| Folds (*Pieghe*) | how many times the wave folds back: the timbre |
| Symmetry (*Simmetria*) | at 50% the folds are symmetric; moving it makes the sound more nasal |
| Ratio (*Rapporto*) | the modulator's ratio to the note (0.5 to 8; in harmonic steps in the first part) |
| Modulation (*Modulazione*) | how much the modulator moves the oscillator |
| Decay (*Durata*) | how long the low-pass gate stays open |
| Gate colour (*Colore del gate*) | at 0 the gate changes only the volume, at 100% it also closes the filter (more "bongo") |
| Release (*Rilascio*) | how long the note goes on after the end of its steps |
| Volume, Pan | |

## 6. Mixer page

One strip per track (1–12, **Ac** the Acid, **Pi** Piega) and the **master** on the right. In each strip: the playing level (green; grey if the track cannot be heard because of a mute or solo), the volume fader, the pan (light blue), delay (violet) and reverb (pink) bars, the **M** (mute) and **S** (solo) buttons. At the top, the selected track and the value you are changing.

| Button | What it does |
|---|---|
| left / right | picks the strip; after **Pi** comes the master |
| L1 / R1 | on tracks picks the row: Volume, Pan, Delay (*Eco*), Reverb (*Riverbero*); on the master picks the parameter |
| up / down | changes the value; with **B** held in big steps |
| **A** | mute on/off |
| **Y** | solo on/off: if at least one track is soloed, only soloed tracks are heard |
| **X** | plays the track |

The mixer volume works on top of the instrument volume (Sounds page); the pan instead is the same on both pages.

### The master

| Parameter | What it does |
|---|---|
| Tempo | 40 to 240 BPM (B held: by 10) |
| Swing | 0 to 75%: delays the even sixteenths; at 33% they are triplets |
| Delay: time (*Eco: tempo*) | 1/16 to 8/16: tempo-synced ping-pong delay |
| Delay: feedback (*Eco: ritorni*) | how many times the delay comes back |
| Delay: tone (*Eco: colore*) | dark to bright |
| Reverb (*Riverbero*) | the size of the room |
| Damping (*Smorzamento*) | how much the reverb loses the highs |
| Compressor (*Compressore*) | compression of the whole mix |
| Pump (*Pompa*) | the mix "breathes" on the kick (track 1), like a sidechain |
| DJ filter (*Filtro DJ*) | the filter's starting point: "lows" (*bassi*, low-pass), "open" (*aperto*), "highs" (*alti*, high-pass); the left stick starts from here |
| Resonance (*Risonanza*) | the DJ filter's resonance |
| Lo-fi | reduces bits and sample rate: a dirty, old-sampler sound |
| Tape (*Nastro*) | saturation and a slight wobble, like a tape recorder |
| Volume | master volume |
| **MIDI clock out** (*Clock MIDI in uscita*) | yes / no (up or down to toggle): sends clock, Start and Stop to the connected MIDI device (chapter 11). On by default |

Tempo and swing apply to all patterns; loading a ready-made groove sets its values.

## 7. Live page

The Flip's 12 buttons become 12 pads, one per drum track, laid out like the console:

| Buttons | Tracks |
|---|---|
| D-pad left, down, right, up | 1, 2, 3, 4 (usually kick, snare, rim, clap) |
| Y, B, A, X | 5, 6, 7, 8 (closed and open hi-hat, low and high tom) |
| L2, L1, R1, R2 | 9, 10, 11, 12 (cymbal and percussion) |

Each pad shows its instrument and button and lights up on every hit, including the sequencer's. **L3** changes mode:

| Mode | The buttons... |
|---|---|
| **Play** (*Suona*) | play the instrument |
| **Play and record** (*Suona e registra*) | play and, while the sequencer runs, write the hit into the pattern that is playing, on the nearest step |
| **Mute** (*Zittisci*) | toggle the track's mute ("muto" appears on the pad) |

Top right: the pattern playing, the loop count and the kit. At the bottom: the sticks' DJ filter, stutter and fill, and the oscilloscope. The mode stays when you change page; in "Play and record" MIDI pads record too.

## 8. Pattern page

On the left the 16 patterns: number, name (if it comes from a ready-made groove) or "empty" (*vuoto*), and a coloured bar for each track with active steps. The violet pattern is the one you edit, the amber frame is the cursor, the green dot shows which one is playing. Below is the **chain** (*Catena*), on the right the list of **ready-made grooves** (*Ritmi pronti*).

| Button | What it does |
|---|---|
| D-pad | picks the pattern; right from the last column moves to the grooves |
| **A** | edit this pattern (without a chain it is also the one playing, from the end of the loop); if the chain is on, turns it off |
| **X** | adds the pattern to the chain |
| **L1** | removes the last pattern from the chain |
| **Y** | copies the pattern; then **Y** on another pattern pastes it |
| **B**, then **B** again | clears the pattern (the first B asks to confirm; any other button cancels) |
| **R1** | exports to WAV |

### The chain

With the chain on, patterns play in the chain's order, repeating, one loop each; the same pattern can appear several times (1 1 2 3 3 4). It can hold up to 64 patterns; the box shows the last 24, and the one playing is green. You can change it while playing: from the end of the loop it restarts from the first. With the chain on, L2/R2 on the Rhythm page only pick the pattern to edit, not the one playing. To turn it off: **A** on a pattern, or **L1** until it is empty.

### Copy, paste, clear

Copy takes all 14 tracks of the pattern and its name; pasting overwrites the destination pattern. Clearing turns off all steps, resets tracks to 16 steps at normal speed and removes the name. There is no undo: before clearing, copy the pattern into a free one.

### Ready-made grooves

The list has 33 grooves, with the tempo next to each (chapter 14). Up/down picks, L1/R1 jump by 10, **A** loads, left goes back to the patterns. Loading a groove:

- rewrites the **12 drum tracks** of the pattern you are editing (Acid and Piega stay) and gives it the groove's name;
- loads the suggested **kit**, which applies to all patterns;
- sets the groove's **tempo** and **swing**.

Every groove uses probabilities, conditions and ratchets: hold the fill (right stick down) to hear the rolls written for it.

### Exporting to WAV

**R1** exports without stopping the music, in a separate engine:

- **without a chain**: the pattern you are editing, for 4 loops;
- **with a chain**: one full pass of the chain.

Two seconds of tail are added for delay and reverb; at most 10 minutes. While exporting, the bottom line shows the seconds written so far; at the end the file name appears. The WAV is stereo, 48 kHz, 16-bit, in `Saves/cirmolo-synth/registrazioni/`, with names like `pattern-2026-10-13-213000.wav` or `catena-2026-10-13-213000.wav`. Mutes, solos and the mixer's DJ filter apply; the sticks do not.

## 9. The sticks: DJ filter, stutter, fill

They work on every page.

- **Left stick, sideways: DJ filter.** Left closes the low-pass (the lows remain), right opens the high-pass (the highs remain). Back in the centre, the filter returns to the mixer's value. The pitch wheel or joystick of a MIDI keyboard does the same.
- **Right stick up: stutter.** Repeats the last fragment of music as long as you hold it: a little up repeats a quarter note, then an eighth, a sixteenth and, all the way up, a 32nd. Let go and the music picks up where it would have been.
- **Right stick down (past halfway): fill.** While you hold it, "Fill only" steps play and "Not in fill" steps are silent.

The state of the sticks is shown at the bottom of the Live page.

## 10. Recording (R3)

**R3** (click the right stick) starts recording everything the synth plays, sticks included: a red bar with **REC** and the time appears at the bottom. **R3** again stops and saves. Files are stereo WAV at 48 kHz, 16-bit, in `Saves/cirmolo-synth/registrazioni/`, with names like `registrazione-2026-10-13-213000.wav`. On exit a recording in progress is closed automatically.

## 11. MIDI

Connect a USB MIDI keyboard or pad controller to the USB-C port, even with the app already open: "MIDI connected" (*MIDI collegato*) appears with the device name. The Flip's buttons keep working together with the controller.

| Message | What it does |
|---|---|
| Notes on **channel 10**, 36–47 | the 12 drum tracks in order: 36 track 1, 37 track 2... 47 track 12 (controller pads) |
| Other notes on channel 10 | as in General MIDI: 35 kick; 48 and 50 high tom (track 8); 49, 51, 52, 53, 55, 57, 59 cymbals (track 9); 56 cowbell (track 10); 54, 58 and 60–70 percussion (track 11); everything else track 12 |
| Notes on other channels | play **Acid** or **Piega**: whichever of the two you last selected on the Rhythm or Sounds page (Acid at startup) |
| Velocity | how hard the hit or note is |
| Knobs **CC 70–77** | the first eight continuous values of the selected track (below) |
| Knobs **CC 24–31** | the same, also with endless encoders (relative values) |
| **Pitch bend** | DJ filter, like the left stick |
| **Modulation (CC 1)** | stutter, like the right stick up |
| **Program change** | picks the kit: 0 is 808 Classica, 1 909 Club... in the order of chapter 13 |
| **Start**, Continue / **Stop** | start (from the beginning) and stop the sequencer |
| CC 120 and 123 | stop all notes |

**The knobs** control the selected track (on Rhythm, Sounds or Mixer) and, while you turn them, the Sounds page shows the parameter that changes. Only continuous values count, in the order of the Sounds page:

| Knob | Drums | Acid | Piega |
|---|---|---|---|
| 1 | Pitch | Pitch | Pitch |
| 2 | Decay | Cutoff | Folds |
| 3 | Tone | Resonance | Symmetry |
| 4 | Attack | Envelope | Ratio |
| 5 | Drive | Decay | Modulation |
| 6 | Volume | Accent | Decay |
| 7 | Pan | Drive | Gate colour |
| 8 | Reverb (send) | Volume | Release |

With CC 70–77 the knob sets the value to its position. With CC 24–31 the synth works out whether the controller sends absolute positions or increments (encoders): as long as only values close to 0 or 127 arrive it treats them as increments, at the first value in between it switches to positions.

Notes played from the keyboard on Acid and Piega are not recorded into the pattern; drum pads are, if the Live page is in "Play and record" and the sequencer is running.

If the device has several MIDI ports, notes and pads come from the first; from the others only knobs and Start/Stop are read.

### MIDI clock out

With **MIDI clock out** on (in the mixer's master; on by default) and a device connected, the Flip is the conductor:

- START on the Flip sends MIDI **Start** and then **clock** at 24 pulses per beat, at the synth's tempo, without swing;
- stopping with START sends **Stop**.

The clock starts and stops with the sequencer and follows tempo changes. The Flip does not follow an external clock: Start and Stop received make it start and stop, but the tempo is always its own. A received Start or Stop is not echoed back.

## 12. Akai MPK mini Plus

Cirmolo Synth recognises the **MPK mini Plus** by its name and gives it a few extra functions. Everything else works as with any controller (chapter 11).

| MPK control | What it does |
|---|---|
| Pads 1–12 (notes 36–47, channel 10) | the 12 drum tracks |
| **Pad 13** (note 48, channel 10), held | **fill** |
| **Pad 14** (note 49), held | **stutter 1/8** |
| **Pad 15** (note 50), held | **stutter 1/16** |
| **Pad 16** (note 51), held | **stutter 1/32** |
| Keys | Acid or Piega |
| **Joystick** (pitch bend) | DJ filter |
| **Modulation wheel** | stutter, finer the higher you push it |
| Knobs | the eight parameters of the selected track |

With the MPK mini Plus, notes 48–51 on channel 10 are used for fill and stutter and do not play toms and cymbals. If you hold several stutter pads, the finest wins. The pads must send on channel 10.

**Clock.** With MIDI clock out on, the Flip gives the tempo to the MPK:

- the MPK's **arpeggiator** and **sequencer**, set to external clock (*External*), follow the Flip's tempo and start with START;
- the MPK's **Clock Out** jack passes the clock on to analog synths and drum machines: the MPK becomes a bridge between the Flip and instruments that only have a clock input.

> **Not yet checked on the real keyboard.** The **knob numbers** and the **transport buttons** (Play and Stop) of the MPK mini Plus have not been checked on an actual MPK mini Plus yet. The synth answers knobs on CC 70–77 or 24–31 and MIDI Start and Stop messages: if the MPK sends other numbers or other messages, the knobs and transport buttons do nothing. The rest (pad 13 and joystick, clock out) is covered by the automated tests on the PC; how the MPK's arpeggiator, sequencer and Clock Out jack follow the clock depends on the MPK's own settings.

## 13. The 26 kits

L2/R2 on the Sounds page cycles them in this order; MIDI program change picks them by number (from 0).

| No. | Kit | Character |
|---|---|---|
| 0 | 808 Classica | the hip hop and electronic drum machine: long kick, cowbell, conga |
| 1 | 909 Club | house and techno: punchy kick, metallic hi-hats, ride and crash |
| 2 | 606 Tic | small and dry, for acid house |
| 3 | CR-78 Vintage | the seventies rhythm box: bongos, maracas, tambourine |
| 4 | Linn 80 | eighties sampled acoustic kit |
| 5 | DMX Hip hop | dirty, heavy acoustic, with finger snap |
| 6 | Simmons 80 | electronic toms, FM kick, zap |
| 7 | Trap | long 808, bright hi-hats, noise sweep |
| 8 | Techno industriale | industrial techno, everything driven: metal, glitch, zap |
| 9 | House | softer 909, conga, shaker, tambourine |
| 10 | Jungle | high-tuned acoustic, ghost snare, 808 sub |
| 11 | Lo-fi | muted, dark acoustic, wood block, crackle |
| 12 | Chiptune NES | 8-bit: noise, blip, glitch |
| 13 | Game Boy | brighter 8-bit: beep, laser, bit |
| 14 | Jazz spazzole | jazz brushes: brushes, cross stick, ride and ride bell, splash |
| 15 | Rock acustica | acoustic rock: full kick and snare, three toms, crash and ride |
| 16 | Latina | Latin: timbale, tumba, conga, bongo, clave, guiro, cascara |
| 17 | Afro | djembe, bell, shekere, talking drum |
| 18 | Bossa e samba | surdo, caixa, tamborim, agogo, pandeiro, cuica |
| 19 | Dembow | 808 and timbal for reggaeton |
| 20 | Elettro | electro 808 with electronic toms and zap |
| 21 | IDM glitch | FM kick, glitch, bit, zap, high wood blocks |
| 22 | Minimal | a few short, precise sounds |
| 23 | Dub | soft kick, high snare, deep toms, siren |
| 24 | Gabber | distorted gabber kick, everything driven |
| 25 | Ambient | long, soft sounds: tabla, triangle, noise sweep |

## 14. The 33 ready-made grooves

Each groove loads its kit, tempo and swing.

| No. | Groove | BPM | Kit | Swing |
|---|---|---|---|---|
| 1 | Cassa dritta house (four-on-the-floor house) | 124 | House | 10% |
| 2 | Techno | 132 | Techno industriale | — |
| 3 | Boom bap | 90 | DMX Hip hop | 18% |
| 4 | Trap | 140 | Trap | — |
| 5 | Drill | 142 | 808 Classica | — |
| 6 | Amen jungle | 165 | Jungle | — |
| 7 | Drum'n'bass | 174 | Jungle | — |
| 8 | Breakbeat | 128 | Linn 80 | 8% |
| 9 | Elettro | 128 | Elettro | — |
| 10 | Disco | 120 | 909 Club | — |
| 11 | Funk | 100 | CR-78 Vintage | 12% |
| 12 | Rock | 110 | Rock acustica | — |
| 13 | Shuffle | 96 | Rock acustica | — |
| 14 | Bossa nova | 135 | Bossa e samba | — |
| 15 | Samba | 100 | Bossa e samba | 12% |
| 16 | Dembow | 96 | Dembow | — |
| 17 | Afrobeat | 110 | Afro | 10% |
| 18 | Reggae one drop | 75 | Dub | 18% |
| 19 | Dub steppers | 136 | Dub | 8% |
| 20 | Jazz swing | 140 | Jazz spazzole | — |
| 21 | Lo-fi hip hop | 82 | Lo-fi | 22% |
| 22 | Chiptune | 150 | Chiptune NES | — |
| 23 | Game Boy | 160 | Game Boy | — |
| 24 | Minimal | 124 | Minimal | 6% |
| 25 | IDM | 150 | IDM glitch | — |
| 26 | Gabber | 180 | Gabber | — |
| 27 | Ambient | 80 | Ambient | 10% |
| 28 | Poliritmo 3 su 4 (3-against-4 polyrhythm) | 120 | Simmons 80 | — |
| 29 | Half time | 140 | Linn 80 | — |
| 30 | Footwork | 160 | 808 Classica | — |
| 31 | Latina: son e tumbao | 100 | Latina | — |
| 32 | Acid house | 125 | 606 Tic | 8% |
| 33 | Garage 2-step | 132 | 909 Club | 20% |

"Amen jungle" is 32 steps long (two pages); "Shuffle" and "Jazz swing" are written in triplets; "Trap" and "IDM" have double-speed hi-hats; "Poliritmo 3 su 4", "Minimal", "IDM" and "Ambient" have tracks of different lengths.

## 15. All the buttons, page by page

**On every page:** SELECT next page · START play/stop · R3 record · L3 stop notes (on Live: mode) · MENU quit · left stick DJ filter · right stick up stutter, down fill.

| Button | Rhythm | Step sheet (X) | Sounds | Mixer | Live | Pattern |
|---|---|---|---|---|---|---|
| D-pad | cursor | up/down row, left/right value | up/down parameter, left/right value | left/right strip, up/down value | pads 1–4 | picks the pattern; right → grooves |
| A | step on/off | runs (A) | play | mute | pad 7 | edit the pattern (turns off the chain) |
| B | held + D-pad: quick edit | closes | held: big steps | held: big steps | pad 6 | twice: clear |
| X | opens the sheet | closes | — | play | pad 8 | add to chain |
| Y | play | play | variation | solo | pad 5 | copy / paste |
| L1 / R1 | page of steps | value in big steps | track | row or master parameter | pads 10 / 11 | L1 removes from chain · R1 exports WAV |
| L2 / R2 | pattern to edit | — | kit | — | pads 9 / 12 | — |
| L3 | stop notes | stop notes | stop notes | stop notes | change mode | stop notes |

In the grooves list: up/down picks, L1/R1 jump by 10, A loads, left goes back to the patterns.

## 16. Files

- `Saves/cirmolo-synth/stato.txt`: sounds, kit, mixer, master, the 16 patterns and the chain (rewritten automatically).
- `Saves/cirmolo-synth/registrazioni/`: recordings (`registrazione-...wav`) and exports (`pattern-...wav`, `catena-...wav`).
- `Saves/spruce/cirmolo-synth.log`: the app log.
- The sources are in `App/CirmoloSynth/src`. PC tests run with `src/build.sh prova`: engine tests, interface tests with screenshots of every page, and the Windows build.

On the SD card full paths start from `/mnt/SDCARD/`, for example `/mnt/SDCARD/Saves/cirmolo-synth/`.

## 17. Current limits

- One kit, one tempo and one swing for all patterns.
- The Flip does not follow an external MIDI clock: it always sets the tempo.
- Live recording covers only the drums (Live buttons and MIDI pads), not keyboard notes.
- There is no undo.
- The interface is in Italian only.
