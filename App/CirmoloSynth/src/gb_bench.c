/* Cirmolo Synth - misura dei tempi sulla console, senza finestra: disegno di ogni pagina e calcolo
 * dell'audio con il ritmo che suona.  gb-bench <cartella dei font> <file di stato> */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gb_app.h"
#include "gb_dsp.h"
#include "gfx.h"

#ifdef GB_PROFILE
extern double gb_prof[3];
#endif
double gb_prof_now(void);
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

double gb_prof_now(void) { return now_ms(); }

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *state = argc > 2 ? argv[2] : "/tmp/gb-bench.txt";
    char f1[512], f2[512];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;
    Canvas c = { calloc(640 * 480, 4), 640, 480 };
    Gb *g = gb_create(GB_RATE);
    App *a = app_create(g, state);
    gb_play(g, 1);
    static float buf[1024 * 2];
    static const char *names[SCR_COUNT] = { "Ritmo", "Suoni", "Mixer", "Live", "Pattern" };
    for (int s = 0; s < SCR_COUNT; s++) {
        app_set_screen(a, s);
        double draw = 0, worst = 0, audio = 0;
        int n = 120;
        for (int i = 0; i < n; i++) {
            double t0 = now_ms();
            gb_render(g, buf, 800);                 /* un fotogramma di audio a 60 Hz */
            double t1 = now_ms();
            app_update(a, 1 / 60.0f);
            app_draw(a, &c);
            double t2 = now_ms();
            audio += t1 - t0;
            draw += t2 - t1;
            if (t2 - t1 > worst) worst = t2 - t1;
        }
#ifdef GB_PROFILE
        printf("   sfondo %.2f, intestazione %.2f, pagina %.2f ms\n", gb_prof[0] / n, gb_prof[1] / n, gb_prof[2] / n);
        gb_prof[0] = gb_prof[1] = gb_prof[2] = 0;
#endif
        printf("%-8s disegno %.2f ms (peggiore %.2f), audio %.2f ms per 800 campioni\n", names[s], draw / n, worst, audio / n);
    }
    app_destroy(a);
    gb_destroy(g);
    remove(state);
    return 0;
}
