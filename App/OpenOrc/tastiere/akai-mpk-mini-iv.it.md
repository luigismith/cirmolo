# Akai MPK mini IV per OpenOrc

*[English](akai-mpk-mini-iv.md) · Italiano*

OpenOrc usa i **valori di fabbrica dei preset utente** della MPK mini IV:
- pad sulle note 36–51, canale 10;
- manopole sui CC 24–31, modo Abs.

Il file `akai-mpk-mini-iv.txt` li contiene, quindi di solito **non serve configurare la tastiera**.
Questi valori non vengono da Akai, che non pubblica la sua MIDI implementation: vengono da due progetti
della community (configurazione REAPER/ReaLearn, codec per Reason). Per questo qui sotto c'è come
controllarli e, se serve, come impostarli. La MPK non ha un programma per il computer e non importa
file: si imposta solo dal pannello (manuale v1.0, pagine 20-22; FAQ Akai).

## 1. Controllo veloce (2 minuti)
1. Sulla MPK: **SHIFT + PLUGIN/DAW** (User Presets), scegli un preset **utente** e premi l'encoder.
   I preset DAW e Plugin mandano i controlli agli script delle DAW.
2. Collega la MPK alla porta USB-C della Flip e apri OpenOrc.
3. Premi **SELECT** fino alla pagina **MIDI**, poi **Y**: compare «Tastiera: Akai MPK mini IV».
4. Tocca un pad e gira una manopola. In basso, alla riga «Ultimo messaggio», devono comparire
   «Nota 36…51 · canale 10» e «CC 24…31».

Se torna tutto, hai finito. Altrimenti passa al punto 2, oppure fai imparare i controlli a OpenOrc dalla
pagina MIDI (A su una riga, poi muovi il controllo) e salva con **R2**.

## 2. Impostare la MPK a mano (solo se serve)
**Menu globale (SHIFT + LOOP)**

| Voce | Valore |
|---|---|
| MidiCh | 1 (tasti e manopole) |
| PadCh | **10** |
| KnobM | **Abs** |
| Toggle | Off |

Esci con **PLUGIN/DAW**.

**Program Edit (SHIFT + OCT -)**: tocca o muovi il controllo da modificare, poi regola con l'encoder.

Per i pad: MidiCh **10**. Il banco si cambia con **BANK A/B**.

| | sinistra | 2° | 3° | destra |
|---|---|---|---|---|
| **Banco A, fila in alto** | 40 Dim | 41 Min | 42 Maj | 43 Sus |
| **Banco A, fila in basso** | 36 6 | 37 m7 | 38 M7 | 39 9 |
| **Banco B, fila in alto** | 48 Modo tonalità | 49 Esecuzione | 50 Suono prec. | 51 Suono succ. |
| **Banco B, fila in basso** | 44 Batteria | 45 Loop: registra | 46 Loop: suona/ferma | 47 Loop: cancella |

È lo schema MPC: la nota è 35 + il numero del pad, contando da quello in basso a sinistra.

Per le manopole: CC#, LoVal 0, HiVal 127, Mode **Abs**.

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| Voicing | Basso | Esecuzione | Tono | Chorus | Delay | Riverbero | Volume |

Le serigrafie della fila in alto valgono per le funzioni dell'arpeggiatore (ARP + manopola), non per
OpenOrc.

**Salva:** **SHIFT + OCT +**, scegli lo slot utente con l'encoder e premilo. Da quel momento lo richiami
con **SHIFT + PLUGIN/DAW**.

## 3. Da sapere
- **Chords, Scales, ARP e Note Repeat della MPK vanno spenti.** Gli accordi li costruisce OpenOrc: la
  tastiera deve mandare note singole.
- **Manopole in Rel:** OpenOrc le capisce. Nella pagina MIDI imposta «Tipo di manopole» su **Relative**:
  con ogni probabilità la MPK manda 1–63 per salire e 65–127 per scendere (da verificare: se una
  manopola va al contrario o a scatti, prova «Relative (64)»). Così non saltano di valore.
- **Tasti di trasporto:** il preset li legge come CC sulla porta 2 (Play 76, Record 77, Loop 74,
  Undo 73), come gli script della community. Il manuale invece dice che nei preset utente mandano note
  (voce Trnspt del menu globale). Se non rispondono, impara il gruppo «Tasti di trasporto» e salva con R2.
- **Firmware:** conviene aggiornarlo una volta dal PC (inMusic Software Center) prima dell'uso. Con un
  firmware vecchio le porte possono essere diverse.
- Da evitare: SysEx inviati alla cieca. Alcuni codici mettono la MPK in modalità aggiornamento firmware.

## Altre tastiere
Copia `akai-mpk-mini-iv.txt` in `Saves/openorc/tastiere/` con un altro nome e cambia i numeri: Y lo trova.
Oppure fai imparare i controlli dalla pagina MIDI e salva con R2. I file in `Saves/openorc/tastiere/`
restano anche dopo gli aggiornamenti di Cirmolo.
