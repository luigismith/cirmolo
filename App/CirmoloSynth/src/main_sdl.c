/* Cirmolo Synth - programma per la console (e per provarlo sul PC).
 *
 * SDL2 viene caricata a runtime (dlopen/LoadLibrary): sulla Flip si usa la libreria di PyUI
 * (/mnt/SDCARD/App/PyUI/dll/libSDL2-2.0.so, SDL 2.32 con ALSA e KMSDRM), senza dipendere dalla
 * versione del firmware. I tasti della Flip si leggono da /dev/input/event5 come fa PyUI;
 * tastiera e gamepad SDL restano come riserva (e servono sul PC).
 *
 * Uso: cirmolo-synth [--fonts DIR] [--state FILE] [--window] [--frames N]
 */
#define SDL_MAIN_HANDLED
#include "SDL.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "gfx.h"
#include "synth.h"

#ifdef _WIN32
#include <windows.h>
#define LIB_OPEN(p) ((void *)LoadLibraryA(p))
#define LIB_SYM(h, n) ((void *)GetProcAddress((HMODULE)(h), n))
#else
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#define LIB_OPEN(p) dlopen(p, RTLD_NOW | RTLD_GLOBAL)
#define LIB_SYM(h, n) dlsym(h, n)
#endif

#define W 640
#define H 480

/* ------------------------------------------------------------------ SDL caricata a runtime */
#define SDL_FUNCS(X) \
    X(SDL_Init) X(SDL_Quit) X(SDL_GetError) X(SDL_CreateWindow) X(SDL_DestroyWindow) \
    X(SDL_CreateRenderer) X(SDL_DestroyRenderer) X(SDL_CreateTexture) X(SDL_DestroyTexture) \
    X(SDL_UpdateTexture) X(SDL_RenderClear) X(SDL_RenderCopy) X(SDL_RenderPresent) \
    X(SDL_RenderSetLogicalSize) X(SDL_PollEvent) X(SDL_OpenAudioDevice) X(SDL_PauseAudioDevice) \
    X(SDL_CloseAudioDevice) X(SDL_Delay) X(SDL_GetTicks) X(SDL_ShowCursor) X(SDL_NumJoysticks) \
    X(SDL_IsGameController) X(SDL_GameControllerOpen) X(SDL_GameControllerGetAxis) X(SDL_GetRendererInfo) \
    X(SDL_SetHint) X(SDL_GetCurrentDisplayMode)

#define DECL(f) static __typeof__(f) *p_##f;
SDL_FUNCS(DECL)
#undef DECL

static int load_sdl(void)
{
    const char *candidates[] = {
        getenv("CIRMOLO_SDL"),
#ifdef _WIN32
        "SDL2.dll",
#else
        "/mnt/SDCARD/App/PyUI/dll/libSDL2-2.0.so",
        "/mnt/SDCARD/spruce/flip/lib/libSDL2-2.0.so",
        "libSDL2-2.0.so.0",
        "libSDL2-2.0.so",
#endif
    };
    void *h = NULL;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) && !h; i++)
        if (candidates[i]) {
            h = LIB_OPEN(candidates[i]);
            if (h) fprintf(stderr, "SDL: %s\n", candidates[i]);
        }
    if (!h) { fprintf(stderr, "SDL2 non trovata\n"); return -1; }
#define LOAD(f) if (!(p_##f = (__typeof__(f) *)LIB_SYM(h, #f))) { fprintf(stderr, "SDL2: manca %s\n", #f); return -1; }
    SDL_FUNCS(LOAD)
#undef LOAD
    return 0;
}

/* ------------------------------------------------------------------ audio */
static Synth *g_synth;

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    if (g_synth) synth_render(g_synth, (float *)stream, len / (int)(2 * sizeof(float)));
    else memset(stream, 0, (size_t)len);
}

/* ------------------------------------------------------------------ ingressi */
typedef struct { float lx, ly, rx, ry, l2, r2; } Axes;

static void trigger(App *a, int btn, float v, float *prev)
{
    int was = *prev > 0.5f, now = v > 0.5f;
    if (was != now) app_button(a, btn, now);
    *prev = v;
}

#ifndef _WIN32
typedef struct { int fd; struct input_absinfo abs[ABS_CNT]; int have[ABS_CNT]; } Evdev;

static int evdev_open(Evdev *e, const char *path)
{
    e->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (e->fd < 0) return -1;
    for (int c = 0; c < ABS_CNT; c++)
        e->have[c] = ioctl(e->fd, EVIOCGABS(c), &e->abs[c]) == 0 && e->abs[c].maximum > e->abs[c].minimum;
    fprintf(stderr, "input: %s\n", path);
    return 0;
}

static float evdev_norm(Evdev *e, int code, int value, int unipolar)
{
    if (!e->have[code]) return 0.0f;
    float mn = (float)e->abs[code].minimum, mx = (float)e->abs[code].maximum;
    float t = (value - mn) / (mx - mn);
    return unipolar ? t : t * 2.0f - 1.0f;
}

static int g_logged;   /* eventi di input gia' scritti nel log (i primi 300 servono per la diagnosi) */

static void evdev_poll(Evdev *e, App *a, Axes *ax, float *l2p, float *r2p, int *hx, int *hy)
{
    struct input_event ev;
    while (read(e->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type != EV_SYN && g_logged < 300) {
            fprintf(stderr, "evdev tipo %d codice %d valore %d\n", ev.type, ev.code, ev.value);
            g_logged++;
        }
        if (ev.type == EV_KEY) {
            int b = -1;
            switch (ev.code) {
            case 305: b = BTN_A; break;          /* tasto A (a destra) */
            case 304: b = BTN_B; break;          /* B (in basso) */
            case 308: b = BTN_X; break;          /* X (in alto) */
            case 307: b = BTN_Y; break;          /* Y (a sinistra) */
            case 310: b = BTN_L1; break;
            case 311: b = BTN_R1; break;
            case 314: b = BTN_SELECT; break;
            case 315: b = BTN_START; break;
            case 316: b = BTN_MENU; break;
            }
            if (b >= 0 && ev.value != 2) app_button(a, b, ev.value != 0);
        } else if (ev.type == EV_ABS) {
            switch (ev.code) {
            case ABS_HAT0X:
                if (*hx < 0) app_button(a, BTN_LEFT, 0);
                if (*hx > 0) app_button(a, BTN_RIGHT, 0);
                *hx = ev.value;
                if (ev.value < 0) app_button(a, BTN_LEFT, 1);
                if (ev.value > 0) app_button(a, BTN_RIGHT, 1);
                break;
            case ABS_HAT0Y:
                if (*hy < 0) app_button(a, BTN_UP, 0);
                if (*hy > 0) app_button(a, BTN_DOWN, 0);
                *hy = ev.value;
                if (ev.value < 0) app_button(a, BTN_UP, 1);
                if (ev.value > 0) app_button(a, BTN_DOWN, 1);
                break;
            case ABS_X: ax->lx = evdev_norm(e, ev.code, ev.value, 0); break;
            case ABS_Y: ax->ly = evdev_norm(e, ev.code, ev.value, 0); break;
            case ABS_RX: ax->rx = evdev_norm(e, ev.code, ev.value, 0); break;
            case ABS_RY: ax->ry = evdev_norm(e, ev.code, ev.value, 0); break;
            case ABS_Z: ax->l2 = evdev_norm(e, ev.code, ev.value, 1); trigger(a, BTN_L2, ax->l2, l2p); break;
            case ABS_RZ: ax->r2 = evdev_norm(e, ev.code, ev.value, 1); trigger(a, BTN_R2, ax->r2, r2p); break;
            }
        }
    }
}
#endif

static int key_to_button(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: return BTN_UP;
    case SDLK_DOWN: return BTN_DOWN;
    case SDLK_LEFT: return BTN_LEFT;
    case SDLK_RIGHT: return BTN_RIGHT;
    case SDLK_x: return BTN_A;
    case SDLK_z: return BTN_B;
    case SDLK_s: return BTN_X;
    case SDLK_a: return BTN_Y;
    case SDLK_q: return BTN_L1;
    case SDLK_w: return BTN_R1;
    case SDLK_1: return BTN_L2;
    case SDLK_2: return BTN_R2;
    case SDLK_BACKSPACE: case SDLK_RSHIFT: return BTN_SELECT;
    case SDLK_RETURN: return BTN_START;
    case SDLK_ESCAPE: return BTN_MENU;
    }
    return -1;
}

static int pad_to_button(int b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_B: return BTN_A;       /* posizioni fisiche: est */
    case SDL_CONTROLLER_BUTTON_A: return BTN_B;       /* sud */
    case SDL_CONTROLLER_BUTTON_Y: return BTN_X;       /* nord */
    case SDL_CONTROLLER_BUTTON_X: return BTN_Y;       /* ovest */
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return BTN_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_R1;
    case SDL_CONTROLLER_BUTTON_BACK: return BTN_SELECT;
    case SDL_CONTROLLER_BUTTON_START: return BTN_START;
    case SDL_CONTROLLER_BUTTON_GUIDE: return BTN_MENU;
    }
    return -1;
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv)
{
    const char *fonts = "/mnt/SDCARD/App/PyUI/fonts";
    const char *state = "/mnt/SDCARD/Saves/cirmolo-synth/stato.txt";
    int windowed = 0, max_frames = 0;
#ifdef _WIN32
    windowed = 1;
    state = "stato.txt";
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fonts") && i + 1 < argc) fonts = argv[++i];
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) state = argv[++i];
        else if (!strcmp(argv[i], "--window")) windowed = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atoi(argv[++i]);
    }
#ifndef _WIN32
    {   /* crea la cartella dei salvataggi */
        char dir[512];
        snprintf(dir, sizeof(dir), "%s", state);
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = 0; mkdir(dir, 0755); }
    }
#endif

    if (load_sdl()) return 1;
    char f1[512], f2[512];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;

    if (p_SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", p_SDL_GetError());
        return 1;
    }
    /* come PyUI: a schermo intero alla risoluzione corrente (640x480, o quella dell'HDMI) */
    int ww = W, wh = H;
    SDL_DisplayMode mode;
    if (!windowed && p_SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.w > 0 && mode.h > 0) { ww = mode.w; wh = mode.h; }
    p_SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Window *win = p_SDL_CreateWindow("Cirmolo Synth", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
                                         windowed ? 0 : SDL_WINDOW_FULLSCREEN);
    fprintf(stderr, "finestra: %dx%d%s\n", ww, wh, windowed ? "" : " a schermo intero");
    if (!win) { fprintf(stderr, "finestra: %s\n", p_SDL_GetError()); p_SDL_Quit(); return 1; }
    p_SDL_ShowCursor(SDL_DISABLE);
    SDL_Renderer *ren = p_SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = p_SDL_CreateRenderer(win, -1, 0);
    if (!ren) { fprintf(stderr, "renderer: %s\n", p_SDL_GetError()); p_SDL_Quit(); return 1; }
    SDL_RendererInfo info;
    int vsync = 0;
    if (p_SDL_GetRendererInfo(ren, &info) == 0) {
        vsync = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        fprintf(stderr, "renderer: %s%s\n", info.name, vsync ? " (vsync)" : "");
    }
    p_SDL_RenderSetLogicalSize(ren, W, H);
    SDL_Texture *tex = p_SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);

    /* audio: si apre in pausa, il synth nasce con la frequenza ottenuta */
    SDL_AudioSpec want = { 0 }, have = { 0 };
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;              /* con 512 ALSA segnalava qualche underrun sulla Flip */
    want.callback = audio_cb;
    SDL_AudioDeviceID dev = p_SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    float rate = dev ? (float)have.freq : 48000.0f;
    if (!dev) fprintf(stderr, "audio non disponibile: %s\n", p_SDL_GetError());
    else fprintf(stderr, "audio: %d Hz, %d campioni per blocco\n", have.freq, have.samples);
    Synth *synth = synth_create(rate);
    App *app = app_create(synth, state);
    g_synth = synth;
    if (dev) p_SDL_PauseAudioDevice(dev, 0);

    for (int i = 0; i < p_SDL_NumJoysticks(); i++)
        if (p_SDL_IsGameController(i)) p_SDL_GameControllerOpen(i);

#ifndef _WIN32
    Evdev ev = { -1 };
    int have_evdev = evdev_open(&ev, getenv("CIRMOLO_INPUT") ? getenv("CIRMOLO_INPUT") : "/dev/input/event5") == 0;
    int hx = 0, hy = 0;
    /* Con evdev aperto, gamepad e tastiera di SDL vengono ignorati: leggono lo stesso dispositivo
       e ogni tasto arriverebbe due volte (con mappature diverse). */
    int sdl_input = !have_evdev || getenv("CIRMOLO_SDL_INPUT") != NULL;
#else
    int sdl_input = 1;
#endif
    fprintf(stderr, "ingressi: %s\n", sdl_input ? "SDL (tastiera e gamepad)" : "evdev");
    uint32_t *px = malloc(sizeof(uint32_t) * W * H);
    Canvas canvas = { px, W, H };
    Axes ax = { 0 }, pad = { 0 };
    float l2p = 0, r2p = 0, pl2p = 0, pr2p = 0;
    uint32_t last = p_SDL_GetTicks();
    int frames = 0, running = 1;

    while (running) {
        SDL_Event e;
        while (p_SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; continue; }
#ifndef _WIN32
            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_CONTROLLERBUTTONDOWN ||
                 e.type == SDL_CONTROLLERBUTTONUP) && g_logged < 300) {
                fprintf(stderr, "sdl evento %#x %s %d\n", e.type, sdl_input ? "usato" : "ignorato",
                        (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) ? (int)e.key.keysym.sym : (int)e.cbutton.button);
                g_logged++;
            }
#endif
            if (!sdl_input) continue;
            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                int b = key_to_button(e.key.keysym.sym);
                if (b >= 0) app_button(app, b, e.type == SDL_KEYDOWN);
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
                int b = pad_to_button(e.cbutton.button);
                if (b >= 0) app_button(app, b, e.type == SDL_CONTROLLERBUTTONDOWN);
            } else if (e.type == SDL_CONTROLLERAXISMOTION) {
                float v = e.caxis.value / 32767.0f;
                switch (e.caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX: pad.lx = v; break;
                case SDL_CONTROLLER_AXIS_LEFTY: pad.ly = v; break;
                case SDL_CONTROLLER_AXIS_RIGHTX: pad.rx = v; break;
                case SDL_CONTROLLER_AXIS_RIGHTY: pad.ry = v; break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT: pad.l2 = v; trigger(app, BTN_L2, v, &pl2p); break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: pad.r2 = v; trigger(app, BTN_R2, v, &pr2p); break;
                }
            }
        }
#ifndef _WIN32
        if (have_evdev) evdev_poll(&ev, app, &ax, &l2p, &r2p, &hx, &hy);
#endif
        /* vince la sorgente che si sta muovendo di piu' */
        Axes use = (fabsf(pad.lx) + fabsf(pad.ly) + fabsf(pad.rx) + fabsf(pad.ry) > fabsf(ax.lx) + fabsf(ax.ly) + fabsf(ax.rx) + fabsf(ax.ry)) ? pad : ax;
        app_axes(app, use.lx, use.ly, use.rx, use.ry, fmaxf(ax.l2, pad.l2), fmaxf(ax.r2, pad.r2));

        uint32_t now = p_SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        app_update(app, dt > 0.1f ? 0.1f : dt);
        if (app_wants_quit(app)) running = 0;

        app_draw(app, &canvas);
        p_SDL_UpdateTexture(tex, NULL, px, W * 4);
        p_SDL_RenderClear(ren);
        p_SDL_RenderCopy(ren, tex, NULL, NULL);
        p_SDL_RenderPresent(ren);
        if (!vsync) {
            uint32_t spent = p_SDL_GetTicks() - now;
            if (spent < 16) p_SDL_Delay(16 - spent);
        }
        frames++;
        if (max_frames && frames >= max_frames) running = 0;
    }

    if (dev) { p_SDL_PauseAudioDevice(dev, 1); p_SDL_CloseAudioDevice(dev); }
    g_synth = NULL;
    app_destroy(app);
    synth_destroy(synth);
#ifndef _WIN32
    if (have_evdev) close(ev.fd);
#endif
    p_SDL_DestroyTexture(tex);
    p_SDL_DestroyRenderer(ren);
    p_SDL_DestroyWindow(win);
    p_SDL_Quit();
    gfx_free_fonts();
    free(px);
    fprintf(stderr, "uscita regolare dopo %d fotogrammi\n", frames);
    return 0;
}
