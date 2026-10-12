# OpenOrc — manuale

*[English](MANUAL.md) · Italiano* · **[Prontuario](QUICKREF.it.md)**: tutti i comandi in una pagina, da tenere a vista mentre suoni.

OpenOrc è un sintetizzatore di accordi per la Miyoo Flip, ispirato all'Orchid di Telepathic Instruments.
Si suona con i tasti della Flip oppure con una tastiera MIDI USB: è pensato per la **Akai MPK mini IV**,
ma impara i controlli di qualunque tastiera. Un tasto (o un pulsante) suona un accordo intero, con il
basso, nel modo scelto: accordo pieno, strum, arpeggio, arpa e altri. Ha sei motori di suono, 25 suoni
di fabbrica in stereo, i tuoi suoni salvati, batteria e un looper a strati.

Questo manuale descrive la versione del ramo `cirmolo` di ottobre 2026.

## In breve

| Vuoi... | Fai così |
|---|---|
| suonare subito | Pagina **Suona**: croce e A B X Y suonano gli accordi della tonalità |
| cambiare suono al volo | **tieni SELECT** e premi su/giù; sulla MPK, i pad 50 e 51 del banco B |
| cambiare tonalità al volo | **tieni SELECT** e premi sinistra/destra |
| registrare un loop | **tieni SELECT** e premi A (oppure il pad 45) |
| salvare un suono tuo | pagina **Suono**, riga «Salva come nuovo suono», **A** |
| cambiare pagina | **tocca SELECT** (Suona → Accordi → Suono → Ritmo → MIDI) |
| far partire batteria e arpeggi a tempo | **START** |
| spegnere tutte le note | **L3** (premi la levetta sinistra) |
| uscire | **MENU**, poi A |

## 1. Avvio, uscita e salvataggio
- OpenOrc si apre dall'area **App** di Cirmolo. Mentre è aperto la console non va in sospensione.
- Si esce con **MENU**: compare una conferma. **A** o giù esce, **B** o su resta.
- Le impostazioni si salvano da sole circa 8 secondi dopo l'ultima modifica e all'uscita, in
  `Saves/openorc/stato.txt`: suono corrente, tonalità, tempo, impostazioni del loop, mappa MIDI.
- Il **loop** si salva all'uscita in `Saves/openorc/loop.txt`. Alla riapertura ritrovi il loop fermo:
  per farlo suonare premi B nella pagina Ritmo, oppure SELECT + B.
- I **tuoi suoni** stanno in `Saves/openorc/suoni/`, un file ciascuno (capitolo 8).
- La batteria parte sempre spenta, anche se all'uscita era accesa.

## 2. Le pagine
In alto ci sono il nome dell'app, le cinque schede, la spia **MIDI** e il simbolo del trasporto.
- La spia MIDI è verde quando una tastiera è collegata e lampeggia a ogni messaggio.
- Il simbolo del trasporto è un quadrato da fermo. In moto diventa un triangolo dorato che pulsa sul battito.

| Pagina | A cosa serve |
|---|---|
| **Suona** | suonare: accordo corrente, oscilloscopio, tasti e pad, manopole |
| **Accordi** | tonalità e scala, voicing e apertura, modo di esecuzione |
| **Suono** | scegliere, salvare e modificare i suoni: timbro, basso, stereo, effetti |
| **Ritmo** | tempo, batteria, looper |
| **MIDI** | collegare e configurare la tastiera |

La fascia in basso ricorda i comandi della pagina.

### Comandi validi in tutte le pagine

| Tasto | Funzione |
|---|---|
| SELECT toccato | pagina successiva |
| SELECT tenuto | elenco dei suoni e funzioni rapide (capitolo 3) |
| START | avvia o ferma il trasporto: batteria, arpeggi e pattern a tempo, looper |
| MENU | uscita, con conferma |
| L3 (levetta sinistra premuta) | spegne tutte le note («panico») |
| levetta sinistra, orizzontale | pitch bend, fino a ±2 semitoni |
| levetta sinistra, in su | vibrato (modulazione) |
| levetta destra, verticale | brillantezza: su più brillante, giù più scuro |

## 3. SELECT tenuto: suoni e funzioni al volo
Tieni premuto **SELECT**: dopo un attimo compare l'**elenco dei suoni**, con il suono corrente
evidenziato e la sua categoria a destra. Finché tieni SELECT, gli altri tasti fanno da funzioni rapide.
Se premi subito un altro tasto, non serve aspettare che l'elenco compaia.

| SELECT + | Funzione |
|---|---|
| su / giù | suono precedente / successivo |
| L1 / R1 | categoria precedente / successiva (Atmosfere, Tastiere, Corde, Arpeggi, Campane, Ottoni, Tuoi) |
| sinistra / destra | tonalità un semitono sotto / sopra |
| L2 | scala maggiore o minore |
| R2 | modo tonalità acceso o spento |
| A | loop: registra / sovraincidi |
| B | loop: suona o ferma |
| Y | loop: annulla |
| X | batteria accesa o spenta |

- Il suono cambia subito: con la MPK puoi continuare a suonare mentre scorri l'elenco.
- In alto a destra c'è la tonalità; in basso, lo stato del loop.
- Quando lasci SELECT l'elenco si chiude e resti nella stessa pagina.
- Funziona in tutte le pagine. Con la sola Flip è il modo di cambiare suono, tonalità e loop senza
  lasciare la pagina Suona.

## 4. Suonare con la Flip (pagina Suona)
Senza tastiera si suona con i tasti della console. Ogni tasto suona un grado della tonalità scelta (all'inizio
Do maggiore). Sullo schermo ogni tasto mostra il nome dell'accordo che suonerebbe adesso, nella stessa
posizione che ha sulla console.

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

**R3** (levetta destra premuta) passa al modo di esecuzione successivo (capitolo 7).

Se tieni due tasti e ne lasci uno, torna a suonare l'altro: vince sempre l'ultimo premuto.

## 5. Suonare con la Akai MPK mini IV
Collega la tastiera alla porta USB-C della Flip, anche con OpenOrc già aperto. Compare «Tastiera
collegata». La MPK funziona con i valori di fabbrica dei suoi preset utente, senza configurazione: vedi
la [scheda della MPK](tastiere/akai-mpk-mini-iv.it.md). Sulla MPK vanno spenti **Chords, Scales, ARP e
Note Repeat**: gli accordi li costruisce OpenOrc.

Con la tastiera collegata, la pagina Suona mostra a sinistra gli 8 pad degli accordi e, sotto, le 8
manopole. I tasti della Flip restano attivi, più piccoli, a destra.

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
| **fila in basso** | Batteria | Loop: registra | Loop: suona o ferma | Loop: annulla |

Con i pad 50 e 51 il nuovo suono compare per un attimo in alto, tra il precedente e il successivo.

### Manopole

| K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 |
|---|---|---|---|---|---|---|---|
| Voicing | Basso | Esecuzione (quantità) | Tono | Chorus | Delay | Riverbero | Volume |

Le manopole compaiono nella pagina Suona, sotto i pad, come otto barrette: quella appena girata si
accende in oro e compare per un attimo il suo nome con il valore. Le manopole della MPK mini IV sono
relative: non saltano mai di valore.

### Altri controlli
- **Rotella di pitch:** pitch bend, ±2 semitoni.
- **Rotella di modulazione:** vibrato.
- **Pedale (sustain, CC 64):** tiene accordo e note finché non lo lasci.
- **Program change:** sceglie il suono con il numero della tabella del capitolo 8; dopo i 25 di
  fabbrica vengono i tuoi.
- **Tasti di trasporto** (Play, Rec, Loop, Undo): avvio e arresto, registra, suona o ferma il loop,
  annulla. Sulla MPK mini IV potrebbero non rispondere: vedi la scheda della tastiera.
- **Clock MIDI:** se arriva da un'altra macchina, OpenOrc ne segue il tempo («Segui il clock MIDI»
  nella pagina Ritmo). I messaggi Start e Stop avviano e fermano il trasporto.

## 6. Lo schermo della pagina Suona
- **Riga di stato**, in alto: suono, modo di esecuzione, tonalità, tempo e batteria, stato del loop.
  - Un **asterisco** dopo il nome del suono vuol dire che l'hai modificato e non l'hai salvato.
  - La tonalità è dorata quando il modo tonalità è attivo.
  - Lo stato del loop è rosso quando registra; «+2» vuol dire due sovraincisioni sopra il primo giro.
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

## 7. Pagina Accordi
È un elenco: **su/giù** sceglie la riga, **sinistra/destra** cambia il valore, **L1/R1** a passi
grandi (sulla tonalità saltano di quinta). **A tenuto** fa ascoltare l'accordo di I grado.

| Gruppo | Riga | Cosa fa |
|---|---|---|
| **Tonalità** | Tonalità, Scala | tonalità (Do…Si) e maggiore/minore, per i tasti della Flip e il modo tonalità |
| | Modo tonalità | i tasti della MPK senza pad suonano gli accordi della scala |
| **Voicing** | Voicing | il registro degli accordi (come K1) |
| | Apertura | stretta, aperta o ampia: quanto sono distanti le note dell'accordo |
| **Esecuzione** | Esecuzione, Quantità | come parte l'accordo (tabella sotto) |
| | Cambia con il suono | **Sì**: ogni suono porta la sua esecuzione, quantità e apertura. **No**: restano quelle scelte, anche cambiando suono |

**Voicing.** OpenOrc sceglie da solo il rivolto dell'accordo e lo tiene vicino al registro scelto: a
sinistra grave, a destra acuto. Passando da un accordo all'altro sceglie il rivolto che muove meno le
voci, così il giro di accordi suona legato.

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

## 8. Pagina Suono
Stessi comandi della pagina Accordi. In più, sulla riga **Suono** sinistra/destra cambiano suono e
**L1/R1** saltano di categoria; sulle righe del gruppo Suono, **A** esegue l'azione.

### Scegliere e salvare i suoni

| Riga | Cosa fa con A |
|---|---|
| **Suono** | il suono corrente (sinistra/destra per cambiarlo); l'asterisco dice che è modificato |
| **Salva come nuovo suono** | salva il suono così com'è con un nome nuovo, scritto a destra: «Nebbia» diventa «Nebbia 2» |
| **Aggiorna il tuo suono** | salva le modifiche sul tuo suono corrente (non vale per quelli di fabbrica) |
| **Ripristina** | toglie le modifiche e torna al suono salvato |
| **Variazione casuale** | sposta un po' timbro, inviluppo ed effetti; premi ancora per un'altra, Ripristina per tornare |
| **Rinomina** | cambia il nome del tuo suono: si scrive con una **tastiera USB** collegata alla Flip; Invio o A conferma, B annulla |
| **Elimina** | cancella il tuo suono: premi A due volte |

Per creare un suono nuovo: parti da quello che ci somiglia di più, cambialo nella pagina Suono (oppure
con le manopole o con «Variazione casuale») e scegli **Salva come nuovo suono**. I tuoi suoni compaiono
in fondo all'elenco, nella categoria **Tuoi**.

Le modifiche al suono corrente non si perdono uscendo: restano, con l'asterisco, finché non carichi un
altro suono. Cambiando suono senza salvare, invece, si perdono.

Un suono comprende timbro, basso, effetti e (se «Cambia con il suono» è Sì) esecuzione, quantità e
apertura. Il **volume** no: resta quello che hai scelto, da un suono all'altro.

### Timbro, basso ed effetti

| Gruppo | Parametro | Cosa fa |
|---|---|---|
| **Timbro** | Motore | Analogico, FM, Piano elettrico, Organo, Corda pizzicata, Ensemble |
| | Tono | brillantezza: filtro, indice FM, armoniche o pizzico |
| | Carattere | dipende dal motore (vedi sotto) |
| | Attacco, Decadimento, Sostegno, Rilascio | l'inviluppo del volume |
| | Vibrato | vibrato; nel piano elettrico diventa **Tremolo** |
| **Basso** | Livello, Tono | il basso monofonico che suona la fondamentale (Do2–Si2); 0 lo spegne |
| **Effetti** | Ampiezza stereo | quanto il suono si allarga a destra e a sinistra (vedi sotto) |
| | Chorus, Delay, Riverbero | effetti; il delay è ping-pong a croma puntata, legato al tempo |
| | Volume | volume generale |

| Motore | Com'è | Cosa cambia il Carattere |
|---|---|---|
| Analogico | due oscillatori a dente di sega con filtro | fino a metà la desintonia; oltre, entra l'onda quadra |
| FM | due operatori, dal piano elettrico alle campane | si chiama **Rapporto**: 1, 2, 3, 4, 1/2, 3,5, 7 |
| Piano elettrico | FM con saturazione, tipo Rhodes e Wurlitzer | quanto è spinta la saturazione |
| Organo | armoniche sommate, come i registri | si chiama **Registri**: quante armoniche entrano |
| Corda pizzicata | una corda vera simulata (Karplus-Strong): chitarra, arpa, koto | si chiama **Durata**: quanto suona la corda, da 0,25 a 8 secondi |
| Ensemble | cinque oscillatori desintonizzati, larghi in stereo: archi, pad, ottoni | si chiama **Desintonia**: quanto sono scostati gli oscillatori |

Con la corda pizzicata il **Tono** dice quanto è brillante il pizzico e quanto restano le armoniche
alte. Per far suonare la corda finché vibra, tieni il **Sostegno** al massimo.

**Ampiezza stereo.** A 0 tutto è al centro. Alzandola, le note dell'accordo si aprono dalla più grave
a sinistra alla più acuta a destra, e arpeggi, strum e arpa passano da un lato all'altro. In più:
- l'**Ensemble** apre anche i suoi oscillatori, come un coro largo;
- nel **Piano elettrico** il tremolo va da un lato all'altro, come nei piani a valigia;
- il basso resta sempre al centro.

### I suoni di fabbrica
Il numero è quello del program change. I suoni nuovi di ottobre 2026 sono in stereo.

| # | Suono | Categoria | Motore | Esecuzione |
|---|---|---|---|---|
| 0 | Lungolago | Atmosfere | Analogico | Accordo |
| 1 | Nebbia | Atmosfere | Analogico | Accordo |
| 2 | Psichedelia | Atmosfere | Analogico | Slop |
| 3 | Aurora | Atmosfere | Ensemble | Accordo |
| 4 | Archi d'inverno | Atmosfere | Ensemble | Accordo |
| 5 | Respiro di vetro | Atmosfere | FM | Slop |
| 6 | Tramonto FM | Tastiere | FM | Accordo |
| 7 | Wurli di notte | Tastiere | Piano elettrico | Slop |
| 8 | Valigia stereo | Tastiere | Piano elettrico | Accordo |
| 9 | Organo della pieve | Tastiere | Organo | Accordo |
| 10 | Organo rotante | Tastiere | Organo | Accordo |
| 11 | Chitarra di carta | Corde | Analogico | Strum |
| 12 | Dodici corde | Corde | Corda pizzicata | Strum |
| 13 | Koto del lago | Corde | Corda pizzicata | Slop |
| 14 | Arpa celtica | Corde | Corda pizzicata | Arpa |
| 15 | Arpa di vetro | Corde | FM | Arpa |
| 16 | Pizzicato | Corde | Corda pizzicata | Accordo |
| 17 | Cascata | Arpeggi | Analogico | Arpeggio |
| 18 | Pioggia | Arpeggi | Corda pizzicata | Arpeggio |
| 19 | Ritmo in levare | Arpeggi | Analogico | Pattern |
| 20 | Treno notturno | Arpeggi | Ensemble | Pattern |
| 21 | Campane lunari | Campane | FM | Strum |
| 22 | Carillon | Campane | FM | Arpeggio |
| 23 | Ottoni | Ottoni | Analogico | Strum |
| 24 | Fanfara | Ottoni | Ensemble | Strum |

### I file dei tuoi suoni
Ogni suono tuo è un file di testo in `Saves/openorc/suoni/`, per esempio `nebbia-2.txt`:

```
nome=Nebbia 2
engine=0
tone=0.3
...
stereo=0.5
spread=2
```

Si possono copiare su un'altra console, condividere e ritoccare con un editor di testo. OpenOrc li
rilegge all'avvio. Ne tiene fino a 64.

## 9. Pagina Ritmo
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
| Y | loop: annulla |

### Il looper
Funziona come un looper a pedale e registra quello che suoni: accordi e note. Il loop suona con il suono
corrente: cambiando suono o manopole cambia anche lui. Gli stessi comandi sono su SELECT + A, B, Y e sui
pad 45, 46, 47.

1. **Registra** (A). A trasporto fermo parte subito. In moto aspetta l'inizio della prossima battuta: se
   premi appena dopo la battuta, parte da quella.
   - Mentre registri, i piatti fanno da metronomo.
   - Ripremere prima che parta annulla.
2. **Registra di nuovo** per chiudere il giro: la lunghezza si arrotonda alla battuta più vicina e il loop
   comincia a suonare. Se arrivi alla lunghezza massima, il giro si chiude da solo.
3. Mentre suona, **Registra** passa alla **sovraincisione**: aggiungi altro sopra. Ripremi per tornare
   ad ascoltare. Ogni sovraincisione è uno **strato** a sé.
4. **Suona/ferma** (B) lo ferma e lo riavvia.
5. **Annulla** (Y) torna indietro di un passo, come nei looper a pedale:
   - durante una sovraincisione, toglie quella in corso;
   - poi toglie le sovraincisioni, dall'ultima alla prima;
   - con il solo primo giro, cancella il loop.

La quantizzazione allinea alla griglia scelta quello che registri. Il loop contiene fino a 1024 eventi:
oltre compare «pieno: niente altre note». Il loop e quello che suoni dal vivo hanno ciascuno il proprio
accordo; il basso segue l'ultimo accordo partito. All'uscita il loop si salva e alla riapertura lo
ritrovi fermo; una sovraincisione lasciata a metà non si salva.

## 10. Pagina MIDI
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
[scheda della MPK](tastiere/akai-mpk-mini-iv.it.md#altre-tastiere). I preset scritti prima di ottobre 2026
con la riga «Loop: cancella» funzionano ancora: oggi si chiama «Loop: annulla».

## 11. File

| File | Contenuto |
|---|---|
| `Saves/openorc/stato.txt` | impostazioni e suono corrente; cancellalo per tornare ai valori di fabbrica |
| `Saves/openorc/loop.txt` | il loop dell'ultima sessione |
| `Saves/openorc/suoni/*.txt` | i tuoi suoni |
| `Saves/openorc/tastiere/*.txt` | i tuoi preset della tastiera |
| `App/OpenOrc/tastiere/` | preset e scheda della MPK mini IV |
| `Saves/spruce/openorc.log` | messaggi dell'ultima esecuzione, utili se qualcosa non va |

## 12. Se qualcosa non va

| Problema | Cosa fare |
|---|---|
| Non si sente nulla | Controlla il volume della console e la riga Volume (o K8). Se l'oscilloscopio si muove, il suono c'è. |
| Una nota resta accesa | **L3**, oppure il pedale su e giù. |
| La tastiera non fa nulla | Pagina MIDI: la spia deve essere verde. Se «Ultimo messaggio» cambia, la tastiera arriva: rifai l'apprendimento. |
| Gli accordi della MPK escono strani o doppi | Spegni Chords, Scales, ARP e Note Repeat sulla MPK. |
| Una manopola va al contrario o a scatti | Prova un altro «Tipo di manopole» nella pagina MIDI. |
| Arpeggio o pattern fuori tempo rispetto a un'altra macchina | Mandagli il clock MIDI e attiva «Segui il clock MIDI». |
| Il tempo non cambia | Arriva un clock MIDI: il tempo lo detta lui. |
| Cambiando suono cambia anche l'arpeggio | Pagina Accordi, «Cambia con il suono»: No. |
| Rinomina non scrive nulla | Serve una tastiera USB collegata alla Flip; senza, il nome resta quello automatico. |
| La corda pizzicata si spegne subito | Alza Carattere (Durata) e tieni il Sostegno al massimo. |

## 13. Limiti attuali
- I quattro stili di batteria sono fissi.
- C'è un solo loop; non si salvano più loop con nomi diversi.
- Il volume non fa parte del suono: un suono salvato più piano o più forte non si ricorda il volume.
- Il nome di un suono si scrive solo con una tastiera USB.
