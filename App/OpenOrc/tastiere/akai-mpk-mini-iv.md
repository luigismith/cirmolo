# Akai MPK mini IV per OpenOrc

La MPK mini IV non ha un programma per il computer e non importa file: Akai conferma che tutto si
imposta dalla tastiera, con **Program Edit** (manuale v1.0, pagine 20-22; FAQ di supporto). Questa scheda
fa preparare **una volta sola** un preset utente che manda i valori del file `akai-mpk-mini-iv.txt`.
Bastano circa dieci minuti, poi si richiama con due tasti.

## 1. Carica un preset utente
I preset DAW e Plugin non si possono modificare.
- **SHIFT + PLUGIN/DAW** (User Presets), gira l'encoder fino a un preset utente libero e premilo.

## 2. Menu globale (SHIFT + LOOP)
| Voce | Valore |
|---|---|
| MidiCh | 1 (tasti e manopole) |
| PadCh | **10** |
| KnobM | **Abs** |
| Toggle | Off |

Esci con **PLUGIN/DAW**.

## 3. Program Edit (SHIFT + OCT -)
**Pad:** MidiCh **10** e i numeri di nota qui sotto. Il banco si cambia con **BANK A/B**.

| | sinistra | 2° | 3° | destra |
|---|---|---|---|---|
| **Banco A, fila in alto** | 40 Dim | 41 Min | 42 Maj | 43 Sus |
| **Banco A, fila in basso** | 36 6 | 37 m7 | 38 M7 | 39 9 |
| **Banco B, fila in alto** | 48 Modo tonalità | 49 Esecuzione | 50 Suono prec. | 51 Suono succ. |
| **Banco B, fila in basso** | 44 Batteria | 45 Loop: registra | 46 Loop: suona/ferma | 47 Loop: cancella |

È lo schema MPC classico: la nota di ogni pad è 35 + il suo numero, dal pad in basso a sinistra.
Su molte MPK è già così di fabbrica: controllalo prima di cambiarlo.

**Manopole:** CC#, LoVal 0, HiVal 127, Mode **Abs**.

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| 70 | 71 | 72 | 73 | 74 | 75 | 76 | 77 |
| Voicing | Basso | Esecuzione | Tono | Chorus | Delay | Riverbero | Volume |

Facoltativo: con i colori dei pad (Off C, On C) puoi distinguere le qualità (fila in alto del banco A)
dalle estensioni.

## 4. Salva
**SHIFT + OCT +**, scegli lo slot utente con l'encoder e premilo. Da quel momento lo richiami con
**SHIFT + PLUGIN/DAW**.

## 5. In OpenOrc
1. Collega la MPK alla porta USB-C della Flip e apri OpenOrc.
2. Premi **SELECT** fino alla pagina **MIDI**, poi **Y**: compare «Tastiera: Akai MPK mini IV».
3. Prova: tieni il pad **Maj** e premi un tasto. Nella pagina MIDI, in basso, «Ultimo messaggio» mostra
   cosa arriva dalla tastiera.

**Tasti di trasporto** (Play/Stop, Record, Overdub, Loop, Undo): escono su un'altra porta con valori che
il manuale non elenca. Nella pagina MIDI scegli il gruppo «Tasti di trasporto», premi **A** e premili in
fila. Poi **R2** salva tutto in `Saves/openorc/tastiere/mia-tastiera.txt`.

Se una manopola salta o va al contrario, prova «Tipo di manopole» nella pagina MIDI (Assolute, Relative,
Relative (64)) oppure rimetti Mode **Abs** sulla tastiera.

## Altre tastiere
Copia `akai-mpk-mini-iv.txt` in `Saves/openorc/tastiere/` con un altro nome e cambia i numeri: Y lo trova.
Oppure fai imparare i controlli dalla pagina MIDI e salva con R2. I file in `Saves/openorc/tastiere/`
restano anche dopo gli aggiornamenti di Cirmolo.
