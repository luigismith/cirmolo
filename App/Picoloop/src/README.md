# Picoloop per la Miyoo Flip: come è stato compilato

Picoloop è il synth e sequencer a passi di yoyz (https://github.com/yoyz/picoloop), licenza GPL
(con i motori Cursynth e Twytch in GPLv3; le licenze dei motori sono in `../licenze/`).

Il binario `../picoloop` è compilato dal commit `5c54d47` del repository originale, con le modifiche
di `patch-flip.diff` (tasti della Flip, finestra 640×480 a schermo intero, audio via SDL, correzione per
clang in Open303) e lo script `build.sh`, che usa Zig come compilatore cross:

```sh
git clone https://github.com/yoyz/picoloop && cd picoloop && git checkout 5c54d47
git apply /percorso/di/patch-flip.diff
ZIG=/percorso/zig SRC=$(pwd)/picoloop DLL=/percorso/App/PyUI/dll sh build.sh
```

`DLL` è la cartella con `libSDL2-2.0.so` e `libSDL2_ttf-2.0.so` di PyUI, usate per il link: sulla console
il binario carica quelle stesse librerie (`LD_LIBRARY_PATH` in `../launch.sh`). `elfdeps.py` controlla
che l'eseguibile dipenda solo da SDL2, SDL2_ttf e glibc ≤ 2.27.
