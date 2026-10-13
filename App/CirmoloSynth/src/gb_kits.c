/* Cirmolo Synth - groovebox: dati (vedi gb_dsp.h). Nomi dei modelli, 26 kit da 12 strumenti, ritmi pronti.
 *
 * Ordine delle tracce nei kit (quando il kit lo permette): 0 cassa, 1 rullante, 2 rim, 3 battimani,
 * 4 charleston chiuso, 5 charleston aperto (choke 1 col chiuso), 6 tom basso, 7 tom alto, 8 piatto o ride,
 * 9-11 percussioni. I ritmi sono scritti per il loro kit consigliato.
 */
#include "gb_dsp.h"

#include <string.h>

static const char *const MODEL_NAMES[DM_COUNT] = {
    [DM_KICK_808] = "Cassa 808", [DM_KICK_909] = "Cassa 909", [DM_KICK_ACOUSTIC] = "Cassa acustica",
    [DM_KICK_FM] = "Cassa FM", [DM_KICK_HARD] = "Cassa gabber",
    [DM_SNARE_808] = "Rullante 808", [DM_SNARE_909] = "Rullante 909", [DM_SNARE_ACOUSTIC] = "Rullante acustico",
    [DM_SNARE_BRUSH] = "Spazzola",
    [DM_RIM] = "Rim", [DM_CLAP] = "Battimani", [DM_SNAP] = "Schiocco di dita",
    [DM_HAT_CLOSED] = "Charleston chiuso", [DM_HAT_OPEN] = "Charleston aperto", [DM_HAT_909] = "Charleston FM",
    [DM_CYMBAL] = "Piatto crash", [DM_RIDE] = "Piatto ride",
    [DM_TOM] = "Tom", [DM_TOM_ELECTRO] = "Tom elettronico",
    [DM_CONGA] = "Conga", [DM_BONGO] = "Bongo", [DM_TABLA] = "Tabla",
    [DM_COWBELL] = "Campanaccio", [DM_CLAVE] = "Claves", [DM_WOODBLOCK] = "Legnetto", [DM_SHAKER] = "Shaker",
    [DM_TAMBOURINE] = "Tamburello", [DM_TRIANGLE] = "Triangolo",
    [DM_CHIP_KICK] = "Cassa 8 bit", [DM_CHIP_SNARE] = "Rullante 8 bit", [DM_CHIP_HAT] = "Charleston 8 bit",
    [DM_ZAP] = "Zap", [DM_GLITCH] = "Glitch", [DM_NOISE_SWEEP] = "Soffio",
};

const char *gb_model_name(int model) { return (model >= 0 && model < DM_COUNT) ? MODEL_NAMES[model] : "?"; }

/* ------------------------------------------------------------------ kit */

typedef struct { const char *name; GbDrum d; } KitInst;
typedef struct { const char *name; float vol; KitInst t[GB_DRUMS]; } KitDef;   /* vol: volume del kit (pareggia i kit fra loro) */

/* nome, modello, tune, decay, tone, snap, drive, level, pan, choke */
#define I(nm, m, tu, de, to, sn, dr, lv, pn, ch) { nm, { m, tu, de, to, sn, dr, lv, pn, ch } }

enum {
    K_808, K_909, K_606, K_CR78, K_LINN, K_DMX, K_SIMMONS, K_TRAP, K_INDUS, K_HOUSE, K_JUNGLE, K_LOFI, K_NES,
    K_GB, K_JAZZ, K_ROCK, K_LATIN, K_AFRO, K_BOSSA, K_DEMBOW, K_ELECTRO, K_IDM, K_MINIMAL, K_DUB, K_GABBER,
    K_AMBIENT, K_COUNT
};

static const KitDef KITS[K_COUNT] = {
    [K_808] = { "808 Classica", 1.00f, {
        I("Cassa", DM_KICK_808, 0, .55f, .25f, .35f, 0, .85f, 0, 0),
        I("Rullante", DM_SNARE_808, 0, .45f, .5f, .6f, 0, .75f, 0, 0),
        I("Rim", DM_RIM, 0, .3f, .5f, .5f, 0, .6f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .45f, .45f, .5f, 0, .7f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 0, .3f, .55f, .4f, 0, .6f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .45f, .55f, .3f, 0, .55f, .2f, 1),
        I("Tom basso", DM_TOM, -5, .5f, .2f, .45f, 0, .75f, -.3f, 0),
        I("Tom alto", DM_TOM, 4, .45f, .2f, .45f, 0, .7f, .3f, 0),
        I("Piatto", DM_CYMBAL, 0, .6f, .5f, .4f, 0, .5f, -.2f, 0),
        I("Campanaccio", DM_COWBELL, 0, .4f, .5f, .5f, 0, .55f, .25f, 0),
        I("Claves", DM_CLAVE, 0, .3f, .3f, .5f, 0, .55f, -.35f, 0),
        I("Conga", DM_CONGA, 0, .4f, .4f, .4f, 0, .65f, .35f, 0) } },
    [K_909] = { "909 Club", 1.05f, {
        I("Cassa", DM_KICK_909, 0, .5f, .5f, .5f, 0, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 0, .45f, .55f, .55f, 0, .75f, 0, 0),
        I("Rim", DM_RIM, 2, .25f, .6f, .6f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .5f, .5f, .55f, 0, .7f, .1f, 0),
        I("Charleston chiuso", DM_HAT_909, 0, .12f, .5f, .5f, 0, .6f, .2f, 1),
        I("Charleston aperto", DM_HAT_909, 0, .55f, .5f, .3f, 0, .55f, .2f, 1),
        I("Tom basso", DM_TOM, -4, .45f, .55f, .6f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_TOM, 5, .4f, .55f, .6f, 0, .65f, .3f, 0),
        I("Ride", DM_RIDE, 0, .55f, .5f, .3f, 0, .5f, -.25f, 0),
        I("Crash", DM_CYMBAL, 0, .65f, .6f, .5f, 0, .5f, .25f, 0),
        I("Shaker", DM_SHAKER, 0, .3f, .6f, .5f, 0, .5f, .3f, 0),
        I("Tamburello", DM_TAMBOURINE, 0, .4f, .5f, .2f, 0, .5f, -.3f, 0) } },
    [K_606] = { "606 Tic", 1.10f, {
        I("Cassa", DM_KICK_808, 3, .3f, .4f, .5f, 0, .85f, 0, 0),
        I("Rullante", DM_SNARE_808, 3, .3f, .75f, .75f, 0, .7f, 0, 0),
        I("Rim", DM_RIM, 3, .2f, .7f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 2, .3f, .6f, .7f, 0, .6f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 2, .2f, .8f, .6f, 0, .6f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 2, .35f, .8f, .4f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -2, .35f, .1f, .3f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_TOM, 6, .3f, .1f, .3f, 0, .65f, .3f, 0),
        I("Piatto", DM_CYMBAL, 2, .5f, .7f, .3f, 0, .45f, -.2f, 0),
        I("Campanaccio", DM_COWBELL, 2, .3f, .6f, .5f, 0, .5f, .25f, 0),
        I("Claves", DM_CLAVE, 0, .25f, .2f, .4f, 0, .5f, -.35f, 0),
        I("Shaker", DM_SHAKER, 2, .25f, .7f, .7f, 0, .45f, .35f, 0) } },
    [K_CR78] = { "CR-78 Vintage", 1.25f, {
        I("Cassa", DM_KICK_808, 4, .25f, .1f, .2f, 0, .85f, 0, 0),
        I("Rullante", DM_SNARE_808, 2, .25f, .3f, .45f, 0, .7f, 0, 0),
        I("Rim", DM_RIM, 0, .25f, .3f, .4f, 0, .55f, -.15f, 0),
        I("Claves", DM_CLAVE, -2, .3f, .3f, .3f, 0, .55f, .15f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, -3, .25f, .2f, .2f, 0, .6f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -3, .35f, .2f, .2f, 0, .5f, .2f, 1),
        I("Bongo basso", DM_BONGO, -4, .4f, .3f, .3f, 0, .65f, -.3f, 0),
        I("Bongo alto", DM_BONGO, 3, .35f, .3f, .3f, 0, .6f, .3f, 0),
        I("Piatto", DM_CYMBAL, -2, .4f, .3f, .2f, 0, .45f, -.2f, 0),
        I("Campanaccio", DM_COWBELL, 4, .25f, .4f, .5f, 0, .5f, .25f, 0),
        I("Maracas", DM_SHAKER, 0, .2f, .5f, .6f, 0, .5f, -.35f, 0),
        I("Tamburello", DM_TAMBOURINE, 0, .3f, .4f, .1f, 0, .5f, .35f, 0) } },
    [K_LINN] = { "Linn 80", 1.00f, {
        I("Cassa", DM_KICK_ACOUSTIC, 0, .4f, .5f, .5f, .1f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 2, .45f, .6f, .6f, .05f, .75f, 0, 0),
        I("Cross stick", DM_RIM, -3, .3f, .3f, .4f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, -1, .45f, .4f, .4f, 0, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 0, .25f, .7f, .5f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .45f, .7f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -6, .5f, .6f, .5f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_TOM, 3, .45f, .6f, .5f, 0, .65f, .3f, 0),
        I("Ride", DM_RIDE, 0, .6f, .6f, .4f, 0, .45f, -.25f, 0),
        I("Campanaccio", DM_COWBELL, -2, .35f, .5f, .5f, 0, .5f, .25f, 0),
        I("Conga", DM_CONGA, 0, .45f, .5f, .4f, 0, .6f, -.35f, 0),
        I("Tamburello", DM_TAMBOURINE, 0, .4f, .6f, .2f, 0, .5f, .35f, 0) } },
    [K_DMX] = { "DMX Hip hop", 0.85f, {
        I("Cassa", DM_KICK_ACOUSTIC, -3, .55f, .3f, .4f, .35f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 0, .5f, .5f, .7f, .3f, .75f, 0, 0),
        I("Rim", DM_RIM, -2, .3f, .4f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, -2, .55f, .35f, .5f, .1f, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, -1, .2f, .45f, .6f, .1f, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -1, .4f, .45f, .4f, .1f, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -7, .5f, .4f, .5f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_TOM, 1, .45f, .4f, .5f, 0, .65f, .3f, 0),
        I("Piatto", DM_CYMBAL, -1, .55f, .45f, .4f, 0, .45f, -.2f, 0),
        I("Schiocco", DM_SNAP, 0, .4f, .5f, .5f, 0, .55f, .25f, 0),
        I("Shaker", DM_SHAKER, -1, .3f, .4f, .5f, 0, .45f, -.35f, 0),
        I("Tamburello", DM_TAMBOURINE, -1, .35f, .4f, .2f, 0, .45f, .35f, 0) } },
    [K_SIMMONS] = { "Simmons 80", 0.90f, {
        I("Cassa", DM_KICK_FM, 0, .4f, .3f, .6f, .1f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 2, .55f, .8f, .8f, .15f, .75f, 0, 0),
        I("Rim", DM_RIM, 0, .3f, .6f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .6f, .6f, .5f, 0, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_909, 0, .1f, .4f, .5f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_909, 0, .5f, .4f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM_ELECTRO, -5, .55f, .2f, .7f, 0, .7f, -.35f, 0),
        I("Tom alto", DM_TOM_ELECTRO, 4, .5f, .2f, .7f, 0, .65f, .35f, 0),
        I("Piatto", DM_CYMBAL, 0, .6f, .6f, .4f, 0, .45f, -.2f, 0),
        I("Tom medio", DM_TOM_ELECTRO, 0, .5f, .2f, .7f, 0, .65f, 0, 0),
        I("Zap", DM_ZAP, 0, .3f, .2f, .5f, 0, .5f, .25f, 0),
        I("Campanaccio", DM_COWBELL, 0, .35f, .5f, .5f, 0, .5f, -.25f, 0) } },
    [K_TRAP] = { "Trap", 0.75f, {
        I("Cassa 808", DM_KICK_808, -3, .85f, .35f, .25f, .2f, .9f, 0, 0),
        I("Rullante", DM_SNARE_909, 3, .45f, .7f, .75f, .1f, .75f, 0, 0),
        I("Rim", DM_RIM, 4, .2f, .7f, .6f, 0, .5f, -.15f, 0),
        I("Battimani", DM_CLAP, 1, .5f, .55f, .7f, .1f, .7f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 3, .15f, .9f, .6f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 3, .4f, .9f, .4f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -3, .5f, .3f, .5f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 5, .45f, .3f, .5f, 0, .6f, .3f, 0),
        I("Piatto", DM_CYMBAL, 0, .55f, .6f, .5f, 0, .45f, -.2f, 0),
        I("Schiocco", DM_SNAP, 0, .4f, .6f, .6f, 0, .55f, .25f, 0),
        I("Cassa 909", DM_KICK_909, -2, .3f, .5f, .5f, 0, .75f, 0, 0),
        I("Soffio", DM_NOISE_SWEEP, 0, .55f, .5f, .8f, 0, .45f, -.2f, 0) } },
    [K_INDUS] = { "Techno industriale", 0.80f, {
        I("Cassa", DM_KICK_909, -2, .55f, .5f, .6f, .45f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 0, .45f, .4f, .7f, .5f, .7f, 0, 0),
        I("Rim", DM_RIM, -2, .3f, .3f, .6f, .4f, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, -3, .65f, .35f, .6f, .3f, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_909, -2, .1f, .6f, .6f, .2f, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_909, -2, .45f, .6f, .4f, .2f, .5f, .2f, 1),
        I("Tom", DM_TOM_ELECTRO, -8, .35f, .3f, .4f, .4f, .65f, -.3f, 0),
        I("Glitch", DM_GLITCH, 0, .3f, .3f, .5f, .2f, .5f, .3f, 0),
        I("Ride", DM_RIDE, -3, .5f, .4f, .2f, .1f, .45f, -.25f, 0),
        I("Metallo", DM_COWBELL, -5, .3f, .7f, .5f, .6f, .5f, .25f, 0),
        I("Soffio", DM_NOISE_SWEEP, 0, .5f, .6f, .2f, .2f, .45f, -.3f, 0),
        I("Zap", DM_ZAP, -5, .25f, .5f, .7f, .3f, .45f, .35f, 0) } },
    [K_HOUSE] = { "House", 1.00f, {
        I("Cassa", DM_KICK_909, 0, .45f, .4f, .45f, .05f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 0, .4f, .6f, .5f, 0, .7f, 0, 0),
        I("Rim", DM_RIM, 0, .25f, .6f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .5f, .5f, .5f, 0, .7f, .1f, 0),
        I("Charleston chiuso", DM_HAT_909, 1, .12f, .6f, .4f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .45f, .7f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -4, .45f, .5f, .5f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 4, .4f, .5f, .5f, 0, .6f, .3f, 0),
        I("Ride", DM_RIDE, 1, .55f, .6f, .4f, 0, .45f, -.25f, 0),
        I("Conga", DM_CONGA, 0, .45f, .5f, .5f, 0, .6f, .3f, 0),
        I("Shaker", DM_SHAKER, 0, .3f, .6f, .5f, 0, .45f, -.35f, 0),
        I("Tamburello", DM_TAMBOURINE, 0, .4f, .6f, .2f, 0, .45f, .35f, 0) } },
    [K_JUNGLE] = { "Jungle", 0.95f, {
        I("Cassa", DM_KICK_ACOUSTIC, 3, .35f, .6f, .6f, .15f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 5, .4f, .7f, .8f, .15f, .75f, 0, 0),
        I("Rim", DM_RIM, 2, .25f, .5f, .5f, 0, .5f, -.15f, 0),
        I("Rullante ghost", DM_SNARE_ACOUSTIC, 5, .3f, .6f, .4f, 0, .6f, .05f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 2, .2f, .75f, .5f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 2, .4f, .75f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -3, .45f, .5f, .5f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 5, .4f, .5f, .5f, 0, .6f, .3f, 0),
        I("Ride", DM_RIDE, 2, .5f, .7f, .3f, 0, .45f, -.25f, 0),
        I("Crash", DM_CYMBAL, 0, .65f, .6f, .5f, 0, .45f, .25f, 0),
        I("Shaker", DM_SHAKER, 2, .25f, .6f, .5f, 0, .45f, -.35f, 0),
        I("Sub 808", DM_KICK_808, -5, .8f, .1f, .2f, 0, .8f, 0, 0) } },
    [K_LOFI] = { "Lo-fi", 1.00f, {
        I("Cassa", DM_KICK_ACOUSTIC, -2, .45f, .2f, .3f, .2f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, -1, .45f, .3f, .3f, .2f, .75f, 0, 0),
        I("Rim", DM_RIM, -4, .25f, .2f, .3f, .1f, .55f, -.15f, 0),
        I("Schiocco", DM_SNAP, -2, .35f, .3f, .4f, 0, .55f, .15f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, -2, .2f, .3f, .3f, .1f, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -2, .4f, .3f, .2f, .1f, .45f, .2f, 1),
        I("Tom basso", DM_TOM, -5, .45f, .2f, .4f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 2, .4f, .2f, .4f, 0, .6f, .3f, 0),
        I("Ride", DM_RIDE, -3, .45f, .3f, .2f, 0, .4f, -.25f, 0),
        I("Shaker", DM_SHAKER, -3, .3f, .2f, .3f, 0, .45f, .3f, 0),
        I("Legnetto", DM_WOODBLOCK, -3, .3f, .3f, .3f, 0, .5f, -.35f, 0),
        I("Fruscio", DM_NOISE_SWEEP, -5, .4f, .1f, .5f, 0, .35f, .1f, 0) } },
    [K_NES] = { "Chiptune NES", 1.40f, {
        I("Cassa", DM_CHIP_KICK, 0, .4f, .3f, .5f, 0, .85f, 0, 0),
        I("Rullante", DM_CHIP_SNARE, 0, .4f, .5f, .6f, 0, .7f, 0, 0),
        I("Tic", DM_CHIP_HAT, 0, .1f, .6f, .7f, 0, .5f, -.2f, 0),
        I("Rumore", DM_CHIP_SNARE, 5, .3f, .8f, .2f, 0, .6f, .15f, 0),
        I("Charleston chiuso", DM_CHIP_HAT, 0, .15f, .7f, .2f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_CHIP_HAT, 0, .45f, .7f, .2f, 0, .5f, .2f, 1),
        I("Tom basso", DM_CHIP_KICK, 7, .35f, .2f, .3f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_CHIP_KICK, 14, .3f, .2f, .3f, 0, .65f, .3f, 0),
        I("Piatto", DM_CHIP_HAT, -5, .7f, .4f, .3f, 0, .45f, -.2f, 0),
        I("Blip", DM_ZAP, 12, .15f, .8f, .9f, 0, .5f, .25f, 0),
        I("Rumore metallico", DM_CHIP_HAT, 0, .3f, .5f, .8f, 0, .5f, -.35f, 0),
        I("Glitch", DM_GLITCH, 0, .25f, .4f, .3f, 0, .5f, .35f, 0) } },
    [K_GB] = { "Game Boy", 1.40f, {
        I("Cassa", DM_CHIP_KICK, -3, .35f, .6f, .7f, 0, .85f, 0, 0),
        I("Rullante", DM_CHIP_SNARE, 3, .35f, .4f, .3f, 0, .7f, 0, 0),
        I("Tic", DM_CHIP_HAT, 5, .08f, .9f, .8f, 0, .5f, -.2f, 0),
        I("Battito", DM_CHIP_SNARE, 8, .2f, .9f, 0, 0, .6f, .15f, 0),
        I("Charleston chiuso", DM_CHIP_HAT, 3, .12f, .9f, .8f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_CHIP_HAT, 3, .4f, .9f, .8f, 0, .5f, .2f, 1),
        I("Tom basso", DM_CHIP_KICK, 5, .3f, .8f, .4f, 0, .7f, -.3f, 0),
        I("Tom alto", DM_CHIP_KICK, 12, .25f, .8f, .4f, 0, .65f, .3f, 0),
        I("Piatto", DM_CHIP_HAT, -3, .65f, .6f, .6f, 0, .45f, -.2f, 0),
        I("Bip", DM_ZAP, 19, .1f, 1, 1, 0, .45f, .25f, 0),
        I("Laser", DM_ZAP, 0, .3f, .5f, .3f, 0, .5f, -.35f, 0),
        I("Bit", DM_GLITCH, 5, .2f, .2f, .6f, 0, .5f, .35f, 0) } },
    [K_JAZZ] = { "Jazz spazzole", 1.25f, {
        I("Cassa", DM_KICK_ACOUSTIC, 2, .3f, .15f, .2f, 0, .75f, 0, 0),
        I("Spazzola", DM_SNARE_BRUSH, 0, .5f, .5f, .2f, 0, .7f, 0, 0),
        I("Spazzola colpo", DM_SNARE_BRUSH, 2, .3f, .6f, .9f, 0, .65f, .05f, 0),
        I("Cross stick", DM_RIM, -3, .25f, .3f, .3f, 0, .55f, -.15f, 0),
        I("Charleston pedale", DM_HAT_CLOSED, -1, .25f, .35f, .3f, 0, .5f, .25f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -1, .4f, .35f, .2f, 0, .45f, .25f, 1),
        I("Tom basso", DM_TOM, -5, .55f, .3f, .3f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 3, .5f, .3f, .3f, 0, .6f, .3f, 0),
        I("Ride", DM_RIDE, -2, .75f, .45f, .35f, 0, .55f, -.25f, 0),
        I("Campana del ride", DM_RIDE, -2, .6f, .5f, .9f, 0, .45f, -.25f, 0),
        I("Splash", DM_CYMBAL, 3, .4f, .6f, .3f, 0, .4f, .3f, 0),
        I("Spazzola lunga", DM_SNARE_BRUSH, -2, .8f, .4f, 0, 0, .55f, -.05f, 0) } },
    [K_ROCK] = { "Rock acustica", 1.00f, {
        I("Cassa", DM_KICK_ACOUSTIC, 0, .45f, .55f, .6f, .15f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 0, .55f, .55f, .6f, .1f, .8f, 0, 0),
        I("Rim", DM_RIM, -2, .3f, .4f, .5f, 0, .55f, -.15f, 0),
        I("Rullante ghost", DM_SNARE_ACOUSTIC, 0, .35f, .5f, .3f, 0, .55f, 0, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, -1, .3f, .6f, .5f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -1, .5f, .6f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM, -7, .6f, .55f, .5f, 0, .7f, -.35f, 0),
        I("Tom alto", DM_TOM, 2, .5f, .55f, .5f, 0, .65f, .2f, 0),
        I("Crash", DM_CYMBAL, 0, .7f, .55f, .5f, 0, .5f, -.25f, 0),
        I("Ride", DM_RIDE, 0, .65f, .5f, .4f, 0, .45f, .3f, 0),
        I("Tom medio", DM_TOM, -2, .55f, .55f, .5f, 0, .65f, -.1f, 0),
        I("Tamburello", DM_TAMBOURINE, 0, .4f, .5f, .2f, 0, .45f, .35f, 0) } },
    [K_LATIN] = { "Latina", 1.30f, {
        I("Cassa", DM_KICK_ACOUSTIC, 0, .35f, .3f, .3f, 0, .7f, 0, 0),
        I("Timbale", DM_TOM, 14, .3f, .9f, .9f, 0, .6f, .25f, 0),
        I("Bordo timbale", DM_RIM, 3, .3f, .5f, .5f, 0, .55f, .25f, 0),
        I("Battimani", DM_CLAP, 0, .4f, .5f, .4f, 0, .6f, -.1f, 0),
        I("Maracas", DM_SHAKER, 0, .25f, .5f, .6f, 0, .5f, -.3f, 0),
        I("Guiro", DM_SHAKER, -5, .5f, .3f, .1f, 0, .45f, .3f, 0),
        I("Tumba", DM_CONGA, -5, .5f, .4f, .3f, 0, .7f, -.35f, 0),
        I("Conga", DM_CONGA, 2, .45f, .5f, .5f, 0, .65f, -.15f, 0),
        I("Campana", DM_COWBELL, -3, .35f, .4f, .5f, 0, .55f, .1f, 0),
        I("Clave", DM_CLAVE, 0, .35f, .3f, .5f, 0, .55f, .15f, 0),
        I("Bongo", DM_BONGO, 0, .4f, .5f, .6f, 0, .6f, .35f, 0),
        I("Cascara", DM_WOODBLOCK, 4, .25f, .6f, .6f, 0, .5f, .3f, 0) } },
    [K_AFRO] = { "Afro", 1.05f, {
        I("Cassa", DM_KICK_ACOUSTIC, -2, .45f, .3f, .4f, .1f, .8f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 2, .45f, .5f, .5f, 0, .7f, 0, 0),
        I("Rim", DM_RIM, 0, .3f, .5f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .45f, .4f, .4f, 0, .6f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 0, .25f, .5f, .5f, 0, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .45f, .5f, .3f, 0, .45f, .2f, 1),
        I("Djembe basso", DM_CONGA, -8, .45f, .3f, .2f, 0, .7f, -.3f, 0),
        I("Djembe", DM_CONGA, -1, .4f, .6f, .8f, 0, .65f, .3f, 0),
        I("Ride", DM_RIDE, 0, .55f, .5f, .4f, 0, .45f, -.25f, 0),
        I("Campana", DM_COWBELL, 5, .4f, .6f, .4f, 0, .5f, .25f, 0),
        I("Shekere", DM_SHAKER, -2, .35f, .4f, .4f, 0, .5f, -.35f, 0),
        I("Tamburo parlante", DM_TABLA, -7, .5f, .4f, .8f, 0, .6f, .35f, 0) } },
    [K_BOSSA] = { "Bossa e samba", 1.10f, {
        I("Surdo", DM_KICK_ACOUSTIC, -4, .6f, .2f, .2f, 0, .85f, 0, 0),
        I("Caixa", DM_SNARE_ACOUSTIC, 4, .35f, .7f, .5f, 0, .7f, 0, 0),
        I("Bordo", DM_RIM, -2, .25f, .4f, .4f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .4f, .4f, .4f, 0, .55f, .1f, 0),
        I("Ganza", DM_SHAKER, 1, .25f, .7f, .7f, 0, .5f, .2f, 0),
        I("Charleston aperto", DM_HAT_OPEN, 0, .4f, .5f, .3f, 0, .45f, .2f, 0),
        I("Tom basso", DM_TOM, -6, .5f, .4f, .4f, 0, .65f, -.3f, 0),
        I("Tamborim", DM_BONGO, 7, .2f, .7f, .9f, 0, .6f, .3f, 0),
        I("Ride", DM_RIDE, 0, .55f, .5f, .3f, 0, .45f, -.25f, 0),
        I("Agogo", DM_COWBELL, 6, .3f, .7f, .5f, 0, .5f, .25f, 0),
        I("Pandeiro", DM_TAMBOURINE, -2, .35f, .4f, .7f, 0, .55f, -.35f, 0),
        I("Cuica", DM_TABLA, 5, .35f, .2f, 1, 0, .55f, .35f, 0) } },
    [K_DEMBOW] = { "Dembow", 0.85f, {
        I("Cassa", DM_KICK_808, 0, .45f, .4f, .5f, .1f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 2, .35f, .7f, .7f, 0, .75f, 0, 0),
        I("Rim", DM_RIM, 2, .25f, .6f, .6f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .45f, .5f, .6f, 0, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 0, .2f, .7f, .5f, 0, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .4f, .7f, .3f, 0, .45f, .2f, 1),
        I("Tom basso", DM_TOM, -3, .4f, .4f, .5f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM, 4, .35f, .4f, .5f, 0, .6f, .3f, 0),
        I("Piatto", DM_CYMBAL, 0, .55f, .5f, .5f, 0, .45f, -.2f, 0),
        I("Timbal", DM_TOM, 12, .25f, .9f, .8f, 0, .6f, .25f, 0),
        I("Conga", DM_CONGA, 0, .4f, .5f, .5f, 0, .6f, -.35f, 0),
        I("Shaker", DM_SHAKER, 0, .25f, .6f, .6f, 0, .45f, .35f, 0) } },
    [K_ELECTRO] = { "Elettro", 0.80f, {
        I("Cassa", DM_KICK_808, 1, .5f, .5f, .5f, .15f, .85f, 0, 0),
        I("Rullante", DM_SNARE_808, 0, .4f, .6f, .85f, 0, .75f, 0, 0),
        I("Rim", DM_RIM, 0, .25f, .6f, .5f, 0, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .45f, .55f, .5f, 0, .65f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, 0, .2f, .6f, .5f, 0, .55f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .4f, .6f, .3f, 0, .5f, .2f, 1),
        I("Tom basso", DM_TOM_ELECTRO, -4, .45f, .1f, .5f, 0, .65f, -.3f, 0),
        I("Tom alto", DM_TOM_ELECTRO, 5, .4f, .1f, .5f, 0, .6f, .3f, 0),
        I("Piatto", DM_CYMBAL, 0, .5f, .5f, .4f, 0, .45f, -.2f, 0),
        I("Campanaccio", DM_COWBELL, 0, .35f, .5f, .5f, 0, .5f, .25f, 0),
        I("Zap", DM_ZAP, 0, .3f, .3f, .6f, 0, .5f, -.3f, 0),
        I("Claves", DM_CLAVE, 0, .3f, .2f, .5f, 0, .5f, .35f, 0) } },
    [K_IDM] = { "IDM glitch", 0.95f, {
        I("Cassa", DM_KICK_FM, -2, .35f, .6f, .5f, .2f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 5, .3f, .8f, .6f, .3f, .7f, 0, 0),
        I("Glitch", DM_GLITCH, 0, .25f, .3f, .4f, 0, .55f, -.25f, 0),
        I("Battimani", DM_CLAP, 4, .25f, .7f, .9f, 0, .6f, .15f, 0),
        I("Charleston chiuso", DM_HAT_909, 5, .05f, .8f, .6f, 0, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_909, 5, .35f, .8f, .3f, 0, .45f, .2f, 1),
        I("Tom", DM_TOM_ELECTRO, -2, .35f, .5f, .9f, 0, .6f, -.3f, 0),
        I("Zap", DM_ZAP, 7, .2f, .6f, .8f, 0, .5f, .3f, 0),
        I("Ride", DM_RIDE, 3, .4f, .7f, .5f, 0, .4f, -.2f, 0),
        I("Bit", DM_GLITCH, 12, .15f, .1f, .9f, 0, .5f, .35f, 0),
        I("Soffio", DM_NOISE_SWEEP, 3, .35f, .7f, .9f, 0, .45f, -.35f, 0),
        I("Legnetto", DM_WOODBLOCK, 9, .2f, .8f, .6f, 0, .5f, .25f, 0) } },
    [K_MINIMAL] = { "Minimal", 1.25f, {
        I("Cassa", DM_KICK_909, 0, .35f, .2f, .35f, 0, .85f, 0, 0),
        I("Rim", DM_RIM, 0, .25f, .5f, .5f, 0, .65f, -.1f, 0),
        I("Battimani", DM_CLAP, 2, .3f, .5f, .7f, 0, .6f, .1f, 0),
        I("Schiocco", DM_SNAP, 0, .35f, .5f, .5f, 0, .55f, .2f, 0),
        I("Charleston chiuso", DM_HAT_909, 2, .06f, .7f, .7f, 0, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, 0, .35f, .7f, .3f, 0, .45f, .2f, 1),
        I("Tom", DM_TOM_ELECTRO, -6, .3f, .2f, .3f, 0, .6f, -.3f, 0),
        I("Legnetto", DM_WOODBLOCK, 0, .3f, .5f, .5f, 0, .5f, .3f, 0),
        I("Ride", DM_RIDE, 2, .45f, .7f, .3f, 0, .4f, -.25f, 0),
        I("Claves", DM_CLAVE, 0, .25f, .3f, .5f, 0, .5f, .35f, 0),
        I("Shaker", DM_SHAKER, 2, .2f, .7f, .7f, 0, .45f, -.35f, 0),
        I("Conga", DM_CONGA, 3, .35f, .5f, .4f, 0, .55f, .25f, 0) } },
    [K_DUB] = { "Dub", 1.05f, {
        I("Cassa", DM_KICK_ACOUSTIC, -3, .5f, .2f, .3f, .1f, .85f, 0, 0),
        I("Rullante", DM_SNARE_ACOUSTIC, 4, .45f, .7f, .7f, .1f, .75f, 0, 0),
        I("Cross stick", DM_RIM, -1, .3f, .4f, .4f, 0, .6f, -.15f, 0),
        I("Battimani", DM_CLAP, -2, .5f, .3f, .4f, 0, .55f, .1f, 0),
        I("Charleston chiuso", DM_HAT_CLOSED, -2, .25f, .4f, .4f, 0, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_OPEN, -2, .45f, .4f, .3f, 0, .45f, .2f, 1),
        I("Tom basso", DM_TOM, -8, .6f, .3f, .6f, 0, .7f, -.35f, 0),
        I("Tom alto", DM_TOM, 0, .55f, .3f, .6f, 0, .65f, .3f, 0),
        I("Piatto", DM_CYMBAL, -2, .6f, .4f, .4f, 0, .45f, -.2f, 0),
        I("Shaker", DM_SHAKER, -2, .3f, .4f, .5f, 0, .45f, .3f, 0),
        I("Legnetto", DM_WOODBLOCK, -5, .35f, .4f, .4f, 0, .5f, -.3f, 0),
        I("Sirena", DM_ZAP, 12, .7f, .2f, .1f, 0, .4f, .2f, 0) } },
    [K_GABBER] = { "Gabber", 0.70f, {
        I("Cassa", DM_KICK_HARD, 0, .55f, .5f, .6f, .3f, .85f, 0, 0),
        I("Rullante", DM_SNARE_909, 2, .4f, .6f, .8f, .5f, .7f, 0, 0),
        I("Rim", DM_RIM, 0, .3f, .6f, .6f, .5f, .55f, -.15f, 0),
        I("Battimani", DM_CLAP, 0, .45f, .6f, .7f, .4f, .6f, .1f, 0),
        I("Charleston chiuso", DM_HAT_909, 2, .08f, .6f, .6f, .3f, .5f, .2f, 1),
        I("Charleston aperto", DM_HAT_909, 2, .4f, .6f, .3f, .3f, .45f, .2f, 1),
        I("Tom basso", DM_TOM_ELECTRO, -3, .4f, .3f, .7f, .3f, .6f, -.3f, 0),
        I("Tom alto", DM_TOM_ELECTRO, 5, .35f, .3f, .7f, .3f, .55f, .3f, 0),
        I("Piatto", DM_CYMBAL, 0, .6f, .6f, .5f, .2f, .45f, -.2f, 0),
        I("Zap", DM_ZAP, 0, .3f, .6f, .5f, .4f, .5f, .25f, 0),
        I("Soffio", DM_NOISE_SWEEP, 0, .5f, .7f, .9f, .2f, .45f, -.35f, 0),
        I("Glitch", DM_GLITCH, 0, .3f, .4f, .5f, .3f, .5f, .35f, 0) } },
    [K_AMBIENT] = { "Ambient", 0.95f, {
        I("Cassa", DM_KICK_808, -2, .7f, 0, .1f, 0, .75f, 0, 0),
        I("Spazzola", DM_SNARE_BRUSH, -2, .6f, .3f, .1f, 0, .6f, 0, 0),
        I("Rim", DM_RIM, -5, .4f, .2f, .2f, 0, .5f, -.2f, 0),
        I("Schiocco", DM_SNAP, -3, .45f, .3f, .3f, 0, .5f, .2f, 0),
        I("Shaker", DM_SHAKER, -3, .45f, .3f, .2f, 0, .45f, .25f, 0),
        I("Tamburello", DM_TAMBOURINE, -3, .6f, .3f, .1f, 0, .4f, -.25f, 0),
        I("Tom basso", DM_TOM, -9, .8f, .1f, .2f, 0, .65f, -.3f, 0),
        I("Tabla", DM_TABLA, -3, .6f, .3f, .3f, 0, .55f, .3f, 0),
        I("Piatto", DM_CYMBAL, -3, .95f, .3f, .1f, 0, .4f, -.2f, 0),
        I("Triangolo", DM_TRIANGLE, 0, .8f, .5f, .3f, 0, .4f, .35f, 0),
        I("Legnetto", DM_WOODBLOCK, -6, .5f, .2f, .2f, 0, .45f, -.35f, 0),
        I("Soffio", DM_NOISE_SWEEP, -5, .9f, .3f, .8f, 0, .4f, .1f, 0) } },
};

int gb_kit_count(void) { return K_COUNT; }

const char *gb_kit_name(int kit) { return (kit >= 0 && kit < K_COUNT) ? KITS[kit].name : "?"; }

const char *gb_kit_track_name(int kit, int track)
{
    if (kit < 0 || kit >= K_COUNT || track < 0 || track >= GB_DRUMS) return "?";
    return KITS[kit].t[track].name;
}

void gb_load_kit(Gb *g, int kit)
{
    if (kit < 0 || kit >= K_COUNT) return;
    for (int t = 0; t < GB_DRUMS; t++) {
        GbDrum d = KITS[kit].t[t].d;
        d.level *= KITS[kit].vol;
        if (d.level > 1.0f) d.level = 1.0f;
        *gb_drum(g, t) = d;
    }
}

/* ------------------------------------------------------------------ ritmi pronti
 *
 * Una stringa per traccia, un carattere per passo (la lunghezza della stringa e' la lunghezza della traccia):
 *   .  pausa            x  colpo (vel 100)     X  accento (127)     o  medio (80)     g  fantasma (45)
 *   2 3 4  ratchet (vel 90)                     f  solo col fill     F  solo col fill, ratchet 3
 *   n  solo senza fill  ?  probabilita' 50%    p  probabilita' 25% (vel 70)        r  probabilita' 75%
 *   h  primo giro di due   H  secondo giro di due   q  un giro su quattro
 *   l  in ritardo (+20% di passo)   e  in anticipo (-12%)
 * Un primo carattere ^ = velocita' doppia (trentaduesimi), _ = meta' (ottavi), ~ = terzine.
 */
typedef struct {
    const char *name;
    int kit;
    float bpm, swing;
    const char *trk[GB_DRUMS];
} GrooveDef;

static const GrooveDef GROOVES[] = {
    { "Cassa dritta house", K_HOUSE, 124, .10f, {
        "x...x...x...x...", 0, "......g.....p...", "....x.......x..F",
        "x..gx..gx..gx..g", "..x...x...x...x.", "............f.f.", "..........f.f...",
        0, "...x..g...x.o...", "gogogogogogogogo", "....?.......x..." } },
    { "Techno", K_INDUS, 132, 0, {
        "x...x...x...x...", "............f.FF", "..g...p...g...?.", "....h.......x...",
        "gg.ggg.ggg.ggg.g", "..x...x...x...x.", "..o..o....o..o..", "....p.......?...",
        "g.g.g.g.g.g.g.g.", "...x......x.....", "q...............", "......?.....p..." } },
    { "Boom bap", K_DMX, 90, .18f, {
        "x......x..x...?.", "....x.......x...", "..........g.....", "............f...",
        "x.o.x.o.x.o.x.o.", "..............f.", "............f.f.", "..........ff....",
        "q...............", ".......p........", "...?.......?....", 0 } },
    { "Trap", K_TRAP, 140, 0, {
        "x......x..x.....", "........x.......", "...g......g.....", "........x.......",
        "x.x.x.x.x.x.3.x.x.x.x.x.4.x.2.4.", "............p...", "............f.f.", "..............ff",
        "q...............", "..........p.....", 0, "q..............." } },
    { "Drill", K_808, 142, 0, {
        "x..x......x..x..", "........x....x..", "......g.........", "........x.......",
        "~x.xx.x.xxx.x", "..........p.....", "............f.f.", "..............ff",
        "q...............", "..p.........p...", "......g.........", "...........g...." } },
    { "Amen jungle", K_JUNGLE, 165, 0, {
        "x.x.......xx....x.x.......x.....", "....x..g.g..x..g....x..g.g..FFx.", "..............................f.", 0,
        "..............................ff", 0, "............................f.f.", "............................ff..",
        "o.o.o.o.o.o.o.o.o.o.o.o.o.o.o.o.", "q...............................", "g.g.g.g.g.g.g.g.g.g.g.g.g.g.g.g.",
        "x.........x.....x.........x....." } },
    { "Drum'n'bass", K_JUNGLE, 174, 0, {
        "x.........x.....", "....x.......x...", 0, "..g....g.g....g.",
        "x.o.x.o.x.o.x.o.", "..............?.", "............ff..", "..............ff",
        0, "q...............", "gogogogogogogogo", "x.........x....." } },
    { "Breakbeat", K_LINN, 128, .08f, {
        "x......x.x....x.", "....x..g.g..x..g", 0, "............f...",
        "x.o.x.o.x.o.x.o.", "...........x....", "............ff..", "..............ff",
        "q...............", 0, ".....g.....g....", "....x.......x..." } },
    { "Elettro", K_ELECTRO, 128, 0, {
        "x.....x...x.x...", "....x.......x...", 0, "....x.......x...",
        "xoxoxoxoxoxoxoxo", "..............x.", "..........f.f...", "............f.f.",
        "q...............", "o..o..o...o.o...", "..p.........p...", "...x......x....." } },
    { "Disco", K_909, 120, 0, {
        "x...x...x...x...", "....x.......x...", 0, "....x.......x...",
        "x...x...x...x...", "..x...x...x...x.", "............f.f.", "..........f.f...",
        0, "q...............", "gogogogogogogogo", "....x.......x..." } },
    { "Funk", K_CR78, 100, .12f, {
        "x.x.......x..x..", "....x..g.g..x..g", 0, "...x..x...x.....",
        "xgxgxgxgxgxgxgxg", "..........x.....", "..g....g..g.....", ".g..g......g..g.",
        "q...............", "x.......x.......", "gogogogogogogogo", "....x.......x..." } },
    { "Rock", K_ROCK, 110, 0, {
        "x.....x.x.......", "....x.......x...", 0, "..........g.....",
        "x.x.x.x.x.x.x.x.", "..............f.", "............f..f", "............ff..",
        "q...............", 0, "..............f.", "....x.......x..." } },
    { "Shuffle", K_ROCK, 96, 0, {
        "~x.....x.x...", "~...x.....x..", 0, "~....g......g",
        "~x.xx.xx.xx.x", 0, "~.........fff", "~......fff...",
        0, 0, 0, "~...x.....x.." } },
    { "Bossa nova", K_BOSSA, 135, 0, {
        "x..x....x..x....", 0, "x..x..x...x..x....x..x...x..x...", 0,
        "xgogxgogxgogxgog", 0, "..............f.", "............f...",
        "o.o.o.o.o.o.o.o.", 0, 0, 0 } },
    { "Samba", K_BOSSA, 100, .12f, {
        "g...X...g...X...", "x.gxx.gxx.gxx.gx", 0, 0,
        "xgggxgggxgggxggg", 0, "..............ff", "x.x.xx.x.x.xx.x.",
        0, "x.x...x.x.x...x.", "ogxgogxgogxgogxg", "......x.......x." } },
    { "Dembow", K_DEMBOW, 96, 0, {
        "x...x...x...x...", "...x..x....x..x.", 0, "...........f..f.",
        "x.o.x.o.x.o.x.o.", "..............p.", "............f.f.", "..........f.f...",
        "q...............", "..............f.", "..g...g...g...g.", "gogogogogogogogo" } },
    { "Afrobeat", K_AFRO, 110, .1f, {
        "x..x....x.x.....", "..g.x..g.g..x...", 0, "............f...",
        "x.oxx.oxx.oxx.ox", "..........x.....", "x.....x..x......", "..o.o...o...o.o.",
        0, "x.x.x.x..x.x.x..", "gogogogogogogogo", "......x.....p..." } },
    { "Reggae one drop", K_DUB, 75, .18f, {
        "........X.......", "...............f", "....g...X.......", 0,
        "x.ogx.ogx.ogx.og", "......x.......?.", "............f.f.", "..........f.f...",
        "q...............", "gogogogogogogogo", "..o...o...o...o.", "q..............." } },
    { "Dub steppers", K_DUB, 136, .08f, {
        "x...x...x...x...", "...............F", "........x.......", 0,
        "g.x.g.x.g.x.g.x.", "..............x.", "............f.f.", "..........f.f...",
        0, "gogogogogogogogo", "...g.......p....", "h..............." } },
    { "Jazz swing", K_JAZZ, 140, 0, {
        "~g..g..g..g..", "~x.....x.....", "~......g...p.", "~...........?",
        "~...x.....x..", 0, "~.........fff", "~......fff...",
        "~x..x.xx..x.x", "q...............", 0, 0 } },
    { "Lo-fi hip hop", K_LOFI, 82, .22f, {
        "x.......x.x.....", "....l.......l...", "......g.........", "........p.......",
        "x.g.x.g.x.g.x.g.", "..............p.", "............f...", "..............f.",
        0, "..g...g...g...g.", "...........g....", "h..............." } },
    { "Chiptune", K_NES, 150, 0, {
        "x.......x.x.....", "....x.......x...", 0, "............f...",
        "x.x.x.x.x.x.x.x.", "..............x.", "............f.f.", "..........f.f...",
        "q...............", "..x.......x.....", "...p......p.....", "...............f" } },
    { "Game Boy", K_GB, 160, 0, {
        "x..x..x...x..x..", "....x.......x...", "..p.....p.......", ".......g......x.",
        "xgxgxgxgxgxgxgxg", 0, "............f.f.", "..........f.f...",
        "q...............", "x.......x.......", "......p.....p...", "...........?...." } },
    { "Minimal", K_MINIMAL, 124, .06f, {
        "x...x...x...x...", "...g......x.....", "....h.......x...", "..........p.....",
        "g.x.g.x.g.x.g.x.", "......p.........", "...........o....", "..x..?.x..x.",
        0, "......g.......g.", "gogogogogogogogo", ".g....g..g......" } },
    { "IDM", K_IDM, 150, 0, {
        "x..?...x.x..p.x.", "....x..2.....x..", "..p..p...p..p..p", "..........4.....",
        "^x.xx.x.xx.xxx.xx.x.xx.x.xx.xxx.", "...........?....", ".......3........", "...p.....p......",
        0, "?..?..?..?..?..?", "q...............", "x..p." } },
    { "Gabber", K_GABBER, 180, 0, {
        "x...x...x...x...", "............f.FF", 0, "....h.......x...",
        "...g...g...g...g", "..x...x...x...x.", "............f.f.", "..........f.f...",
        "q...............", "..............f.", "q...............", "...........p...." } },
    { "Ambient", K_AMBIENT, 80, .1f, {
        "x.........?.....", "....o.......o...", "..........p.....", "......p.........",
        "g.p.g.p.g.p.g.p.", "........p.......", "......?.........", "..g..?..g...",
        "q...............", "x....?....", "...p......p.....", "........q......." } },
    { "Poliritmo 3 su 4", K_SIMMONS, 120, 0, {
        "x...x...x...x...", "....x.......x...", 0, "............f...",
        "x.o.x.o.x.o.x.o.", 0, "x....o", "x..",
        "q...............", "~x..x..x..x..", "?....", "x..o..." } },
    { "Half time", K_LINN, 140, 0, {
        "x......x.....x..", "........x.......", 0, "........x.......",
        "x.x.x.x.x.x.x.x.", "..............x.", "............f.f.", "..............ff",
        "q...............", 0, "...g......g.....", "........x......." } },
    { "Footwork", K_808, 160, 0, {
        "x..x..x...x..x..", "............FF..", 0, "....x.......x...",
        "xgxgxgxgxgxgxgxg", 0, "......x.....x...", "..x.......x.....",
        0, 0, "...x..x...x..x..", "..g..g..g..g...." } },
    { "Latina: son e tumbao", K_LATIN, 100, 0, {
        "...x......x.....", "............f.ff", 0, 0,
        "gogogogogogogogo", 0, "...............x", "g.g.X.g.g.g.g.xx",
        "x.o.x.o.x.o.x.o.", "x..x..x...x.x...", "x.gox.gox.gox.go", "x.x.xx.x.x.xx.x." } },
    { "Acid house", K_606, 125, .08f, {
        "x...x...x...x...", "....x.......x...", 0, "............f...",
        "xo.oxo.oxo.oxo.o", "..x...x...x...x.", "..........f.f...", "............f.f.",
        "q...............", "...p.......p....", "......g.........", 0 } },
    { "Garage 2-step", K_909, 132, .2f, {
        "x......x..x.....", "....x.......x...", "..g.....g...g...", "............f...",
        "g.x.g.x.g.x.g.x.", "..............p.", "............f.f.", "..........f.f...",
        0, "q...............", "gogogogogogogogo", "....x.......x..." } },
};

#define GROOVE_COUNT ((int)(sizeof(GROOVES) / sizeof(GROOVES[0])))

int gb_groove_count(void) { return GROOVE_COUNT; }
const char *gb_groove_name(int i) { return (i >= 0 && i < GROOVE_COUNT) ? GROOVES[i].name : "?"; }
int gb_groove_kit(int i) { return (i >= 0 && i < GROOVE_COUNT) ? GROOVES[i].kit : 0; }
float gb_groove_bpm(int i) { return (i >= 0 && i < GROOVE_COUNT) ? GROOVES[i].bpm : 120.0f; }
float gb_groove_swing(int i) { return (i >= 0 && i < GROOVE_COUNT) ? GROOVES[i].swing : 0.0f; }

static void track_from_string(const char *s, GbTrackPattern *tp)
{
    for (int i = 0; i < GB_MAX_STEPS; i++) {
        GbStep *st = &tp->step[i];
        memset(st, 0, sizeof(*st));
        st->vel = 100;
        st->prob = 100;
        st->ratchet = 1;
        st->len = 1;
        st->lock_tune = st->lock_decay = st->lock_tone = GB_LOCK_NONE;
    }
    tp->length = 16;
    tp->speed = 0;
    if (!s) return;
    if (*s == '^') { tp->speed = 1; s++; }
    else if (*s == '_') { tp->speed = 2; s++; }
    else if (*s == '~') { tp->speed = 3; s++; }
    int n = 0;
    for (; *s && n < GB_MAX_STEPS; s++, n++) {
        GbStep *st = &tp->step[n];
        st->on = 1;
        switch (*s) {
        case 'x': break;
        case 'X': st->vel = 127; break;
        case 'o': st->vel = 80; break;
        case 'g': st->vel = 45; break;
        case '2': case '3': case '4': st->ratchet = (uint8_t)(*s - '0'); st->vel = 90; break;
        case 'f': st->cond = COND_FILL; st->vel = 110; break;
        case 'F': st->cond = COND_FILL; st->ratchet = 3; st->vel = 110; break;
        case 'n': st->cond = COND_NOT_FILL; break;
        case '?': st->prob = 50; st->vel = 90; break;
        case 'p': st->prob = 25; st->vel = 70; break;
        case 'r': st->prob = 75; st->vel = 85; break;
        case 'h': st->cond = COND_1_2; break;
        case 'H': st->cond = COND_2_2; break;
        case 'q': st->cond = COND_1_4; break;
        case 'l': st->micro = 20; st->vel = 95; break;
        case 'e': st->micro = -12; st->vel = 95; break;
        default: st->on = 0; break;
        }
    }
    tp->length = (uint8_t)(n > 0 ? n : 16);
}

void gb_groove_load(int i, GbPattern *dst)
{
    if (i < 0 || i >= GROOVE_COUNT || !dst) return;
    for (int t = 0; t < GB_DRUMS; t++) track_from_string(GROOVES[i].trk[t], &dst->track[t]);
}
