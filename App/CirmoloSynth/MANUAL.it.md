# Cirmolo Synth — manuale

*[English](MANUAL.md) · Italiano* · Le schermate di tutte le pagine sono nella [pagina illustrata](https://luigismith.github.io/cirmolo/synth/) del sito.

Cirmolo Synth è una groovebox per la Miyoo Flip: una drum machine con un sequencer potente, un basso acido e un synth da melodie, tutto in una console. Non usa campioni: ogni suono è sintetizzato al momento. Lo trovi nella Home, sotto **Musica**.

Suonano insieme **14 tracce**:

- **12 batterie**, una per traccia: casse, rullanti, battimani, charleston, piatti, tom, conga, campanacci, suoni a 8 bit e altro. Ogni traccia usa uno dei **34 modelli** di strumento; un **kit** (ce ne sono **26**, dalla 808 alla Game Boy) sceglie i 12 strumenti e li regola insieme;
- **Acido**, un basso monofonico alla 303: dente di sega o quadra, filtro risonante a 4 poli, accento e slide;
- **Piega**, un synth West Coast a 4 voci: oscillatore complesso con wavefolder e low-pass gate, per note morbide o percussive.

Ogni traccia ha fino a 64 passi con la sua lunghezza e la sua velocità; ogni passo può avere probabilità, condizioni, ripetizioni rapide, spostamento fine e valori bloccati. Ci sono 16 pattern da mettere in catena, **33 ritmi pronti**, un mixer con eco e riverbero, una pagina **Live** in cui i 12 tasti della Flip diventano pad, il filtro DJ e lo stutter sulle levette, la registrazione e l'esportazione in WAV. Con una tastiera o dei pad MIDI USB si suona e si regola tutto dal controller, e la Flip manda il clock MIDI agli strumenti collegati.

Questo manuale descrive la versione del ramo `cirmolo` di ottobre 2026. Lo schermo è in italiano.

## In breve

| Vuoi... | Fai così |
|---|---|
| sentire subito qualcosa | **START**: al primo avvio il pattern 1 ha già un ritmo house, un giro di acido e qualche nota di Piega |
| cambiare pagina | **SELECT** (Ritmo → Suoni → Mixer → Live → Pattern → Ritmo) |
| caricare un altro ritmo | pagina **Pattern**, destra fino all'elenco **Ritmi pronti**, scegli e premi **A** |
| accendere o spegnere un passo | pagina **Ritmo**, croce sul passo, **A** |
| cambiare la forza di un colpo | pagina **Ritmo**, **B** tenuto + su/giù |
| regolare un passo nei dettagli | pagina **Ritmo**, **X** apre la scheda del passo |
| cambiare kit | pagina **Suoni**, **L2/R2** |
| suonare le batterie con i tasti | pagina **Live**: i 12 tasti sono 12 pad |
| filtro DJ, stutter, fill | levetta sinistra di lato; levetta destra su (stutter) o giù (fill), in ogni pagina |
| registrare quello che senti | **R3** (premi la levetta destra); di nuovo R3 ferma |
| esportare il pattern in WAV | pagina **Pattern**, **R1** |
| uscire | **MENU**, poi A |

## 1. Avvio, uscita e salvataggio

- Mentre Cirmolo Synth è aperto la console non va in sospensione.
- Si esce con **MENU**: compare «Uscire dal synth?». **A** o giù esce, **B** o su resta.
- Tutto si salva da solo: suoni, kit, mixer, i 16 pattern, la catena e la pagina in cui eri. Il salvataggio avviene all'uscita e, se hai cambiato qualcosa, ogni 30 secondi. Alla prossima apertura ritrovi tutto com'era, fermo.
- Al primo avvio (o se il file dello stato non c'è) il pattern 1 contiene il ritmo pronto «Cassa dritta house» con il kit House a 124 BPM, una linea di Acido di 16 passi e quattro note lunghe di Piega. Gli altri 15 pattern sono vuoti.
- Lo stato del vecchio Cirmolo Synth (il sintetizzatore a otto voci di prima) non si importa: la prima volta il nuovo synth riparte da capo e lo sostituisce.

## 2. Le pagine e i comandi comuni

In alto ci sono le cinque pagine; quella attiva è evidenziata. A destra **P1 124** dice quale pattern sta suonando e il tempo; il pallino verde pulsa a ogni battito mentre suona, il quadratino grigio vuol dire fermo. In basso c'è sempre una riga con i tasti della pagina.

| Pagina | A cosa serve |
|---|---|
| **Ritmo** | la griglia: 14 tracce per 16 passi, la scheda di ogni passo |
| **Suoni** | il kit e i parametri dello strumento di ogni traccia |
| **Mixer** | volume, pan, mandate a eco e riverbero, muto e solo; il master con tempo, swing ed effetti |
| **Live** | i 12 tasti della Flip suonano le 12 batterie, le registrano o le zittiscono |
| **Pattern** | 16 pattern, catena, copia e incolla, ritmi pronti, esportazione in WAV |

### Comandi validi in tutte le pagine

| Tasto | Cosa fa |
|---|---|
| **SELECT** | pagina successiva |
| **START** | suona o ferma; parte sempre dall'inizio del pattern (o della catena) |
| **R3** (premi la levetta destra) | registra l'uscita in WAV; di nuovo R3 ferma (capitolo 10) |
| **L3** (premi la levetta sinistra) | spegne tutte le note; nella pagina Live cambia modo |
| **MENU** | esce, con conferma |
| levetta sinistra, di lato | filtro DJ (capitolo 9) |
| levetta destra in su / in giù | stutter / fill (capitolo 9) |

La traccia scelta è una sola per Ritmo, Suoni e Mixer: se ti sposti sull'Acido nella griglia, in Suoni trovi già l'Acido.

## 3. Primi passi

1. Apri Cirmolo Synth e premi **START**: suona il pattern 1. Premi di nuovo START per fermare.
2. Con la musica che va, premi **SELECT** fino alla pagina **Pattern**. Premi **destra** finché il fuoco passa all'elenco **Ritmi pronti** a destra, scegli un ritmo con su/giù (il numero accanto è il tempo) e premi **A**: il ritmo entra nel pattern che stai modificando, con il suo kit, il suo tempo e il suo swing. Sinistra torna alla griglia dei pattern.
3. **SELECT** fino a **Ritmo**: vedi i passi del ritmo. Sposta il cursore con la croce e premi **A** per accendere o spegnere un passo. Tieni **B** e premi su o giù per cambiarne la forza.
4. Scendi sulla riga **Acido**, accendi qualche passo con **A** e, con **B** tenuto, alza o abbassa la nota (su/giù) o l'ottava (sinistra/destra).
5. Nella pagina **Live** suona sopra al ritmo con i tasti e prova le levette: la sinistra filtra, la destra in su fa lo stutter, in giù il fill.
6. Quando ti piace, **R3** registra quello che senti; nella pagina Pattern **R1** esporta il pattern in WAV.

## 4. Pagina Ritmo

In alto: il numero del pattern che modifichi, il suo nome (quello del ritmo pronto, se l'hai caricato), la pagina di passi (per esempio 1/2) e il kit. Se sta suonando un altro pattern compare «(suona un altro)».

### Come si legge la griglia

Una riga per traccia: le 12 batterie, una linea, poi Acido e Piega. A sinistra il nome dello strumento, con un pallino che si accende a ogni colpo (grigio se la traccia è muta). Le colonne sono 16 passi, divisi in gruppi di quattro; la colonna più chiara è quella che sta suonando, il riquadro ambra è il cursore.

- Il **colore** dice il tipo di strumento: rosso le casse, ambra rullanti, rim e battimani, azzurro charleston, piatti, shaker e tamburelli, verde tom, conga, bongo e tabla, rosa campanacci, claves e legnetti, viola gli effetti (zap, glitch, soffio). L'Acido è verde lime, Piega viola.
- La **luminosità** di un passo acceso è la sua dinamica: più è chiaro, più è forte. Nelle tracce melodiche un passo con l'accento è ancora più chiaro.
- **Puntini** in basso: le ripetizioni (2, 3 o 4 colpi nel passo).
- **Angolo tagliato** in alto a sinistra: la probabilità è sotto il 100%.
- **Pallino** in alto a destra: c'è una condizione; ambra se è «Solo nel fill».
- **Trattino** in basso: il passo ha dei valori bloccati.
- **Tacca** in alto, spostata dal centro: il passo è spostato in anticipo o in ritardo.
- Nell'**Acido** e in **Piega** il passo mostra il nome della nota; un trattino a destra è lo slide (Acido), una barretta a destra una nota lunga più di un passo (Piega).
- Le caselle più scure sono oltre la fine della traccia. Se una traccia non è lunga 16 passi, o va a un'altra velocità, a destra della riga compare la sua lunghezza con un segno: **»** velocità doppia, **«** metà, **³** terzine.

La griglia mostra tante pagine da 16 quante servono alla traccia più lunga del pattern (fino a 4, cioè 64 passi).

### Tasti

| Tasto | Cosa fa |
|---|---|
| Croce | sposta il cursore (passo e traccia); ai bordi ricomincia dall'altra parte |
| **A** | accende o spegne il passo; da fermo fa anche sentire il suono |
| **B** tenuto + croce | cambia il passo al volo (sotto) |
| **X** | apre la scheda del passo e della traccia |
| **Y** | fa sentire la traccia (nelle melodiche, la nota del passo) |
| L1 / R1 | pagina di 16 passi precedente / successiva |
| L2 / R2 | pattern precedente / successivo da modificare |

Accendendo un passo dell'Acido o di Piega si mette l'ultima nota che hai usato su quella traccia. Se accendi un passo oltre la fine della traccia, la traccia si allunga fino alla fine di quella pagina.

Senza catena, il pattern che modifichi è anche quello che suona: cambiando pattern con L2/R2 mentre suona, il cambio avviene alla fine del giro in corso.

### B tenuto + croce

Il modo più rapido per lavorare sui passi. Se il passo è spento, lo accende.

| | Batterie | Acido e Piega |
|---|---|---|
| B + su / giù | dinamica più forte / più piano | nota su / giù di un semitono |
| B + destra / sinistra | una ripetizione in più / in meno (da 1 a 4) | un'ottava su / giù |

Mentre tieni B la riga in basso ricorda cosa fa la croce.

### La scheda del passo (X)

**X** apre a destra la scheda del passo sotto il cursore, con in fondo i valori di tutta la traccia.

| Tasto | Cosa fa |
|---|---|
| su / giù | sceglie la riga |
| sinistra / destra | cambia il valore |
| L1 / R1 (premuti, o tenuti con sinistra/destra) | cambia a passi grandi |
| **A** | esegue le righe con (A); su Accento e Slide accende o spegne |
| **Y** | fa sentire il passo |
| **B** o **X** | chiude la scheda |

Cambiare un valore del passo lo accende, se era spento (tranne lo Spostamento).

**Righe del passo**

| Riga | Valori | Cosa fa |
|---|---|---|
| Nota | da Do1 a Do7 (Acido e Piega) | la nota del passo; L1/R1 un'ottava |
| Accento | sì / no (Acido e Piega) | nota più forte; nell'Acido apre anche il filtro, quanto dice «Accento» in Suoni |
| Slide verso il passo dopo | sì / no (Acido) | la nota scivola legata nella successiva, come nella 303 |
| Durata (passi) | da 1 a 16 (Piega) | quanti passi dura la nota |
| Dinamica | da 1 a 127 | la forza del colpo (a passi di 4; L1/R1 di 16) |
| Probabilità | da 1 a 100% | quante volte su cento il passo suona (a passi di 5; L1/R1 di 25) |
| Condizione | vedi sotto | in quali giri il passo suona |
| Ripetizioni | da 1 a 4 | colpi rapidi dentro il passo (ratchet) |
| Spostamento | da −50% a +50% | anticipa (−) o ritarda (+) il passo, in percentuale di un passo |
| Altezza solo qui | ±24 semitoni (batterie) | intonazione di questo colpo soltanto |
| Durata solo qui | 0–100% | durata di questo colpo soltanto |
| Colore solo qui | 0–100% | il colore di questo colpo; nell'Acido diventa **Taglio solo qui** (il filtro), in Piega **Pieghe solo qui** |

I tre valori «solo qui» partono da «come la traccia»: il primo destra mette un valore (0 semitoni, oppure 50%), e scendendo oltre il minimo si torna a «come la traccia».

**Righe della traccia** (sotto la scritta «Traccia», valgono per tutta la riga del pattern)

| Riga | Cosa fa |
|---|---|
| Lunghezza | da 1 a 64 passi (L1/R1 di 16) |
| Velocità | normale (sedicesimi), doppia (trentaduesimi), metà (ottavi), terzine (12 passi per battuta) |
| Euclideo: colpi | quanti colpi distribuire sulla lunghezza della traccia |
| Euclideo: rotazione | di quanti passi spostarli a destra |
| Applica l'euclideo (A) | riscrive la traccia: i colpi più distribuiti possibile, con valori normali |
| Sposta tutto (sin./des.) | fa scorrere tutti i passi della traccia di un passo a sinistra o a destra |
| Svuota la traccia (A) | spegne tutti i passi della traccia e li rimette ai valori normali |

### A cosa servono

- **Probabilità.** Un charleston al 50% suona un giro sì e uno no, a caso: il ritmo non si ripete mai identico. Probabilità basse sui colpi fantasma danno un groove vivo senza fatica.
- **Condizioni.** Decidono in quali giri il passo suona:

  | Condizione | Il passo suona... |
  |---|---|
  | Sempre | sempre |
  | Solo nel fill | solo mentre tieni il fill (levetta destra in giù, o il pad 13 della MPK mini Plus) |
  | Non nel fill | sempre, tranne quando tieni il fill |
  | 1° giro su 2 | nei giri dispari del pattern |
  | 2° giro su 2 | nei giri pari |
  | 1 giro su 4 | nel primo giro di ogni quattro |
  | Solo il 1° giro | solo nel primo giro dopo la partenza (o dopo il cambio di pattern) |

  I giri si contano da quando il pattern è partito; quando si passa a un altro pattern il conto ricomincia (in una catena, per sentire il «2° giro su 2» metti lo stesso pattern due volte di seguito). Con «1° giro su 2» e «2° giro su 2» un pattern di una battuta diventa una frase di due; con «Solo nel fill» prepari rullate che senti solo quando le chiedi.
- **Ripetizioni.** Dividono il passo in 2, 3 o 4 colpi uguali: le raffiche dei charleston trap, le rullate.
- **Spostamento.** Un rullante in ritardo del 10–20% suona più pigro, un charleston in anticipo più urgente. Va oltre lo swing, che sposta tutti i sedicesimi pari allo stesso modo.
- **Valori bloccati** («solo qui»). Lo stesso strumento cambia da un passo all'altro: un tom che sale su quattro passi, un charleston aperto più lungo solo a fine battuta, un taglio dell'Acido che si apre su una nota sola.
- **Euclideo.** Distribuisce N colpi su una lunghezza nel modo più regolare possibile: 3 su 8 è il tresillo, 5 su 8 il cinquillo, 5 su 16 o 7 su 16 danno ritmi africani e latini. La rotazione sposta l'inizio.
- **Lunghezze diverse.** Ogni traccia gira con la sua lunghezza: una riga di 12 passi contro una di 16 si ritrova al punto di partenza solo ogni 48 passi, e il ritmo cambia continuamente. Il giro del pattern è la traccia più lunga fra quelle con passi accesi: le tracce che ci stanno un numero intero di volte ripartono insieme, le altre continuano libere. Il ritmo pronto «Poliritmo 3 su 4» ne è un esempio.
- **Velocità.** Doppia per le raffiche di trentaduesimi, metà per una traccia lenta che dura due battute con 16 passi, terzine per lo shuffle e il jazz (lo swing non tocca le terzine).

## 5. Pagina Suoni

A sinistra il kit e le 14 tracce, a destra i parametri della traccia scelta, con il nome del modello in alto e sotto l'oscilloscopio dell'uscita.

| Tasto | Cosa fa |
|---|---|
| su / giù | sceglie il parametro |
| sinistra / destra | lo cambia; con **B** tenuto a passi grandi |
| L1 / R1 | traccia precedente / successiva |
| L2 / R2 | kit precedente / successivo |
| **A** | fa sentire la traccia |
| **Y** | variazione: sposta un poco, a caso, i valori del suono (non volume, altezza e pan) |

### Kit

Un kit sceglie e regola i 12 strumenti delle batterie. Cambiare kit sostituisce i 12 strumenti con tutti i loro valori, compresi quelli che avevi cambiato; non tocca i pattern, il mixer, l'Acido e Piega. Il kit vale per tutti i 16 pattern. L'elenco completo è nel capitolo 13.

Nei kit le tracce seguono quasi sempre lo stesso ordine: 1 cassa, 2 rullante, 3 rim, 4 battimani, 5 charleston chiuso, 6 charleston aperto, 7 tom basso, 8 tom alto, 9 piatto o ride, 10–12 percussioni. Così un ritmo funziona anche con un altro kit.

### Parametri delle batterie

| Parametro | Cosa fa |
|---|---|
| Strumento | uno dei 34 modelli; cambiandolo la traccia prende il nome del modello e lo senti subito |
| Altezza | ±24 semitoni rispetto all'altezza tipica del modello |
| Durata | da cortissima a lunghissima, per quel modello |
| Colore | filtro e brillantezza, o il rapporto fra corpo e rumore |
| Attacco | il clic, lo schiocco o la caduta d'altezza iniziale (il senso dipende dal modello) |
| Spinta | saturazione |
| Volume | il volume dello strumento |
| Pan | da sinistra a destra (è lo stesso pan del mixer) |
| Gruppo che zittisce | da 1 a 4, oppure nessuno: le tracce dello stesso gruppo si zittiscono a vicenda, come il charleston chiuso che taglia quello aperto |

**I 34 modelli**

| Famiglia | Modelli |
|---|---|
| Casse | Cassa 808, Cassa 909, Cassa acustica, Cassa FM, Cassa gabber |
| Rullanti | Rullante 808, Rullante 909, Rullante acustico, Spazzola |
| Colpi secchi | Rim, Battimani, Schiocco di dita |
| Charleston e piatti | Charleston chiuso, Charleston aperto, Charleston FM, Piatto crash, Piatto ride |
| Tom | Tom, Tom elettronico |
| Pelli | Conga, Bongo, Tabla |
| Percussioni | Campanaccio, Claves, Legnetto, Shaker, Tamburello, Triangolo |
| 8 bit | Cassa 8 bit, Rullante 8 bit, Charleston 8 bit |
| Effetti | Zap, Glitch, Soffio |

### Acido

Un basso monofonico alla 303. Nel sequencer conta lo slide: due passi accesi con lo slide sul primo diventano una nota sola che scivola.

| Parametro | Cosa fa |
|---|---|
| Onda | dente di sega o quadra |
| Altezza | ±24 semitoni su tutta la traccia |
| Taglio | la frequenza del filtro a 4 poli |
| Risonanza | il fischio del filtro |
| Inviluppo | quanto l'inviluppo apre il filtro a ogni nota |
| Durata | quanto dura l'inviluppo |
| Accento | quanto pesano gli accenti, sul filtro e sul volume |
| Spinta | saturazione |
| Volume, Pan | |

### Piega

Un synth West Coast a 4 voci: un oscillatore modulato da un secondo oscillatore, piegato dal wavefolder e chiuso da un low-pass gate. Con poche pieghe è morbido e rotondo, con tante diventa brillante e metallico; con un gate corto suona come una percussione accordata.

| Parametro | Cosa fa |
|---|---|
| Altezza | ±24 semitoni |
| Pieghe | quante volte l'onda si ripiega: il timbro |
| Simmetria | al 50% le pieghe sono simmetriche; spostandola il suono si fa più nasale |
| Rapporto | il rapporto fra il modulatore e la nota (da 0,5 a 8; nella prima parte a scatti armonici) |
| Modulazione | quanto il modulatore muove l'oscillatore |
| Durata | quanto resta aperto il low-pass gate |
| Colore del gate | a 0 il gate cambia solo il volume, a 100% chiude anche il filtro (più «bongo») |
| Rilascio | quanto la nota continua dopo la fine dei suoi passi |
| Volume, Pan | |

## 6. Pagina Mixer

Una striscia per traccia (1–12, **Ac** l'Acido, **Pi** Piega) e a destra il **master**. In ogni striscia: il livello che suona (verde; grigio se la traccia non si sente per un muto o un solo), il fader del volume, le barrette di pan (azzurra), eco (viola) e riverbero (rosa), i tasti **M** (muto) e **S** (solo). In alto la traccia scelta e il valore che stai cambiando.

| Tasto | Cosa fa |
|---|---|
| sinistra / destra | sceglie la striscia; dopo **Pi** c'è il master |
| L1 / R1 | sulle tracce sceglie la riga: Volume, Pan, Eco, Riverbero; sul master sceglie il parametro |
| su / giù | cambia il valore; con **B** tenuto a passi grandi |
| **A** | muto sì/no |
| **Y** | solo sì/no: se c'è almeno un solo, si sentono solo le tracce in solo |
| **X** | fa sentire la traccia |

Il volume del mixer agisce sopra quello dello strumento (pagina Suoni); il pan invece è lo stesso nelle due pagine.

### Il master

| Parametro | Cosa fa |
|---|---|
| Tempo | da 40 a 240 BPM (B tenuto: di 10 in 10) |
| Swing | da 0 a 75%: ritarda i sedicesimi pari; al 33% sono terzine |
| Eco: tempo | da 1/16 a 8/16: l'eco ping-pong, a tempo |
| Eco: ritorni | quante volte l'eco ritorna |
| Eco: colore | da scuro a chiaro |
| Riverbero | la grandezza della stanza |
| Smorzamento | quanto il riverbero perde gli acuti |
| Compressore | compressione di tutto il mix |
| Pompa | il mix «respira» sulla cassa (la traccia 1), come un sidechain |
| Filtro DJ | il punto di partenza del filtro: «bassi» (passa-basso), «aperto», «alti» (passa-alto); la levetta sinistra parte da qui |
| Risonanza | la risonanza del filtro DJ |
| Lo-fi | riduce bit e frequenza: suono sporco, da campionatore vecchio |
| Nastro | saturazione e un leggero ondeggiare, come un registratore a nastro |
| Volume | il volume generale |
| **Clock MIDI in uscita** | sì / no (su o giù per cambiare): manda clock, Start e Stop al dispositivo MIDI collegato (capitolo 11). È acceso all'inizio |

Il tempo e lo swing valgono per tutti i pattern; caricando un ritmo pronto prendono i suoi valori.

## 7. Pagina Live

I 12 tasti della Flip diventano 12 pad, uno per batteria, disposti come sulla console:

| Tasti | Tracce |
|---|---|
| croce sinistra, giù, destra, su | 1, 2, 3, 4 (di solito cassa, rullante, rim, battimani) |
| Y, B, A, X | 5, 6, 7, 8 (charleston chiuso e aperto, tom basso e alto) |
| L2, L1, R1, R2 | 9, 10, 11, 12 (piatto e percussioni) |

Ogni pad mostra lo strumento e il tasto, e si illumina a ogni colpo, anche quelli del sequencer. **L3** cambia modo:

| Modo | I tasti... |
|---|---|
| **Suona** | suonano lo strumento |
| **Suona e registra** | suonano e, mentre il sequencer va, scrivono il colpo nel pattern che sta suonando, sul passo più vicino |
| **Zittisci** | accendono e spengono il muto della traccia (sul pad compare «muto») |

In alto a destra: il pattern che suona, il giro e il kit. In basso: il filtro DJ, lo stutter e il fill delle levette, e l'oscilloscopio. Il modo resta anche quando cambi pagina; in «Suona e registra» anche i pad MIDI registrano.

## 8. Pagina Pattern

A sinistra i 16 pattern: numero, nome (se viene da un ritmo pronto) o «vuoto», e una barretta colorata per ogni traccia che ha passi accesi. Il pattern viola è quello che modifichi, il riquadro ambra è il cursore, il pallino verde dice quale sta suonando. Sotto c'è la **catena**, a destra l'elenco dei **ritmi pronti**.

| Tasto | Cosa fa |
|---|---|
| Croce | sceglie il pattern; destra dall'ultima colonna passa ai ritmi pronti |
| **A** | modifica questo pattern (senza catena è anche quello che suona, dalla fine del giro); se la catena è accesa, la spegne |
| **X** | aggiunge il pattern alla catena |
| **L1** | toglie l'ultimo pattern dalla catena |
| **Y** | copia il pattern; poi **Y** su un altro pattern lo incolla |
| **B**, poi ancora **B** | svuota il pattern (il primo B chiede conferma; un altro tasto annulla) |
| **R1** | esporta in WAV |

### La catena

Con la catena accesa i pattern suonano nell'ordine della catena, a ripetizione, ognuno per un giro; lo stesso pattern può comparire più volte (1 1 2 3 3 4). Può contenere fino a 64 pattern; il riquadro ne mostra gli ultimi 24 e il pattern che suona è in verde. Si cambia anche mentre suona: dalla fine del giro riparte dal primo. Con la catena accesa L2/R2 nella pagina Ritmo scelgono solo il pattern da modificare, non quello che suona. Per spegnerla: **A** su un pattern, oppure **L1** fino a vuotarla.

### Copia, incolla, svuota

La copia prende tutte le 14 tracce del pattern e il suo nome; incollando si sovrascrive il pattern di destinazione. Svuotare spegne tutti i passi, rimette le tracce a 16 passi e velocità normale e toglie il nome. Non c'è un annulla: prima di svuotare, copia il pattern in uno libero.

### Ritmi pronti

L'elenco ha 33 ritmi, con il tempo accanto (capitolo 14). Su/giù sceglie, L1/R1 salta di 10, **A** carica, sinistra torna ai pattern. Caricare un ritmo:

- riscrive le **12 tracce di batteria** del pattern che stai modificando (l'Acido e Piega restano) e gli dà il nome del ritmo;
- carica il **kit** consigliato, che vale per tutti i pattern;
- imposta **tempo** e **swing** del ritmo.

Ogni ritmo usa probabilità, condizioni e ripetizioni: tieni il fill (levetta destra in giù) per sentire le rullate scritte apposta.

### Esportare in WAV

**R1** esporta senza fermare la musica, in un motore separato:

- **senza catena**: il pattern che modifichi, per 4 giri;
- **con la catena**: un giro completo della catena.

Si aggiungono due secondi di coda per l'eco e il riverbero; al massimo 10 minuti. Durante l'esportazione la riga in basso mostra i secondi già scritti; alla fine compare il nome del file. Il WAV è stereo, 48 kHz, 16 bit, in `Saves/cirmolo-synth/registrazioni/`, con nomi come `pattern-2026-10-13-213000.wav` o `catena-2026-10-13-213000.wav`. Valgono muti, soli e il filtro DJ del mixer; le levette no.

## 9. Le levette: filtro DJ, stutter, fill

Funzionano in tutte le pagine.

- **Levetta sinistra, di lato: filtro DJ.** A sinistra chiude il passa-basso (restano i bassi), a destra apre il passa-alto (restano gli alti). Lasciata al centro, il filtro torna al valore del mixer. La rotella del pitch o il joystick di una tastiera MIDI fanno lo stesso.
- **Levetta destra in su: stutter.** Ripete l'ultimo frammento di musica finché la tieni: poco in su ripete un quarto, poi un ottavo, un sedicesimo e, tutta in cima, un trentaduesimo. Lasciandola, la musica riprende dove sarebbe arrivata.
- **Levetta destra in giù (oltre metà): fill.** Finché la tieni suonano i passi «Solo nel fill» e tacciono quelli «Non nel fill».

Lo stato delle levette si vede in fondo alla pagina Live.

## 10. Registrare (R3)

**R3** (premi la levetta destra) comincia a registrare tutto quello che esce dal synth, levette comprese: in basso compare una barra rossa con **REC** e il tempo. Di nuovo **R3** ferma e salva. I file sono WAV stereo a 48 kHz e 16 bit, in `Saves/cirmolo-synth/registrazioni/`, con nomi come `registrazione-2026-10-13-213000.wav`. Uscendo, una registrazione in corso si chiude da sola.

## 11. MIDI

Collega una tastiera o dei pad MIDI USB alla porta USB-C, anche ad app già aperta: compare «MIDI collegato» con il nome del dispositivo. I tasti della Flip continuano a funzionare insieme al controller.

| Messaggio | Cosa fa |
|---|---|
| Note sul **canale 10**, 36–47 | le 12 batterie in ordine: 36 la traccia 1, 37 la 2... 47 la 12 (i pad dei controller) |
| Altre note sul canale 10 | come nel General MIDI: 35 cassa; 48 e 50 tom alto (traccia 8); 49, 51, 52, 53, 55, 57, 59 piatti (traccia 9); 56 campanaccio (traccia 10); 54, 58 e 60–70 percussioni (traccia 11); tutte le altre la traccia 12 |
| Note sugli altri canali | suonano l'**Acido** o **Piega**: quella delle due che hai scelto per ultima nella pagina Ritmo o Suoni (all'avvio l'Acido) |
| Dinamica | la forza del colpo o della nota |
| Manopole **CC 70–77** | i primi otto valori continui della traccia scelta (sotto) |
| Manopole **CC 24–31** | lo stesso, anche con encoder infiniti (valori relativi) |
| **Pitch bend** | filtro DJ, come la levetta sinistra |
| **Modulazione (CC 1)** | stutter, come la levetta destra in su |
| **Program change** | sceglie il kit: 0 è 808 Classica, 1 909 Club... nell'ordine del capitolo 13 |
| **Start**, Continue / **Stop** | fanno partire (dall'inizio) e fermare il sequencer |
| CC 120 e 123 | spengono tutte le note |

**Le manopole** regolano la traccia scelta (in Ritmo, Suoni o Mixer) e, mentre giri, la pagina Suoni mostra il parametro che cambia. Contano solo i valori continui, nell'ordine della pagina Suoni:

| Manopola | Batterie | Acido | Piega |
|---|---|---|---|
| 1 | Altezza | Altezza | Altezza |
| 2 | Durata | Taglio | Pieghe |
| 3 | Colore | Risonanza | Simmetria |
| 4 | Attacco | Inviluppo | Rapporto |
| 5 | Spinta | Durata | Modulazione |
| 6 | Volume | Accento | Durata |
| 7 | Pan | Spinta | Colore del gate |
| 8 | Riverbero (mandata) | Volume | Rilascio |

Con le CC 70–77 la manopola porta il valore alla sua posizione. Con le CC 24–31 il synth capisce da solo se il controller manda posizioni assolute o spostamenti (encoder): finché arrivano solo valori vicini a 0 o a 127 li tratta come spostamenti, al primo valore intermedio passa alle posizioni.

Le note suonate dalla tastiera sull'Acido e su Piega non si registrano nel pattern; i pad delle batterie sì, se la pagina Live è in «Suona e registra» e il sequencer va.

Se il dispositivo ha più porte MIDI, note e pad arrivano dalla prima; dalle altre si leggono le manopole e Start/Stop.

### Clock MIDI in uscita

Con **Clock MIDI in uscita** acceso (nel master del mixer; è acceso all'inizio) e un dispositivo collegato, la Flip fa da direttore:

- START sulla Flip manda **Start** MIDI e poi il **clock** a 24 impulsi per battito, al tempo del synth, swing escluso;
- fermando con START manda **Stop**.

Il clock parte e si ferma con il sequencer e segue i cambi di tempo. La Flip non segue un clock esterno: Start e Stop ricevuti la fanno partire e fermare, ma il tempo è sempre il suo. Uno Start o uno Stop ricevuti non vengono rimandati indietro.

## 12. Akai MPK mini Plus

Cirmolo Synth riconosce la **MPK mini Plus** dal nome e le dà qualche funzione in più. Il resto funziona come con qualunque controller (capitolo 11).

| Comando della MPK | Cosa fa |
|---|---|
| Pad 1–12 (note 36–47, canale 10) | le 12 batterie |
| **Pad 13** (nota 48, canale 10), tenuto | **fill** |
| **Pad 14** (nota 49), tenuto | **stutter 1/8** |
| **Pad 15** (nota 50), tenuto | **stutter 1/16** |
| **Pad 16** (nota 51), tenuto | **stutter 1/32** |
| Tastiera | Acido o Piega |
| **Joystick** (pitch bend) | filtro DJ |
| **Rotella della modulazione** | stutter, più fitto quanto più la alzi |
| Manopole | gli otto parametri della traccia scelta |

Con la MPK mini Plus le note 48–51 sul canale 10 servono a fill e stutter e non suonano tom e piatti. Se tieni più pad di stutter insieme vale il più fitto. I pad devono mandare sul canale 10.

**Il clock.** Con il clock MIDI in uscita acceso, la Flip dà il tempo alla MPK:

- l'**arpeggiatore** e il **sequencer** della MPK, impostati sul clock esterno (*External*), seguono il tempo della Flip e partono con START;
- la presa **Clock Out** della MPK ripete il clock verso synth e drum machine analogici: la MPK fa da ponte fra la Flip e gli strumenti che hanno solo un ingresso di clock.

> **Non ancora verificato sulla tastiera vera.** I **numeri delle manopole** e dei **tasti di trasporto** (Play e Stop) della MPK mini Plus non sono ancora stati controllati su una MPK mini Plus. Il synth risponde alle manopole sulle CC 70–77 o 24–31 e ai messaggi MIDI Start e Stop: se la MPK manda altri numeri o altri messaggi, le manopole e i tasti di trasporto non fanno niente. Il resto (pad 13 e joystick, clock in uscita) è provato con le prove automatiche sul PC; come l'arpeggiatore, il sequencer e la presa Clock Out della MPK seguono il clock dipende dalle impostazioni della MPK.

## 13. I 26 kit

L2/R2 nella pagina Suoni li scorre in quest'ordine; il program change MIDI li sceglie con il numero (da 0).

| N. | Kit | Carattere |
|---|---|---|
| 0 | 808 Classica | la drum machine dell'hip hop e dell'elettronica: cassa lunga, campanaccio, conga |
| 1 | 909 Club | house e techno: cassa decisa, charleston metallici, ride e crash |
| 2 | 606 Tic | piccola e secca, per l'acid house |
| 3 | CR-78 Vintage | la ritmica anni Settanta: bongo, maracas, tamburello |
| 4 | Linn 80 | batteria acustica campionata degli anni Ottanta |
| 5 | DMX Hip hop | acustica sporca e pesante, con schiocco di dita |
| 6 | Simmons 80 | tom elettronici, cassa FM, zap |
| 7 | Trap | 808 lunga, charleston brillanti, soffio |
| 8 | Techno industriale | tutto saturato: metallo, glitch, zap |
| 9 | House | 909 più morbida, conga, shaker, tamburello |
| 10 | Jungle | acustica accordata in alto, rullante ghost, sub 808 |
| 11 | Lo-fi | acustica smorzata e scura, legnetto, fruscio |
| 12 | Chiptune NES | 8 bit: rumore, blip, glitch |
| 13 | Game Boy | 8 bit più acuta: bip, laser, bit |
| 14 | Jazz spazzole | spazzole, cross stick, ride e campana del ride, splash |
| 15 | Rock acustica | cassa e rullante pieni, tre tom, crash e ride |
| 16 | Latina | timbale, tumba, conga, bongo, clave, guiro, cascara |
| 17 | Afro | djembe, campana, shekere, tamburo parlante |
| 18 | Bossa e samba | surdo, caixa, tamborim, agogo, pandeiro, cuica |
| 19 | Dembow | 808 e timbal per il reggaeton |
| 20 | Elettro | 808 elettro con tom elettronici e zap |
| 21 | IDM glitch | cassa FM, glitch, bit, zap, legnetti acuti |
| 22 | Minimal | pochi suoni corti e precisi |
| 23 | Dub | cassa morbida, rullante alto, tom profondi, sirena |
| 24 | Gabber | cassa gabber distorta, tutto saturato |
| 25 | Ambient | suoni lunghi e morbidi: tabla, triangolo, soffio |

## 14. I 33 ritmi pronti

Ogni ritmo carica il suo kit, il suo tempo e il suo swing.

| N. | Ritmo | BPM | Kit | Swing |
|---|---|---|---|---|
| 1 | Cassa dritta house | 124 | House | 10% |
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
| 28 | Poliritmo 3 su 4 | 120 | Simmons 80 | — |
| 29 | Half time | 140 | Linn 80 | — |
| 30 | Footwork | 160 | 808 Classica | — |
| 31 | Latina: son e tumbao | 100 | Latina | — |
| 32 | Acid house | 125 | 606 Tic | 8% |
| 33 | Garage 2-step | 132 | 909 Club | 20% |

«Amen jungle» è lungo 32 passi (due pagine); «Shuffle» e «Jazz swing» sono scritti in terzine; «Trap» e «IDM» hanno i charleston a velocità doppia; «Poliritmo 3 su 4», «Minimal», «IDM» e «Ambient» hanno tracce di lunghezze diverse.

## 15. Tutti i tasti, pagina per pagina

**In tutte le pagine:** SELECT pagina successiva · START suona/ferma · R3 registra · L3 spegne le note (in Live: modo) · MENU esce · levetta sinistra filtro DJ · levetta destra su stutter, giù fill.

| Tasto | Ritmo | Scheda del passo (X) | Suoni | Mixer | Live | Pattern |
|---|---|---|---|---|---|---|
| Croce | cursore | su/giù riga, sin./des. valore | su/giù parametro, sin./des. valore | sin./des. striscia, su/giù valore | pad 1–4 | sceglie il pattern; destra → ritmi pronti |
| A | passo sì/no | esegue (A) | ascolta | muto | pad 7 | modifica il pattern (spegne la catena) |
| B | tenuto + croce: valore rapido | chiude | tenuto: passi grandi | tenuto: passi grandi | pad 6 | due volte: svuota |
| X | apre la scheda | chiude | — | ascolta | pad 8 | aggiunge alla catena |
| Y | ascolta | ascolta | variazione | solo | pad 5 | copia / incolla |
| L1 / R1 | pagina di passi | valore a passi grandi | traccia | riga o parametro del master | pad 10 / 11 | L1 toglie dalla catena · R1 esporta in WAV |
| L2 / R2 | pattern da modificare | — | kit | — | pad 9 / 12 | — |
| L3 | spegne le note | spegne le note | spegne le note | spegne le note | cambia modo | spegne le note |

Nell'elenco dei ritmi pronti: su/giù sceglie, L1/R1 salta di 10, A carica, sinistra torna ai pattern.

## 16. File

- `Saves/cirmolo-synth/stato.txt`: suoni, kit, mixer, master, i 16 pattern e la catena (si riscrive da solo).
- `Saves/cirmolo-synth/registrazioni/`: le registrazioni (`registrazione-...wav`) e le esportazioni (`pattern-...wav`, `catena-...wav`).
- `Saves/spruce/cirmolo-synth.log`: il registro dell'app.
- I sorgenti sono in `App/CirmoloSynth/src`. Le prove sul PC partono con `src/build.sh prova`: prove del motore, prove dell'interfaccia con le schermate di tutte le pagine, e la versione per Windows.

Sulla scheda SD i percorsi completi cominciano da `/mnt/SDCARD/`, per esempio `/mnt/SDCARD/Saves/cirmolo-synth/`.

## 17. Limiti attuali

- Un solo kit, un solo tempo e un solo swing per tutti i pattern.
- La Flip non segue un clock MIDI esterno: è sempre lei a dare il tempo.
- Dal vivo si registrano solo le batterie (tasti in Live e pad MIDI), non le note della tastiera.
- Non c'è un annulla.
- Lo schermo è solo in italiano.
