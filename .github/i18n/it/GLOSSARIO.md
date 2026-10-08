# Italiano di Cirmolo: guida di stile e glossario

Regole per tradurre l'interfaccia (PyUI, menu delle impostazioni, app e messaggi).
Il controllo automatico è in `.github/i18n/check_lang.py`.

## Stile
- **Dai del tu:** «Vuoi spegnere la console?», «Premi A per iniziare».
  - Le azioni vanno all'imperativo: Salva, Elimina, Riprova.
  - Le voci delle impostazioni sono nomi: «Luminosità LED RGB».
- **Maiuscola solo all'inizio:** «Impostazioni schermo», non «Impostazioni Schermo». Fanno eccezione i nomi propri (RetroArch, PPSSPP, Game Switcher, Syncthing…) e le sigle.
- **Accenti e apostrofi:**
  - **È** e non E'; si scrive perché, né, po', qual è, un altro.
  - Usa l'apostrofo dritto (') e i tre punti (...): ci sono in tutti i caratteri.
- **Avanzamento e errori:**
  - Le operazioni in corso si scrivono con «… in corso…»: «Ricerca reti in corso...».
  - Un errore dice cosa è successo e poi cosa fare: «Impossibile scaricare… Riprova più tardi.».
- **Numeri e unità:** con il segnaposto evita gli accordi singolare/plurale («Trovate finora: {found}»). Le unità vanno staccate dal numero: «{minutes} min», «50 MB».
- **Segnaposto intatti:** `{name}`, `{count}`, `$system`, `{ip_addr}` restano identici.
  - Le descrizioni delle impostazioni e il formato data passano da `str.format`: niente altre parentesi graffe.
- **Testo solo a schermo:** i valori salvati nelle impostazioni non si traducono, si traduce solo come vengono mostrati.

## Lunghezze a 640×480

| Elemento | Massimo consigliato |
|---|---|
| Riquadri del menu principale | 12 caratteri |
| Titolo nella barra superiore | 24 |
| Voci dei menu a comparsa (non scorrono) | 26 |
| Nome di un'impostazione | 30 |
| Valore di un'impostazione | 15 |
| Descrizione su una riga (oltre 70 scorre) | 60 |

## Glossario

| Inglese | Italiano | Note |
|---|---|---|
| Save State / Load State | Salva stato / Carica stato | il nome è «salvataggio di stato» |
| save (in-game) | salvataggio | |
| core, ROM | core, ROM | invariabili (il core, la ROM) |
| boxart, scrape | copertina, scarica copertine | |
| Collections / Favorites / Recents | Collezioni / Preferiti / Recenti | |
| Apps / Settings | App / Impostazioni | |
| Tasks | Strumenti | per non confonderlo con «Monitoraggio attività» |
| button | tasto | sempre «tasto», non «pulsante» |
| D-Pad, analog stick | croce direzionale («Croce su»), levetta | |
| hotkey, shortcut | scorciatoia | |
| fast forward | avanti veloce | |
| sleep, idle, lid, rumble | sospensione, inattività, coperchio, vibrazione | |
| Wi-Fi | Wi-Fi | non «WiFi» o «wifi» |
| achievements, cheevos | obiettivi | |
| True / False | Sì / No | |
| On / Off | Attivo / Disattivo | |
| Confirm / Back | OK / Indietro | |
| screensaver | salvaschermo | |
| Game Switcher | Game Switcher | nome della funzione, resta in inglese |
| Nightly / Stable | Nightly / Stabile | canali di aggiornamento |

**Restano in inglese:** RetroArch, PPSSPP, DraStic, DSperate, PortMaster, Moonlight, Syncthing, Samba, SSH, PICO-8, ScummVM, RetroAchievements (Casual, Hardcore), i nomi degli emulatori e dei core.
