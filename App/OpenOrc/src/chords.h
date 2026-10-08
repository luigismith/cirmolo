/* OpenOrc - teoria degli accordi: costruzione, tonalita', voicing e nomi. Solo funzioni pure.
 *
 * Gli intervalli sono semitoni dalla fondamentale, in ordine crescente (0..23); la maschera ha un bit
 * per intervallo. I nomi italiani usano Do Re Mi ("Lam7", "Sibmaj7"), i simboli inglesi C D E ("Am7").
 */
#ifndef OPENORC_CHORDS_H
#define OPENORC_CHORDS_H

#include <stddef.h>

enum { Q_MAJ, Q_MIN, Q_SUS4, Q_DIM, Q_AUG, Q_SUS2, Q_COUNT };
enum { EXT_6 = 1, EXT_7 = 2, EXT_MAJ7 = 4, EXT_9 = 8 };     /* i pad 6, m7, M7 e 9 */
enum { SPELL_MIX, SPELL_SHARP, SPELL_FLAT };                 /* MIX: Do# Mib Fa# Lab Sib */
enum { SPREAD_CLOSE, SPREAD_OPEN, SPREAD_WIDE, SPREAD_COUNT };

#define CHORD_MAX_IV 8

/* Accordo di qualita' e estensioni date. Con 6 e settima insieme la sesta diventa tredicesima. */
int chord_intervals(int quality, int ext, int *iv);
/* Accordo diatonico sul grado 0..6 della scala maggiore o minore naturale; settima, sesta e nona sono
   prese dalla scala (EXT_7 o EXT_MAJ7 danno la settima della scala). */
int chord_diatonic(int key, int minor, int degree, int ext, int *root, int *iv);
/* Grado della nota pc nella tonalita': esatto, o quello subito sotto se la nota e' fuori scala. */
int scale_degree(int key, int minor, int pc);
int scale_pc(int key, int minor, int degree);
/* Modificatori: terza maggiore <-> minore (il diminuito diventa maggiore), terza -> quarta (sus4). */
int chord_swap_third(int *iv, int n);
int chord_sus4(int *iv, int n);

unsigned chord_mask(const int *iv, int n);
/* Suffisso del nome: "", "m", "7", "maj7", "m7b5", "dim7", "sus4", "6/9"... */
const char *chord_suffix(unsigned mask);
void chord_name(char *out, size_t size, int root, unsigned mask, int spelling);     /* "Lam7" */
void chord_symbol(char *out, size_t size, int root, unsigned mask, int spelling);   /* "Am7" */
/* Numero romano del grado, minuscolo per gli accordi minori: "ii7", "V7", "vii\xc2\xb0", "IVmaj7". */
void chord_numeral(char *out, size_t size, int degree, unsigned mask);

const char *note_it(int pc, int spelling);
const char *note_en(int pc, int spelling);
int key_spelling(int key, int minor);
void key_name(char *out, size_t size, int key, int minor);                         /* "Sib maggiore" */

/* Voicing: tra tutti i rivolti (e le ottave) sceglie quello con la nota media piu' vicina al registro
   position (0 = grave, 1 = acuto). Con l'accordo precedente (prev, prev_n > 0) pesa anche quanto si
   muovono le voci, cosi' passando da un accordo all'altro le parti restano legate.
   Restituisce le note MIDI in ordine crescente. */
int chord_voicing(int root, const int *iv, int n, float position, int spread, const int *prev, int prev_n,
                  int *out, int out_max);
/* Basso: la fondamentale tra Do2 (36) e Si2 (47). */
int chord_bass(int root);

#endif
