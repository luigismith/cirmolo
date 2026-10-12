# OpenOrc — manuale

*[English](MANUAL.md) · Italiano*

OpenOrc è un sintetizzatore di accordi per la Miyoo Flip, ispirato all'Orchid di Telepathic Instruments.
Si suona con i tasti della Flip oppure con una tastiera MIDI USB: è pensato per la **Akai MPK mini IV**,
ma impara i controlli di qualunque tastiera. Un tasto (o un pulsante) suona un accordo intero, con il
basso, nel modo scelto: accordo pieno, strum, arpeggio, arpa e altri.

Questo manuale descrive la versione del ramo `cirmolo` di ottobre 2026.

## In breve

| Vuoi... | Fai così |
|---|---|
| suonare subito | Pagina **Suona**: croce e A B X Y suonano gli accordi della tonalità |
| cambiare suono | Pagina **Suono**, prima riga, sinistra/destra; sulla MPK i pad 50 e 51 (banco B) |
| cambiare pagina | **SELECT** (Suona → Suono → Ritmo → MIDI → Suona) |
| far partire batteria e arpeggi a tempo | **START** |
| spegnere tutte le note | **L3** (premi la levetta sinistra) |
| uscire | **MENU**, poi A |

## 1. Avvio, uscita e salvataggio
- OpenOrc si apre dall'area **App** di Cirmolo. Mentre è aperto la console non va in sospensione.
- Si esce con **MENU**: compare una conferma. **A** o giù esce, **B** o su resta.
- Le impostazioni si salvano da sole circa 8 secondi dopo l'ultima modifica e all'uscita, in
  `Saves/openorc/stato.txt`: suono corrente, tonalità, tempo, loop, mappa MIDI.
- La batteria parte sempre spenta, anche se all'uscita era accesa.
- Il loop registrato **non** si salva: all'uscita si perde.

## 2. Le pagine
In alto ci sono il nome dell'app, le quattro schede, la spia **MIDI** e il simbolo del trasporto.
- La spia MIDI è verde quando una tastiera è collegata e lampeggia a ogni messaggio.
- Il simbolo del trasporto è un quadrato da fermo. In moto diventa un triangolo dorato che pulsa sul battito.

| Pagina | A cosa serve |
|---|---|
| **Suona** | suonare: accordo corrente, oscilloscopio, tasti e pad |
| **Suono** | suono, tonalità, voicing, modo di esecuzione, timbro, basso, effetti |
| **Ritmo** | tempo, batteria, looper |
| **MIDI** | collegare e configurare la tastiera |

La fascia in basso ricorda i comandi della pagina.

### Comandi validi in tutte le pagine

| Tasto | Funzione |
|---|---|
| SELECT | pagina successiva |
| START | avvia o ferma il trasporto: batteria, arpeggi e pattern a tempo, looper |
| MENU | uscita, con conferma |
| L3 (levetta sinistra premuta) | spegne tutte le note («panico») |
| levetta sinistra, orizzontale | pitch bend, fino a ±2 semitoni |
| levetta sinistra, in su | vibrato (modulazione) |
| levetta destra, verticale | brillantezza: su più brillante, giù più scuro |

## 3. Suonare con la Flip (pagina Suona)
Senza tastiera si suona con i tasti della console. Ogni tasto suona un grado della tonalità scelta nella
pagina Suono (all'inizio Do maggiore). Sullo schermo ogni tasto mostra il nome dell'accordo che suonerebbe
adesso, nella stessa posizione che ha sulla console.

| Tasto | Grado (maggiore) | In Do maggiore | Grado (minore) | In La minore |
|---|---|---|---|---|
| ← sinistra | I | Do | i | Lam |
| ↑ su | IV | Fa | iv | Rem |
| → destra | V | Sol | v | Mim |
| ↓ giù | vi | Lam | VI | Fa |
| Y | ii | Rem | ii° | Si° |
| X | iii | Mim | III | Do |
| B | vii° | Si° | VII | Sol |
| A | VII bemolle (fuori scala) | Sib | V maggiore | Mi |

I dorsali cambiano l'accordo mentre li tieni. Si possono combinare e valgono anche sull'accordo che sta
già suonando.

| Dorsale | Effetto |
|---|---|
| L1 | scambia maggiore e minore (il diminuito diventa maggiore) |
| R1 | sus4: la terza diventa quarta |
| L2 | aggiunge la settima della scala |
| R2 | aggiunge la nona |

**R3** (levetta destra premuta) passa al modo di esecuzione successivo (vedi il capitolo 6).

Se tieni due tasti e ne lasci uno, torna a suonare l'altro: vince sempre l'ultimo premuto.

## 4. Suonare con la Akai MPK mini IV
Collega la tastiera alla porta USB-C della Flip, anche con OpenOrc già aperto. Compare «Tastiera
collegata». La MPK funziona con i valori di fabbrica dei suoi preset utente, senza configurazione: vedi
la [scheda della MPK](tastiere/akai-mpk-mini-iv.it.md). Sulla MPK vanno spenti **Chords, Scales, ARP e
Note Repeat**: gli accordi li costruisce OpenOrc.

Con la tastiera collegata, la pagina Suona mostra a sinistra gli 8 pad degli accordi. I tasti della Flip
restano attivi, più piccoli, a destra.

### Pad del banco A: gli accordi

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **fila in alto** (qualità) | Dim | Min | Maj | Sus |
| **fila in basso** (estensioni) | 6 | m7 | M7 | 9 |

- **Tieni un pad di qualità e premi un tasto:** suona l'accordo di quella qualità con fondamentale sul
  tasto premuto. Esempio: Min + Re = Rem.
- **Aggiungi un pad della fila in basso** per l'estensione: Min + m7 + Re = Rem7.
- Combinazioni speciali:
  - Dim + Maj danno l'accordo **aumentato**;
  - Min + Sus danno il **sus2**;
  - 6 con una settima dà la **tredicesima**.
- Se tieni più pad di qualità vince l'ultimo premuto.
- Conta solo il nome della nota, non l'ottava: il registro lo decide il voicing (manopola K1).
- **Senza pad**, i tasti suonano **note singole** (melodia) con lo stesso suono, sopra gli accordi.
- Con il **modo tonalità** attivo (pad 48), i tasti senza pad suonano gli accordi della tonalità: ogni
  nota della scala dà il suo accordo, e le note fuori scala prendono il grado subito sotto.
- La dinamica conta: suonando più forte l'accordo è più forte e più brillante.

### Pad del banco B: le funzioni

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **fila in alto** | Modo tonalità | Esecuzione successiva | Suono precedente | Suono successivo |
| **fila in basso** | Batteria | Loop: registra | Loop: suona o ferma | Loop: cancella |

### Manopole

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| Voicing | Basso | Esecuzione (quantità) | Tono | Chorus | Delay | Riverbero | Volume |

Girando una manopola compare per un attimo il suo nome e il valore. Le manopole della MPK mini IV sono
relative: non saltano mai di valore.

### Altri controlli
- **Rotella di pitch:** pitch bend, ±2 semitoni.
- **Rotella di modulazione:** vibrato.
- **Pedale (sustain, CC 64):** tiene accordo e note finché non lo lasci.
- **Program change:** sceglie il suono (0 = primo suono, 1 = secondo...).
- **Tasti di trasporto** (Play, Rec, Loop, Undo): avvio e arresto, registra, suona o ferma il loop,
  cancella il loop. Sulla MPK mini IV potrebbero non rispondere: vedi la scheda della tastiera.
- **Clock MIDI:** se arriva da un'altra macchina, OpenOrc ne segue il tempo («Segui il clock MIDI»
  nella pagina Ritmo). I messaggi Start e Stop avviano e fermano il trasporto.

## 5. Lo schermo della pagina Suona
- **Riga di stato**, in alto: suono, modo di esecuzione, tonalità, tempo e batteria, stato del loop.
  - La tonalità è dorata quando il modo tonalità è attivo.
  - Lo stato del loop è rosso quando registra.
- **Fascia dell'accordo**, al centro:
  - il nome grande, per esempio «Lam7»;
  - sotto, il simbolo inglese («Am7») e le note;
  - poi il grado nella tonalità («ii7 in Sol maggiore») oppure la nota del basso.
- In basso a sinistra nella fascia ci sono le note suonate con l'ottava. È il voicing scelto da K1.
- Dietro al nome scorre l'**oscilloscopio** dell'uscita. Il suo colore dice chi sta suonando:

| Colore | Chi suona |
|---|---|
| oro | l'accordo suonato dal vivo |
| viola | la melodia |
| verde acqua | il loop |

  Quando non suona nulla compare «silenzio».

## 6. Voicing, apertura ed esecuzione
**Voicing (K1).** Sceglie da solo il rivolto dell'accordo. Lo tiene vicino al registro della manopola: a
sinistra grave, a destra acuto. Passando da un accordo all'altro sceglie il rivolto che muove meno le
voci, così il giro di accordi suona legato.

**Apertura.** Stretta, aperta o ampia: quanto sono distanti le note dell'accordo.

**Esecuzione.** Dice come parte l'accordo. La «Quantità» (manopola K3) cambia significato a seconda del modo:

| Modo | Cosa fa | Quantità |
|---|---|---|
| Accordo | tutte le note insieme | — |
| Strum | come una chitarra; alterna pennata in giù e in su | distanza tra le note, 8–98 ms |
| Slop | note un po' sfasate e di forza diversa, come suonate a mano | quanto sono sfasate |
| Arpeggio | su e giù su due ottave, finché tieni l'accordo | velocità: 1/4, 1/8, 1/8 terzina, 1/16, 1/32 |
| Pattern | l'accordo ribattuto con un ritmo | uno dei 4 ritmi |
| Arpa | glissato sulle note dell'accordo, su più ottave | distanza tra le note, 25–115 ms |

Arpeggio e Pattern seguono il tempo della pagina Ritmo. Con il trasporto in moto (START) si agganciano
alla griglia di batteria e looper.

## 7. Pagina Suono
È un elenco: **su/giù** sceglie la riga, **sinistra/destra** cambia il valore. Tenendo premuto il valore
scorre. **L1/R1** cambiano a passi grandi; sulla Tonalità saltano di quinta. **A tenuto** fa ascoltare
l'accordo di I grado, così senti le modifiche mentre le fai.

| Gruppo | Parametro | Cosa fa |
|---|---|---|
| | **Suono** | carica uno dei suoni di fabbrica |
| **Accordi** | Tonalità, Scala | tonalità (Do…Si) e maggiore/minore, per i tasti della Flip e il modo tonalità |
| | Modo tonalità | i tasti della MPK senza pad suonano gli accordi della scala |
| | Voicing, Apertura | vedi il capitolo 6 |
| | Esecuzione, Quantità | vedi il capitolo 6 |
| **Timbro** | Motore | Analogico, FM, Piano elettrico, Organo |
| | Tono | brillantezza: filtro, indice FM o armoniche |
| | Carattere | dipende dal motore (vedi sotto) |
| | Attacco, Decadimento, Sostegno, Rilascio | l'inviluppo del volume |
| | Vibrato | vibrato; nel piano elettrico diventa **Tremolo** |
| **Basso** | Livello, Tono | il basso monofonico che suona la fondamentale (Do2–Si2); 0 lo spegne |
| **Effetti** | Chorus, Delay, Riverbero | effetti; il delay è ping-pong a croma puntata, legato al tempo |
| | Volume | volume generale |

Il **Carattere** dipende dal motore:

| Motore | Cosa cambia il Carattere |
|---|---|
| Analogico | fino a metà aumenta la desintonia fra due oscillatori a dente di sega; oltre, entra l'onda quadra |
| FM | si chiama **Rapporto**: 1, 2, 3, 4, 1/2, 3,5, 7 tra modulante e portante |
| Piano elettrico | quanto è spinta la saturazione |
| Organo | si chiama **Registri**: quante armoniche entrano |

### I suoni di fabbrica

| # | Suono | Motore | Esecuzione |
|---|---|---|---|
| 0 | Lungolago | Analogico | Accordo |
| 1 | Tramonto FM | FM | Accordo |
| 2 | Wurli di notte | Piano elettrico | Slop |
| 3 | Arpa di vetro | FM | Arpa |
| 4 | Cascata | Analogico | Arpeggio |
| 5 | Psichedelia | Analogico | Slop |
| 6 | Organo della pieve | Organo | Accordo |
| 7 | Ottoni | Analogico | Strum |
| 8 | Ritmo in levare | Analogico | Pattern |
| 9 | Campane lunari | FM | Strum |
| 10 | Chitarra di carta | Analogico | Strum |
| 11 | Nebbia | Analogico | Accordo |

Il numero è quello del program change. I suoni si possono scorrere anche dalla MPK, con i pad 50 e 51
del banco B.

Un suono comprende anche l'esecuzione, la quantità e l'apertura: cambiando suono cambiano anche queste.
Il volume invece resta quello scelto.

> **Attenzione:** le modifiche al suono corrente si conservano, anche dopo l'uscita, finché non carichi
> un altro suono. Caricandolo si perdono: per ora non si possono salvare suoni propri.

## 8. Pagina Ritmo
In alto ci sono tre riquadri:
- il **tempo**, con la scritta «clock MIDI» quando arriva un clock esterno;
- la **batteria**, con il pattern dei 16 passi e il passo corrente;
- il **loop**, con la linea del tempo, le battute, gli accordi (barre dorate), le note (punti viola) e la
  posizione.

Sotto c'è l'elenco delle impostazioni:

| Impostazione | Valori |
|---|---|
| Batteria | Sì / No. Accenderla avvia il trasporto |
| Stile | Disco, Psichedelico, Trap, Bossa nova |
| Livello | volume della batteria |
| Tempo | 30–300 BPM |
| Segui il clock MIDI | Sì / No |
| Lunghezza massima del loop | 1, 2, 4, 8 battute |
| Quantizzazione del loop | No, 1/16, 1/8, 1/4 |

| Tasto | Funzione |
|---|---|
| su/giù, sinistra/destra | scegli e cambia |
| L1 / R1 | tempo −10 / +10, da qualunque riga |
| X | tap tempo: battilo almeno due volte |
| A | loop: registra / sovraincidi |
| B | loop: suona o ferma |
| Y | loop: cancella |

### Il looper
Funziona come un looper a pedale e registra quello che suoni: accordi e note. Il loop suona con il suono
corrente: cambiando suono o manopole cambia anche lui.

1. **Registra** (A, o il pad 45). A trasporto fermo parte subito. In moto aspetta l'inizio della prossima
   battuta: se premi appena dopo la battuta, parte da quella.
   - Mentre registri, i piatti fanno da metronomo.
   - Ripremere prima che parta annulla.
2. **Registra di nuovo** per chiudere il giro: la lunghezza si arrotonda alla battuta più vicina e il loop
   comincia a suonare. Se arrivi alla lunghezza massima, il giro si chiude da solo.
3. Mentre suona, **Registra** passa alla **sovraincisione**: aggiungi altro sopra. Ripremi per tornare
   ad ascoltare.
4. **Suona/ferma** (B, o il pad 46) lo ferma e lo riavvia.
5. **Cancella** (Y, o il pad 47) lo svuota.

La quantizzazione allinea alla griglia scelta quello che registri. Il loop contiene fino a 1024 eventi:
oltre compare «pieno: niente altre note». Il loop e quello che suoni dal vivo hanno ciascuno il proprio
accordo; il basso segue l'ultimo accordo partito.

## 9. Pagina MIDI
Elenca tutti i controlli che OpenOrc riceve dalla tastiera e a cosa sono collegati, in quattro gruppi:
pad degli accordi, pad delle funzioni, manopole, tasti di trasporto.

| Tasto | Funzione |
|---|---|
| su/giù | scegli la riga |
| L1 / R1 | salta al gruppo precedente o successivo |
| A su una riga | **impara**: la riga dice «in ascolto...», premi o muovi il controllo sulla tastiera |
| A sull'intestazione di un gruppo | impara tutto il gruppo, un controllo dopo l'altro |
| B | annulla l'apprendimento |
| X | toglie l'assegnazione della riga |
| Y | carica il prossimo **preset della tastiera** |
| R2 | salva la mappa corrente in `Saves/openorc/tastiere/mia-tastiera.txt` |
| sinistra/destra o A su «Tipo di manopole» | Assolute, Relative, Relative (64) |

- In fondo, **Ultimo messaggio** mostra l'ultimo messaggio arrivato, con nota o CC, canale e porta. È
  il modo più rapido per capire cosa manda una tastiera.
- Un controllo assegnato a una funzione viene tolto da quella che lo usava prima: niente doppioni.
- Le manopole imparano solo CC. I pad imparano note o CC.
- Se nessun preset della tastiera funziona, Y torna alla mappa di fabbrica della MPK mini IV.

**Tipo di manopole:**
- **Assolute:** la manopola manda 0–127 e il parametro salta alla posizione della manopola.
- **Relative:** 1–63 sale, 65–127 scende. È il tipo della MPK mini IV.
- **Relative (64):** 64 è fermo; sopra sale, sotto scende.

I preset della tastiera sono file di testo leggibili. Si cercano in `Saves/openorc/tastiere/` (i tuoi,
che restano dopo gli aggiornamenti) e nella cartella `tastiere` dell'app. Il formato è spiegato nella
[scheda della MPK](tastiere/akai-mpk-mini-iv.it.md#altre-tastiere).

## 10. File

| File | Contenuto |
|---|---|
| `Saves/openorc/stato.txt` | impostazioni e suono corrente; cancellalo per tornare ai valori di fabbrica |
| `Saves/openorc/tastiere/*.txt` | i tuoi preset della tastiera |
| `App/OpenOrc/tastiere/` | preset e scheda della MPK mini IV |
| `Saves/spruce/openorc.log` | messaggi dell'ultima esecuzione, utili se qualcosa non va |

## 11. Se qualcosa non va

| Problema | Cosa fare |
|---|---|
| Non si sente nulla | Controlla il volume della console e la riga Volume (o K8). Se l'oscilloscopio si muove, il suono c'è. |
| Una nota resta accesa | **L3**, oppure il pedale su e giù. |
| La tastiera non fa nulla | Pagina MIDI: la spia deve essere verde. Se «Ultimo messaggio» cambia, la tastiera arriva: rifai l'apprendimento. |
| Gli accordi della MPK escono strani o doppi | Spegni Chords, Scales, ARP e Note Repeat sulla MPK. |
| Una manopola va al contrario o a scatti | Prova un altro «Tipo di manopole» nella pagina MIDI. |
| Arpeggio o pattern fuori tempo rispetto a un'altra macchina | Mandagli il clock MIDI e attiva «Segui il clock MIDI». |
| Il tempo non cambia | Arriva un clock MIDI: il tempo lo detta lui. |

## 12. Limiti attuali
- Non si possono salvare suoni propri; caricare un suono cancella le modifiche al precedente.
- Il suono si cambia solo uno alla volta, senza un elenco da cui scegliere.
- Il loop non si salva e non ha un vero «annulla»: Undo cancella tutto il giro.
- I quattro stili di batteria sono fissi.
- Dalla pagina Suona non si cambiano tonalità né suono con i soli tasti della Flip.
