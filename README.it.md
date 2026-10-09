<p align="center">
  <img src="App/BootLogo/Imgs/Cirmolo.png" alt="Logo di Cirmolo: la pigna viola del pino cembro tra due ciuffi di aghi" width="420">
</p>

# Cirmolo

*[English](README.md) · Italiano*

**Cirmolo** è un fork di [spruceOS](https://github.com/spruceUI/spruceOS) pensato per la **Miyoo Flip**, con l'interfaccia completamente in italiano.
Il nome viene dal cirmolo, il pino cembro delle Dolomiti: un legno profumato e resistente, parente dell'abete rosso (*spruce*).

> **Stato:** in sviluppo, testato solo sulla Miyoo Flip. Lo usi a tuo rischio, come spruceOS.

<p align="center">
  <img src=".github/branding/schermate/menu.png" width="32%" alt="Menu principale">
  <img src=".github/branding/schermate/app.png" width="32%" alt="App divise in aree">
  <img src=".github/branding/schermate/musica.png" width="32%" alt="Area Musica">
  <img src=".github/branding/schermate/giochi.png" width="32%" alt="Giochi">
  <img src=".github/branding/schermate/synth.png" width="32%" alt="Cirmolo Synth">
  <img src=".github/branding/schermate/openorc.png" width="32%" alt="OpenOrc">
</p>

## Cosa cambia rispetto a spruceOS
- **Italiano completo:** interfaccia (PyUI), menu delle impostazioni, descrizioni e valori. La guida di stile è in [`.github/i18n/it/GLOSSARIO.md`](.github/i18n/it/GLOSSARIO.md).
- **Strumenti per le traduzioni** in [`.github/i18n/`](.github/i18n/), utili a tutte le lingue: estrazione delle chiavi usate dal codice, allineamento di `English.json`, validatore.
- **Aggiornamenti OTA dal feed di Cirmolo:** un aggiornamento non riporta la versione originale.
- **Segnalazioni di bug:** il report resta sulla SD e si allega alle [issue di Cirmolo](https://github.com/luigismith/cirmolo/issues), invece di andare al server del team spruce.
- **Versione di Cirmolo** in Impostazioni → Informazioni.
- **Logo di avvio di Cirmolo** (la pigna viola del cirmolo) e **cambio del logo più sicuro sulla Flip:** prima di riscrivere la memoria interna, l'app «Logo di avvio» controlla la batteria, copia sulla SD tutta la memoria interna con i checksum, verifica la partizione di avvio e, se la scrittura fallisce, rimette da sola la copia.
- **Tema Cirmolo**, predefinito: i colori del logo (notte, viola della pigna, verde degli aghi) nei menu, nelle icone delle app e delle aree, nelle animazioni di caricamento e di ricarica e nelle schermate di sistema, con la pigna al posto dell'albero di spruce. È ricavato dal tema Spruce di tenlevels ([`.github/branding/tema_cirmolo.py`](.github/branding/tema_cirmolo.py)), che resta tra i temi.
- **App raggruppate in cartelle** (Musica, Giochi, Sistema, Altro...).

### App native nuove
Scritte in C per la Flip, con un kit comune ([`spruce/cirmolo-kit`](spruce/cirmolo-kit)): grafica, tasti, audio e tastiere MIDI USB.
- **Cirmolo Synth** ([`App/CirmoloSynth`](App/CirmoloSynth)): sintetizzatore a 8 voci con filtro risonante, batteria e sequencer a 16 passi, pattern A-D, arpeggiatore, preset e registrazione in WAV. Si suona con i tasti o con una tastiera MIDI USB.
- **Diapason** ([`App/Diapason`](App/Diapason)): accordatore, note di riferimento e metronomo.
- **OpenOrc** ([`App/OpenOrc`](App/OpenOrc)): sintetizzatore di accordi per la tastiera Akai MPK mini IV, ispirato all'Orchid. Ha 4 motori di suono, 6 modi di suonare l'accordo (strum, arpeggio, arpa...), batteria, looper e una pagina MIDI che impara i controlli. Un oscilloscopio mostra l'uscita. Il preset della tastiera e la scheda per configurare la MPK sono in [`App/OpenOrc/tastiere`](App/OpenOrc/tastiere/akai-mpk-mini-iv.it.md). Senza tastiera si suona con i tasti della Flip.
- **Chiedi all'IA** ([`App/ClaudeChat`](App/ClaudeChat)): chat con i modelli di intelligenza artificiale, anche a voce. Claude è il primo e il predefinito; ci sono anche OpenAI, Gemini, DeepSeek, Qwen, GLM, Kimi, MiniMax, Groq, OpenRouter, Mistral, Cerebras e qualunque server compatibile OpenAI (per esempio Ollama sul PC di casa, con `fornitori.json`). Alcuni hanno modelli gratuiti (GLM Flash, Groq, OpenRouter `:free`, Mistral). Tenendo premuto R2 si parla (microfono o cuffie USB, cuffie Bluetooth in prova) e le risposte si possono far leggere ad alta voce (trascrizione con Groq, OpenAI o Gemini; voce con OpenAI o Gemini). Interfaccia in italiano e in altre nove lingue. Servono il Wi-Fi e le proprie chiavi API, salvate solo sulla SD.
- **Funzioni IA per tutta la console** (scheda Console di Chiedi all'IA, con un modello che legge le immagini; GLM-4.6V-Flash è gratuito):
  - **Traduttore dei giochi**: nei giochi di RetroArch, SELECT + giù mette in pausa e mostra la traduzione del testo sullo schermo (per esempio dal giapponese), anche letta ad alta voce. È un piccolo server per il servizio IA di RetroArch che parte e si ferma con il gioco; le traduzioni restano in `Saves/claude/traduzioni`.
  - **Scheda del gioco**: dal menu di un gioco in PyUI (MENU, «Scheda del gioco (IA)»), una scheda con copertina: in breve, di cosa parla, come si gioca, consigli e curiosità, da leggere o ascoltare. Si salva e la volta dopo si legge subito.
  - **Diario delle partite**: a ogni fine partita si annotano gioco, durata e ultima schermata; con l'IA anche dove eri rimasto, che compare in un promemoria quando riapri il gioco e nella scheda.
- **Cosa gioco?** ([`App/CosaGioco`](App/CosaGioco)): tre domande (tempo, voglia, nuovo o da riprendere) e il modello sceglie quattro giochi tra quelli sulla SD, tenendo conto del diario; A avvia il gioco.

**In arrivo:** altre app, miglioramenti dell'interfaccia e delle prestazioni sulla Flip.

## Rami e versioni
- **`cirmolo`** (predefinito): lo sviluppo di Cirmolo.
- **`release/0.2`**: la versione pubblicata, vedi le [release](https://github.com/luigismith/cirmolo/releases); `release/0.1` resta com'era per la 0.1.0.
- **`Development`**: copia identica di spruceOS, da cui si prendono gli aggiornamenti dell'originale. Gli altri rami vengono da spruceOS.

## Crediti e licenza
- Cirmolo esiste grazie al lavoro del team **spruceUI** e dei contributori di spruceOS: il merito della base è tutto loro.
- Come spruceOS, è distribuito con licenza **CC BY-NC 4.0** (uso non commerciale, con attribuzione): vedi [LICENSE](LICENSE). I componenti di terze parti mantengono le loro licenze.
- Non contiene e non conterrà mai giochi o BIOS.
- Problemi e proposte vanno nelle [issue di Cirmolo](https://github.com/luigismith/cirmolo/issues), non al team spruce.

---

<details>
<summary><b>README originale di spruceOS</b> (in inglese): funzioni, dispositivi, crediti del team spruceUI</summary>

# spruceOS 

  - SpruceOS is a community software package intended to help you get the most out of your Miyoo, TrimUI and Anbernic RG XX devices.
  - Our mission is to provide a balanced user experience for both brand new and well-seasoned emulation enthusiasts alike: sane and well-optimized defaults for those who don't want to tweak settings, but immensely deep customization options for those that do.
  - Spruce is intended to be sleek, intuitive, efficient, and user friendly. We hope that you enjoy it.

    _We are not responsible for damage to your device. You must use spruce and its features at your own risk._

## Features

* Snappy and incredibly themeable custom Python-based UI.
* Game Switcher: seamlessly switch between save states during gameplay.
* Autosave on shutdown/autoresume on boot: automatic save state when powering off in-game; powering on will resume play from where you left off.
* Network services: Retroachievments, RTC sync via WiFi, SSH/SFTP, Syncthing, Samba and HTTP file transfer.
* CPU performance profiles pre-configured for optimized battery life and performance.
* Native Pico-8 support with Splore.
* Built-in boxart scraper app using libretro API.
* OTA updates over Wi-Fi on device.
* Game Nursery for downloading free ports, demakes, and homebrew for a variety of systems, directly to your device.
* Theme Garden for browsing and downloading an ever-growing (currently over 80!) selection of community-made themes

## Download the latest version

  - [CLICK HERE FOR THE LATEST RELEASE](https://github.com/spruceUI/spruceOS/releases)

## Need help?
  
  - [CLICK HERE TO SEE THE WIKI](https://github.com/spruceUI/spruceOS/wiki)

## Installation
  - The short version is: format your SD card to FAT32 and extract the 7z (using the 7zip app) file to your PC, then copy the files onto your SD card.
  - For more information, see the new [Wiki installation page](https://github.com/spruceUI/spruceOS/wiki/Installation-Instructions)
  - Place your BIOS files in the `BIOS` folder on the root of SD card.

## All In One Installer
  - Download [The spruceOS Installer program](https://github.com/spruceUI/spruceOS-Installer/releases/latest)
  - Simply insert your SD card into your computer and run the program, be sure to select the correct drive!
  - It formats your card, downloads the latest official spruce release, and installs it in one click!
  - Now V1.1 has a boxart scraper!

## UPDATING TO THE LATEST RELEASE
To update:

See our updating spruce [Wiki page for more info](https://github.com/spruceUI/spruceOS/wiki/01.-Installation-Instructions)

## Controls and Hotkeys


### Global

* Quicksave + Shutdown: Hold POWER for 3 seconds*
* Game Switcher: Hold MENU for 3 seconds
* Brightness down: START + L1 or MENU + VOLDOWN
* Brightness up: START + R1 or MENU + VOLUP

\*Holding POWER after the vibration occurs will cause your device to force shutdown (in case of freezes etc.)

### RetroArch (and PPSSPP)
![hotkeyDefaults](https://github.com/user-attachments/assets/7558ecd9-8149-4009-936a-2cd32c9c7ec9)

* MOD: SELECT or MENU, depending on the device (can be changed either in RA or in spruce settings)
* Screenshot: MOD + A
* Exit to spruceUI: MOD + B
* Open menu: HOME/MENU (label differs by device)
* Open menu: MOD + X
* Toggle FPS display: MOD + Y
* Load state: MOD + L1
* Save state: MOD + R1
* Toggle slow-motion: MOD + L2
* Toggle fast-forward: MOD + R2
* Toggle current shader: MOD + D-Pad UP
* Cycle state slots: MOD + D-Pad LEFT/D-Pad RIGHT

### DSperate

* MOD = MENU (SELECT on devices with no MENU key)
* screenshot = MOD + A
* Quit = MOD + B
* Open menu = MOD + X
* Toggle FPS = MOD + Y
* Cycle state slots = MOD + left/right
* Save state = MOD + R1
* Load state = MOD + L1
* Cycle screen layouts = MOD + up/down
* change primary screen = MOD + L2
* FF hold = R2
* FF toggle = MOD + R2
* Tap stylus = L2 or L3

Zero-stick devices:
* Use D-pad as stylus = hold R2
* no FF hold button

One-stick devices:
* stylus control is on left stick

Two-stick devices:
* stylus control is on right stick
* left stick mirrors d-pad


## Themes

  - The classic spruce theme is minimalistic, but we include around a dozen default themes for you to try.
  - For additional themes, you can either download additional themes from [here](https://github.com/spruceUI/PyUI-Themes) and place the unextracted .7z archives into the Themes folder on your SD card, or you can use our Theme Garden app to browse the repository and install them over the air, directly onto your spruce device!
  - We are always seeking new themes, and would love to feature your artwork! If you are interested in contributing a theme, please reach out! There has also been some preliminary work on a [Theme Guide](https://github.com/spruceUI/spruceOS/wiki/Theme-design-guide) to help get you started.

## Active team members:

spruceOS is a volunteer community effort, with a very fluid team structure. It would be impossible to list everyone who has contributed to the project, code or otherwise. Here are some contributors from recent development cycles, in alphabetical order:

   - [Arkun](https://github.com/CatalyticArkun)
   - [Chrisj951](https://github.com/chrisj951) - Discord @chrisbastion
   - [Hario](https://github.com/Hairo)
   - [KitFox](https://github.com/orgs/spruceUI/people/KitFox0618)
   - [KMFDManic](https://github.com/KMFDManic)
   - [Lonko](https://github.com/orgs/spruceUI/people/LonkoDeLonk)
   - [Mike Kendall](https://github.com/zenkalia)
   - [Noxwell](https://github.com/beebono)
   - [RDWilliamson](https://github.com/orgs/spruceUI/people/robertdw1974-byte)
   - [Ryan "Ry" Sartor](https://github.com/ryanmsartor)
   - [SundownerSport](https://github.com/Sundownersport)
   

## Special Thanks
  - Tenlevels: Starting spruce, making kickass themes and getting the A30 where it deserves to be! Spruce would never have existed without him, we are eternally grateful to the long hours and dedication he put in. Thanks buddy!
  - Shauninman: Constant help, tech support, and inspiration (plus all the code we stole from him :D).
  - MustardOS Team: we borrowed kind of a lot from you guys... thank you!
  - Christian Haitian: updated graphics driver for Miyoo Flip; some libretro cores.
  - Knulli and the rest of the Open Handheld Collective for amazing collaboration and sharing. It takes a village! 
  - XanXic: lots of organizational improvements; designing the OTA and EZ Updaters; and so much more!
  - Onion Team: guidance and inspiration.
  - Steward, trngaje: Custom DraStic versions.
  - XK: Custom SDL2 versions for the Miyoo Mini family of devices
  - KMFDManic: Building and testing new cores (N64 F^%$ Yeah!).
  - Hoo: Testing and encouragement.
  - Metallic77: Custom lightweight shaders for the A30, and Flycast tweaks.
  - Russ from RGC: His YouTube channel is an inspiration.
  - [Icons8.com](icons8.com) for the logo, icons and their genrosity in giving us expanded access to icons for this project.
  - [Miyoo](https://lomiyoo.com/), [TrimUI](https://trimui.net/), and [Anbernic](https://anbernic.com/) for sending us development units.
  - All past and present Team Members!
  - Our wonderful nightly testers, who have provided tons of helpful feedback, bug reports, and comeradery!


THANK YOU TO THE AMAZING RETRO HANDHELD COMMUNITY!!


## AI Disclosure

Some of our contributors use AI to help code, as is the industry standard. The spruceUI organizations’s policy on AI use is that we will always judge potential contributions based on the code’s quality rather than who wrote it, or how.


## SUPPORTED GAME SYSTEMS

[Click here for a table of supported systems and file extensions.](https://github.com/spruceUI/spruceOS/wiki/11.-Adding-Games#rom-folder-chart)

## Interested in being a tester, or just hanging out? To provide feedback and speak with the development team please join our Discord server by clicking on the image below or using [this link](https://discord.gg/KjR5uMQQt9)

[Discord di spruce](https://discord.gg/KjR5uMQQt9)

</details>

