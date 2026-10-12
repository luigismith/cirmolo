# Fioritura — manual

*[Italiano](MANUAL.it.md)*

Fioritura ("blossoming") is a garden of sound and light for the Miyoo Flip. You do not need to know how to play: you plant a few notes, the garden repeats them, lets them fade slowly and, if you leave it alone, carries on by itself. It is made for headphones, for the evening, or for a soft background while you do something else. The interface is in Italian; the names in brackets below are what you see on screen. You find it on the Home screen, under **Music** (*Musica*).

It has two modes, and they play together:

- **the Garden** (*Giardino*), after *Bloom* by Brian Eno and Peter Chilvers: every note you plant blooms on screen and comes back on each turn of the cycle, softer each time, until the garden forgets it;
- **the Fireflies** (*Lucciole*), after *Otomata* by Batuhan Bozkurt: on an 8×8 grid fireflies fly straight, play a note when they hit an edge and turn when they meet.

If you connect a USB microphone, the garden listens to you: sing and it blooms, blow and the wind rises, clap and a firefly is born (see [Microphone](#microphone)).

Quit with **MENU** (then A). The garden, the fireflies and the settings are saved: next time the garden starts playing again where it was.

## The Garden

Thin lines cross the screen: the **strings**, the notes of the scale from low (bottom) to high (top), three octaves. The brighter strings are the tonics.

| Button | What it does |
|---|---|
| D-pad up / down | moves the cursor one string (the note) |
| D-pad left / right, stick | moves the cursor: left to right is the stereo position |
| **A** | plants a flower: the note plays and blooms |
| A held + D-pad up/down | every new string under the cursor is a flower: glissandos and arpeggios |
| **B** | removes the last flower you planted |
| B held (one second) | empties the garden |
| **X**, R1 / L1 | next / previous mood |
| **Y** | dream on or off |
| L2 / R2 | memory: how many times each flower comes back |
| **SELECT** | switches to the Fireflies |
| SELECT held | help with every button, while you hold it |
| **START** | panel |

### How it works

- **Breath** (*Respiro*). The garden turns in a cycle (14 seconds at first). Each flower comes back at the point of the cycle where you planted it: the thin line at the top shows the cycle, with a dot for each flower and the light running along it.
- **Memory** (*Memoria*). Each return is softer than the one before. With memory 6 a flower comes back six times and then goes; with "forever" (*per sempre*) it stays until you remove it.
- **Wind** (*Vento*). On each return the wind can move a flower one string up or down: the melody slowly changes by itself. At 0 the garden repeats exactly.
- **Dream** (*Sogno*). If you touch nothing for six seconds, a ghost cursor starts walking among the strings and planting by itself: small steps, now and then a leap or two notes together. Any button wakes the garden. The dream plants more often when the garden is nearly empty and stops when it is full.
- After a few seconds without input, labels and hints fade away and only the garden is left. They come back with the first button.

## The Fireflies

An 8×8 grid. The notes of the rows are on the left; the columns play the same notes, left to right.

| Button | What it does |
|---|---|
| D-pad, stick | chooses the cell |
| **A** | a firefly is born flying up; press again to turn it right, down, left; the fifth press lets it fly away |
| **B** | removes the fireflies in the cell |
| B held | frees them all |
| **Y** | makes the fireflies fly, or stops them |
| L2 / R2 | tempo, in steps of 4 BPM (one step is an eighth note) |
| X, R1 / L1 | mood |
| SELECT | back to the Garden (held: help) |
| START | panel |

There are three rules:

1. on each step every firefly moves one cell in its direction;
2. when it hits an edge it plays the note of that column (top and bottom edges) or of that row (left and right edges) and turns back;
3. two fireflies that land in the same cell both turn right.

Two or three fireflies are enough for loops that repeat and slowly change. The fireflies keep flying when you go back to the Garden, and the garden keeps playing while you watch the fireflies: they layer.

## The panel (START)

Up and down choose the row, left and right change the value, A runs the actions, B or START close. L1 and R1 change mood from here too.

| Row | What it sets |
|---|---|
| Atmosfera (mood) | one of six: Alba, Lago, Bosco, Notte, Pioggia, Neve |
| Strumento (instrument) | bell, glass, harp, breath, drop, kalimba |
| Scala (scale) | major and minor pentatonic, major, minor, Dorian, Lydian, Mixolydian, in-sen, hirajoshi, whole tone |
| Tonica (tonic) | the starting note of the scale |
| Luce (light) | how bright the instrument is |
| Coda (tail) | how long each note lasts |
| Respiro (breath) | the length of the garden's cycle (4–40 s) |
| Memoria (memory) | how many times each flower comes back (1–16, or forever) |
| Vento (wind) | how often the wind moves the flowers |
| Tempo delle lucciole | fireflies' tempo, 40–240 BPM |
| Spazio (space) | the reverb |
| Shimmer | the reverb tail rising one octave |
| Eco (echo) | the echo bouncing left and right |
| Bordone (drone) | a long low note (tonic and fifth) breathing under everything; 0 = off |
| Sogno (dream) | on or off |
| Ascolto (listening) | how sensitive the microphone is (10–100%), or off |
| Volume | the main volume |
| Registra in WAV | records what you hear (see below) |
| Ripristina l'atmosfera | puts the mood's instrument, scale and space back as they were |

Instrument, scale, tonic, light, tail, space, shimmer, echo and drone belong to the mood: each mood remembers its own. Breath, memory, wind, tempo, dream, listening and volume apply to all of them. When you change instrument, scale, tonic, light or tail you hear a test note at once. If you change the scale, flowers stay on their string and change note with it.

## The six moods

| Mood | Instrument | Scale | Colours |
|---|---|---|---|
| Alba (dawn) | bell | major pentatonic, D | indigo and rose, warm light |
| Lago (lake) | glass | Lydian, F | deep blue and cyan |
| Bosco (wood) | harp | Dorian, A | pine-needle green, gold |
| Notte (night) | breath | minor pentatonic, C# | the violet of the pine cone |
| Pioggia (rain) | drop | major, G | blue-grey, long echo |
| Neve (snow) | kalimba | in-sen, E | white and ice |

When the mood changes the sky fades into the new colour and the drone glides to the new tonic.

## MIDI keyboard

Connect a USB MIDI keyboard, such as the MPK mini, to the USB-C port. Every note you play becomes a flower in the garden, at its exact pitch, at a random point of the stereo field. If the note is in the scale the flower goes onto its string and the wind can move it. How hard you play sets how loud the flower is.

## Launchpad Mini MK3

Connected to the USB-C port, the Launchpad becomes a second surface with colour LEDs:

- **in the Garden** the grid is 8 strings (the bottom row is the lowest): each pad plants a flower, the column is the stereo position. ▲ and ▼ shift the 8 strings by an octave. Flowers that are lit or coming back light up on the pads;
- **in the Fireflies** the grid is the fireflies' grid: a pad gives birth to a firefly or turns it, User held + pad removes them. ▲ and ▼ change the tempo;
- ◀ and ▶ change mood; **Session** switches mode; **Drums** turns the dream on; **Keys** makes the fireflies fly;
- the buttons on the right, from the top: the six moods, then **empty** (only with User held) and **record**.

When you quit, the Launchpad goes dark and returns to its normal mode.

## Microphone

Connect a USB-C microphone to the top port (a USB headset with a microphone works too, or a conference speakerphone such as the Jabra Speak) and the garden listens to you. **ascolta** ("listening") appears at the top right, with a dot that lights up with what it hears, and the strings start vibrating by themselves with the sounds of the room. Each one answers to its own note, like the strings of a harp when someone sings nearby. The Flip has no built-in microphone: without a USB microphone everything stays as before.

Three gestures:

| Gesture | What happens |
|---|---|
| **Sing** a note (humming or whistling works too) | a trail of light writes your voice from left to right, at its exact pitch, vibrato and flat notes included; every note you hold for a moment becomes a flower on the nearest string, and the garden repeats it like any other. Notes that are too low or too high are brought into the garden's three octaves. |
| **Blow** on the microphone (or go "ssss") | the wind rises: the strings sway, petals fly away and a wave of light crosses the garden from left to right, softly playing the flowers it meets, like an aeolian harp. The wind can move a few flowers by one string. If the garden is nearly empty, the wind sows new flowers. |
| **Clap** (or snap your fingers, or knock on the table) | a firefly is born and flies across the garden, and the fireflies start flying. If you keep clapping, the fireflies flash with you (a new firefly is born only after two seconds of silence); if you clap in time, from the fourth clap they take your tempo, one clap per quarter note, with a step on the clap. |

Singing, blowing or clapping keeps the garden awake: the dream waits for the microphone to be quiet too.

In the panel, **Ascolto** sets the sensitivity (10–100%) or turns listening off. With high sensitivity the garden also hears soft, distant sounds; with low sensitivity only loud, close ones.

The garden does not listen to itself. If the microphone hears the Flip's speaker, after a few notes the ear notices, learns how loud the garden reaches it and only listens to what is louder; it also ignores a note that starts right after the garden played the same note. With headphones there is no such problem and listening is more sensitive. With the speaker, sing a little louder than the garden, or get closer to the microphone.

## Recording

In the panel, "Registra in WAV" starts recording what you hear: **REC** and the time appear at the top right. Press A again on the same row to stop. On the Launchpad, use the last button on the right. Files are stereo WAV, 48 kHz, 16 bit, in `Saves/fioritura/registrazioni/`, named like `fioritura-2026-10-12-213000.wav`. A recording in progress is closed automatically when you quit.

## Tips

- To start: Notte, memory "forever", five or six scattered flowers, then let the dream take over.
- Wind at 20–30% with high memory gives melodies that change slowly and never quite repeat.
- Two fireflies on different rows, one flying right and one flying up, already make a long loop.
- Full shimmer with the bell or the glass gives a tail that rises like a choir.
- With a microphone: Lago, memory "forever", sing a slow melody and then stop. The garden repeats it on every breath; when you want to change it, blow.

## Files

- `Saves/fioritura/stato.txt`: mood, settings, flowers and fireflies (rewritten automatically).
- `Saves/fioritura/registrazioni/`: the recordings.
- `Saves/spruce/fioritura.log`: the app's log.
- Sources are in `App/Fioritura/src`. The PC tests run with `src/build.sh prova`.
