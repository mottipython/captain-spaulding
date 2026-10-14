/*
 * captain-spaulding: a paper-doll screensaver of Captain Spaulding's arrival, from
 * Animal Crackers (1930).
 *
 * The African explorer, cut out of his publicity photograph as a jointed paper doll
 * with brass split pins, marches on to a cardboard toy-theatre stage and sings and
 * dances "Hooray for Captain Spaulding", in time with the song from the film.
 *
 *   captain-spaulding            full screen on every display; any key or mouse movement quits
 *   captain-spaulding --window   in a window; Esc or Q quits
 *   CaptainSpaulding.scr /s      Windows screensaver conventions
 */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The pictures (drawn with Gemini, prepared by tools/make_sprites.py) are built
 * into the program as PNG and JPEG files and decoded at start-up. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS /* else Windows builds need libwinpthread-1.dll */
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__APPLE__)
/* Mach-O: its own section names, and C names carry a leading underscore */
#define ART_SECTION ".section __DATA,__const\n"
#define ART_SYM "_art_"
#elif defined(_WIN32)
#define ART_SECTION ".section .rdata,\"dr\"\n"
#define ART_SYM "art_"
#else
#define ART_SECTION ".section .rodata\n"
#define ART_SYM "art_"
#endif
#define ART(name, file) \
    __asm__(ART_SECTION ".globl " ART_SYM #name "\n" ART_SYM #name ":\n.incbin \"assets/art/" file "\"\n" \
            ".globl " ART_SYM #name "_end\n" ART_SYM #name "_end:\n.text\n"); \
    extern const unsigned char art_##name[], art_##name##_end[];
/* Every picture, by name and file. The doll's pieces are cut from the photograph by
 * tools/make_doll.py, which also writes where their pins are (doll.h); the backdrop
 * and the curtains are from Pollock's toy-theatre scenes, cut by tools/make_stage.py. */
#define SPRITES \
    X(head, "sp_head.png") X(hat, "sp_hat.png") X(body, "sp_body.png") X(arm_l, "sp_arm_l.png") X(arm_r, "sp_arm_r.png") \
    X(thigh_l, "sp_thigh_l.png") X(thigh_r, "sp_thigh_r.png") X(shin_l, "sp_shin_l.png") X(shin_r, "sp_shin_r.png") \
    X(pin, "sp_pin.png") X(backdrop, "backdrop.jpg") X(drape_l, "drape_l.png") X(drape_r, "drape_r.png")
#define X(name, file) ART(name, file)
SPRITES
#undef X
#define X(name, file) SP_##name,
enum { SPRITES NSPRITE };
#undef X
#define X(name, file) {art_##name, art_##name##_end},
static struct { const unsigned char *file, *end; int w, h; unsigned char *px; } sprite[NSPRITE] = { SPRITES };
#undef X
#include "doll.h"
#include "song.h"
static SDL_Texture **cur_tex; /* the pictures, for the renderer being drawn to */

static void load_sprites(void)
{
    for (int i = 0; i < NSPRITE; i++) {
        int n;
        sprite[i].px = stbi_load_from_memory(sprite[i].file, (int)(sprite[i].end - sprite[i].file), &sprite[i].w, &sprite[i].h, &n, 4);
        if (!sprite[i].px) { sprite[i].w = sprite[i].h = 1; }
    }
}
static void make_textures(SDL_Renderer *r, SDL_Texture **tex)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    for (int i = 0; i < NSPRITE; i++) {
        tex[i] = NULL;
        if (!sprite[i].px) continue;
        tex[i] = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, sprite[i].w, sprite[i].h);
        if (!tex[i]) continue;
        SDL_UpdateTexture(tex[i], NULL, sprite[i].px, sprite[i].w * 4);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
    }
}

/* ---- options ---- */
static int opt_window = 0;
static double opt_speed = 1.0;
static uint64_t opt_seed = 0;
static const char *opt_snapshot = NULL;
static double opt_time = 30;
static int opt_w = 1920, opt_h = 1200;

/* ---- colours ---- */
typedef struct { Uint8 r, g, b; } Rgb;
static const Rgb BG = {8, 8, 20}, INK = {30, 24, 20};

/* ---- geometry buffer ---- */
static SDL_Vertex *vb;
static int *ib;
static int nv, ni, vcap, icap;

static void vb_need(int v, int i)
{
    if (nv + v > vcap) { vcap = (nv + v) * 2; vb = realloc(vb, sizeof *vb * vcap); }
    if (ni + i > icap) { icap = (ni + i) * 2; ib = realloc(ib, sizeof *ib * icap); }
}
static inline SDL_Color col(Rgb c, double a)
{
    SDL_Color o = {c.r, c.g, c.b, (Uint8)(a < 0 ? 0 : a > 1 ? 255 : a * 255)};
    return o;
}
static inline void vert(double x, double y, SDL_Color c)
{
    vb[nv].position.x = (float)x; vb[nv].position.y = (float)y;
    vb[nv].color = c; vb[nv].tex_coord.x = vb[nv].tex_coord.y = 0;
    nv++;
}
static void quad4(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3,
                  SDL_Color c0, SDL_Color c1, SDL_Color c2, SDL_Color c3)
{
    vb_need(4, 6);
    int b = nv;
    vert(x0, y0, c0); vert(x1, y1, c1); vert(x2, y2, c2); vert(x3, y3, c3);
    ib[ni++] = b; ib[ni++] = b + 1; ib[ni++] = b + 2; ib[ni++] = b; ib[ni++] = b + 2; ib[ni++] = b + 3;
}
static void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, SDL_Color c)
{
    quad4(x0, y0, x1, y1, x2, y2, x3, y3, c, c, c, c);
}
static void rect(double x, double y, double w, double h, SDL_Color c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
static void disc(double cx, double cy, double r, SDL_Color centre, SDL_Color rim)
{
    int n = r < 4 ? 10 : r < 30 ? 28 : 56;
    vb_need(n + 1, 3 * n);
    int b = nv;
    vert(cx, cy, centre);
    for (int i = 0; i < n; i++) vert(cx + r * cos(i * 2 * M_PI / n), cy + r * sin(i * 2 * M_PI / n), rim);
    for (int i = 0; i < n; i++) { ib[ni++] = b; ib[ni++] = b + 1 + i; ib[ni++] = b + 1 + (i + 1) % n; }
}
/* a stroke with round ends */
static void line(double x0, double y0, double x1, double y1, double w, SDL_Color c)
{
    double dx = x1 - x0, dy = y1 - y0, l = hypot(dx, dy);
    if (l > 1e-9) {
        double nx = -dy / l * w / 2, ny = dx / l * w / 2;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }
    if (w > 2.5) { disc(x0, y0, w / 2, c, c); disc(x1, y1, w / 2, c, c); }
}
__attribute__((unused)) static void ring(double cx, double cy, double r, double w, SDL_Color c)
{
    for (int i = 0; i < 64; i++) {
        double a = i * M_PI / 32, b = (i + 1) * M_PI / 32;
        line(cx + cos(a) * r, cy + sin(a) * r, cx + cos(b) * r, cy + sin(b) * r, w, c);
    }
}
static void flush(SDL_Renderer *r)
{
    if (ni) SDL_RenderGeometry(r, NULL, vb, nv, ib, ni);
    nv = ni = 0;
}

static SDL_Renderer *ren; /* where the current frame is being drawn */

/* a picture stretched over a rectangle; sx0..sx1 picks a slice of it across (0..1) */
static void pic_part(int which, double x, double y, double w, double h, double sx0, double sx1, double tilt, int flip)
{
    flush(ren);
    if (!cur_tex || !cur_tex[which]) return;
    SDL_Rect src = {(int)(sx0 * sprite[which].w), 0, (int)((sx1 - sx0) * sprite[which].w), sprite[which].h};
    SDL_FRect dst = {(float)x, (float)y, (float)w, (float)h};
    SDL_FPoint pivot = {(float)(w / 2), (float)h};
    SDL_RenderCopyExF(ren, cur_tex[which], &src, &dst, tilt, &pivot, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
/* a picture `tall` high, standing with the point (ax, ay) of it (0..1 across and down) at (x, y) */
__attribute__((unused)) static void pic(int which, double x, double y, double tall, double ax, double ay)
{
    double w = tall * sprite[which].w / sprite[which].h;
    pic_part(which, x - w * ax, y - tall * ay, w, tall, 0, 1, 0, 0);
}

/* ---- lettering: pen strokes on a 4 x 6 grid, drawn with a round nib ---- */
static const char *GLYPH[] = {
    "A06 02 20 42 46|04 44", "B00 06 36 45 44 33 03|33 42 41 30 00", "C41 30 10 01 05 16 36 45",
    "D00 06 26 44 42 20 00", "E40 00 06 46|03 33", "F40 00 06|03 33", "G41 30 10 01 05 16 36 45 43 23",
    "H00 06|40 46|03 43", "I20 26|10 30|16 36", "J40 45 36 16 05", "K00 06|40 03 46", "L00 06 46",
    "M06 00 23 40 46", "N06 00 46 40", "O10 30 41 45 36 16 05 01 10", "P06 00 30 41 42 33 03",
    "Q10 30 41 45 36 16 05 01 10|24 46", "R06 00 30 41 42 33 03|23 46", "S41 30 10 01 02 13 33 44 45 36 16 05",
    "T00 40|20 26", "U00 05 16 36 45 40", "V00 26 40", "W00 16 23 36 40", "X00 46|40 06", "Y00 23 40|23 26",
    "Z00 40 06 46", "010 30 41 45 36 16 05 01 10", "111 20 26|16 36", "201 10 30 41 42 06 46",
    "301 10 30 41 42 33 23|33 44 45 36 16 05", "436 30 04 44", "540 00 03 33 44 45 36 16 05",
    "630 10 01 05 16 36 45 44 33 03", "700 40 16", "813 02 01 10 30 41 42 33 13 04 05 16 36 45 44 33",
    "943 13 02 01 10 30 41 45 36 16", ",25 26 17", ".26 26", ":22 22|25 25", "'20 21", "?01 10 30 41 42 24|26 26",
    "-13 33", "!20 24|26 26", "+03 43|21 25", "=02 42|04 44",
};
static const char *glyph(char ch)
{
    for (unsigned i = 0; i < sizeof GLYPH / sizeof *GLYPH; i++)
        if (GLYPH[i][0] == ch) return GLYPH[i] + 1;
    return NULL;
}
static double text_width(const char *str, double s) { return strlen(str) * 6.2 * s - 2.2 * s; }
/* text centred on cx, top at y; s is one grid step, so letters are 6s tall */
static void text(const char *str, double cx, double y, double s, double weight, SDL_Color c)
{
    double x = cx - text_width(str, s) / 2;
    for (; *str; str++, x += 6.2 * s) {
        const char *g = glyph(*str);
        if (!g) continue;
        int have = 0; double lx = 0, ly = 0;
        for (; *g; g++) {
            if (*g == '|') { have = 0; continue; }
            if (*g == ' ') continue;
            double gx = x + (g[0] - '0') * s, gy = y + (g[1] - '0') * s;
            g++;
            if (have) line(lx, ly, gx, gy, s * weight, c);
            disc(gx, gy, s * weight * (have && lx == gx && ly == gy ? 0.62 : 0.5), c, c); /* the nib is round */
            lx = gx; ly = gy; have = 1;
        }
    }
}
/* ---- the music ----
 *
 * The song from the film, built in as Ogg Vorbis and decoded at the start. The show
 * follows the music's own clock while it plays, so the doll stays in time. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"
#undef L
#undef R
#undef C
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
ART(song, "spaulding.ogg")

static short *pcm;
static int pcm_len, pcm_rate, pcm_channels;
static volatile int pcm_pos;            /* where the sound has got to, in frames */
static SDL_AudioDeviceID audio;
static int opt_mute = 0;

static void load_song(void)
{
    pcm_len = stb_vorbis_decode_memory(art_song, (int)(art_song_end - art_song), &pcm_channels, &pcm_rate, &pcm);
    if (pcm_len <= 0) pcm = NULL;
}
static void play(void *u, Uint8 *out, int bytes)
{
    (void)u;
    short *o = (short *)out;
    int n = bytes / (int)sizeof(short) / pcm_channels;
    for (int i = 0; i < n; i++, pcm_pos++)
        for (int c = 0; c < pcm_channels; c++)
            *o++ = pcm_pos < pcm_len ? pcm[pcm_pos * pcm_channels + c] : 0;
}
static void start_song(void)
{
    if (opt_mute || opt_snapshot || !pcm || audio) return;
    SDL_AudioSpec want = {0}, got;
    want.freq = pcm_rate; want.format = AUDIO_S16SYS; want.channels = (Uint8)pcm_channels;
    want.samples = 2048; want.callback = play;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (audio) { pcm_pos = 0; SDL_PauseAudioDevice(audio, 0); }
}

/* ---- the show ----
 *
 * One time-line, the song's: T seconds into it. The curtains open and he is let down
 * on to the stage on a string. He marks time while the chorus sings, keeps setting
 * off for the wings through "Hello, I must be going", and then does his dance as he
 * does it in the film, move for move: the hat thrown up, the knock-kneed shuffle,
 * high kicks, a crouch, the hat waved overhead, a kicking lope across the stage,
 * scissor lunges, and the hat held before him at the end; and now and then a leg
 * goes right round its pin, which no man could do. Then a bow, the curtains close,
 * and it begins again.
 *
 * The song is the film's, cut together (tools/make_song.py): a time in the song after
 * SONG_PART2 is FILM_OFFSET seconds before the same moment in the film. Film times
 * are in the Wikimedia Commons 1080p copy, which runs at the true speed. */
#define GAP 3.0                         /* seconds of quiet between one time round and the next */
#define FILM(m, s) ((m) * 60 + (s) - FILM_OFFSET)   /* a time in the film, as a time in the song */
#define END (SONG_LENGTH - 4.5)          /* where the bow begins */
static double T, anim_t;
static uint64_t seed_state;

static uint64_t next_rand(void)
{
    seed_state ^= seed_state << 13; seed_state ^= seed_state >> 7; seed_state ^= seed_state << 17;
    return seed_state;
}
static double ramp(double t, double a, double b) { return t <= a ? 0 : t >= b ? 1 : (t - a) / (b - a); }
static double ease(double x) { return x * x * (3 - 2 * x); }

static void update(double dt)
{
    anim_t += dt;
    if (audio) {
        T = (double)pcm_pos / pcm_rate;
        if (pcm_pos >= pcm_len + (int)(GAP * pcm_rate)) { SDL_LockAudioDevice(audio); pcm_pos = 0; SDL_UnlockAudioDevice(audio); T = 0; }
    } else {
        T += dt * opt_speed;
        if (T >= SONG_LENGTH + GAP) T = 0;
    }
}

/* which beat T is in, and how far through it (0..1) */
static int beat_at(double t, double *phase)
{
    int n = sizeof BEAT / sizeof *BEAT, lo = 0, hi = n - 1;
    if (t < BEAT[0]) { *phase = 0; return 0; }
    if (t >= BEAT[n - 1]) { *phase = fmod(t - BEAT[n - 1], 0.5) / 0.5; return n - 1 + (int)((t - BEAT[n - 1]) / 0.5); }
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (BEAT[m] <= t) lo = m; else hi = m; }
    *phase = (t - BEAT[lo]) / (BEAT[lo + 1] - BEAT[lo]);
    return lo;
}

/* The doll's pose: where he stands (across the stage, 0..1), how high he hops (in his
 * heights), the turn of each piece about its pin in degrees (for an arm or a leg,
 * plus swings it out to the left), and how far round he has spun (1 facing us, -1
 * turned about, as a card is turned); `hang` is how far he is held up by his string
 * (as in a swivel, with both feet off the floor). Then the hat: on his head, in his right hand,
 * or flying, and where. */
typedef struct { double x, hop, body, head, arm_l, arm_r, thigh_l, shin_l, thigh_r, shin_r, spin, drop, hang; } Pose;
enum { HAT_ON, HAT_HAND, HAT_FLYING };
typedef struct { int mode; double x, y, turn; } Hat;

/* Two poses a beat apart, and the snap from one to the other: like a paper doll worked
 * by a hand, every move is a quick jerk at the start of the beat, then a hold. */
static double snap(double ph) { return ease(fmin(1, ph / 0.16)); }
static Pose mix(Pose a, Pose b, double f)
{
    Pose p;
    double *pa = &a.x, *pb = &b.x, *pp = &p.x;
    for (unsigned i = 0; i < sizeof p / sizeof(double); i++) pp[i] = pa[i] + (pb[i] - pa[i]) * f;
    return p;
}

/* the moves, from the film */
enum { M_DANGLE, M_MARK, M_WAVE, M_GRIN, M_SHUFFLE, M_KICKS, M_CROUCH, M_HATWAVE, M_HUNCH, M_LOPE, M_SCISSOR, M_SWAY };
static const struct { double from; int move; } ROUTINE[] = {
    {0, M_DANGLE}, {7.0, M_MARK}, {SONG_PART2, M_WAVE},
    {FILM(8, 10.5), M_GRIN},        /* he grins at the crowd */
    {FILM(8, 19.9), M_SHUFFLE},     /* the hat thrown up, a spin, and the shuffle begins */
    {FILM(8, 24.1), M_KICKS},        /* high kicks and a hop */
    {FILM(8, 26.7), M_CROUCH},     /* down in a crouch, a leg swept out */
    {FILM(8, 29.3), M_SHUFFLE},      /* up again: the knock-kneed shuffle, hands at his lapels */
    {FILM(8, 41.8), M_HATWAVE},     /* hat off and waved overhead, big kicks */
    {FILM(8, 46.7), M_HUNCH},       /* hat back on, hunched over */
    {FILM(8, 50.9), M_LOPE},        /* the kicking lope across the stage and back */
    {FILM(9, 0.8), M_SCISSOR},   /* a spin, and scissor lunges */
    {FILM(9, 7.6), M_HATWAVE},     /* a high kick, the hat waved, thrown and caught */
    {FILM(9, 11.2), M_LOPE},      /* off towards the wings with the hat held low, and back */
    {FILM(9, 14.9), M_SWAY},        /* the hat held before him, swaying, grinning */
};
static int move_at(double t)
{
    int m = M_DANGLE;
    for (unsigned i = 0; i < sizeof ROUTINE / sizeof *ROUTINE; i++) if (t >= ROUTINE[i].from) m = ROUTINE[i].move;
    return m;
}

/* the pose he jerks into on beat b */
static Pose key_pose(int b, double t)
{
    Pose p = {0};
    p.spin = 1;
    int odd = b & 1, bar = b / 4, in_bar = b % 4;
    double side = odd ? 1 : -1;
    switch (move_at(t)) {
    case M_DANGLE:                                 /* hanging on his string, limbs loose */
        p.thigh_l = 12 * side; p.thigh_r = 12 * side; p.shin_l = -8 * side; p.shin_r = -8 * side;
        p.arm_l = 30 + 15 * side; p.arm_r = -30 + 15 * side; p.head = 6 * side;
        break;
    case M_MARK:                                   /* marking time, as the chorus sings */
        p.thigh_l = odd ? 45 : 0; p.shin_l = odd ? -70 : 0;
        p.thigh_r = odd ? 0 : -45; p.shin_r = odd ? 0 : 70;
        p.arm_l = odd ? 20 : 95; p.arm_r = odd ? -95 : -20;
        p.head = 10 * side; p.body = 4 * side;
        if (bar % 4 == 3) { p.arm_l = 150; p.arm_r = -150; p.head = -12; }   /* hooray! */
        if (bar % 8 == 6) { p.arm_r = -150; p.arm_l = 20; p.head = 14; }      /* a tip of the hat */
        break;
    case M_WAVE:                                   /* "Hello, I must be going": waving goodbye */
        p.arm_l = 155 + 22 * side; p.arm_r = -30;
        p.thigh_l = odd ? 30 : 0; p.shin_l = odd ? -45 : 0;
        p.thigh_r = odd ? 0 : -30; p.shin_r = odd ? 0 : 45;
        p.head = 12 * side; p.body = -3 * side;
        if (bar % 2) { p.arm_l = 30; p.arm_r = -155 - 22 * side; }
        break;
    case M_GRIN:                                   /* grinning, hands at his lapels, a bob */
        p.arm_l = -28; p.arm_r = 28;
        p.thigh_l = 6; p.thigh_r = -6; p.shin_l = odd ? -14 : 0; p.shin_r = odd ? 0 : 14;
        p.head = 9 * side; p.body = 2 * side;
        break;
    case M_SHUFFLE:                                /* knock-kneed, hopping from foot to foot */
        p.thigh_l = odd ? -16 : 6; p.shin_l = odd ? 46 : -10;
        p.thigh_r = odd ? -6 : 16; p.shin_r = odd ? 10 : -46;
        p.arm_l = -30; p.arm_r = 30;
        p.hop = odd ? 0.025 : 0.01; p.head = 11 * side; p.body = -5 * side;
        p.x = 0.035 * side;
        break;
    case M_KICKS:                                  /* high kicks, one leg then the other */
        p.thigh_l = in_bar == 0 ? 105 : 0; p.thigh_r = in_bar == 2 ? -105 : 0;
        p.shin_l = in_bar == 0 ? -15 : 0; p.shin_r = in_bar == 2 ? 15 : 0;
        p.arm_l = in_bar == 2 ? 120 : 40; p.arm_r = in_bar == 0 ? -120 : -40;
        p.hop = in_bar & 1 ? 0.04 : 0; p.body = in_bar == 0 ? -10 : in_bar == 2 ? 10 : 0;
        p.head = -p.body * 1.5;
        break;
    case M_CROUCH:                                 /* down low, knees out, a leg swept out */
        p.thigh_l = 75; p.shin_l = -150; p.thigh_r = -75; p.shin_r = 150;
        if (odd) { p.thigh_r = -88; p.shin_r = 0; }
        p.arm_l = 80; p.arm_r = -80; p.head = 12 * side; p.body = 5 * side;
        break;
    case M_HATWAVE:                                /* the hat waved overhead, kicking */
        p.arm_r = -165 + 20 * side; p.arm_l = 60;
        p.thigh_l = in_bar == 1 ? 95 : 0; p.thigh_r = in_bar == 3 ? -95 : 0;
        p.hop = in_bar & 1 ? 0.04 : 0; p.head = -10 * side; p.body = 6 * side;
        break;
    case M_HUNCH:                                  /* hunched over, a crouching step */
        p.body = 14; p.head = 16;
        p.thigh_l = odd ? 50 : 20; p.shin_l = odd ? -90 : -40;
        p.thigh_r = odd ? -20 : -50; p.shin_r = odd ? 40 : 90;
        p.arm_l = 50; p.arm_r = -20;
        break;
    case M_LOPE:                                   /* the kicking lope: knee up, foot flung out */
        p.thigh_l = odd ? 70 : -20; p.shin_l = odd ? -40 : 0;
        p.thigh_r = odd ? 20 : -70; p.shin_r = odd ? 0 : 40;
        p.arm_l = odd ? -30 : 70; p.arm_r = odd ? -70 : 30;
        p.body = 8; p.head = 6 * side; p.hop = 0.02;
        break;
    case M_SCISSOR:                                /* legs flung wide, one kicked high, arms up */
        p.thigh_l = odd ? 100 : 40; p.thigh_r = odd ? -40 : -100;
        p.arm_l = 150; p.arm_r = -150;
        p.hop = 0.05; p.body = 7 * side; p.head = -12 * side;
        break;
    case M_SWAY:                                   /* the hat held in front, swaying, grinning */
        p.arm_r = 30; p.arm_l = -20;
        p.body = 6 * side; p.head = 12 * side;
        p.thigh_l = 4 * side; p.thigh_r = 4 * side;
        break;
    }
    return p;
}

/* where he moves across the stage */
static double place_at(double t)
{
    double x = 0.5;
    if (t > SONG_PART2 && t < FILM(8, 6.4)) {            /* setting off for the wings, and back */
        double u = fmod(t - SONG_PART2, 16.0);
        double going = ease(ramp(u, 4, 7)) * (1 - ease(ramp(u, 10, 13)));
        x += 0.28 * going * ((int)((t - SONG_PART2) / 16.0) % 2 ? -1 : 1);
    }
    double a = FILM(8, 50.9), b = FILM(9, 0.8);         /* the lope: right, all the way left, back */
    if (t > a && t < b) {
        double u = ramp(t, a, b);
        x += u < 0.3 ? 0.3 * ease(u / 0.3) : u < 0.75 ? 0.3 - 0.6 * ease((u - 0.3) / 0.45) : -0.3 + 0.3 * ease((u - 0.75) / 0.25);
    }
    a = FILM(9, 11.2); b = FILM(9, 14.9);                /* off towards the wings and back */
    if (t > a && t < b) x += 0.32 * sin(M_PI * ramp(t, a, b));
    return x;
}

/* the leg loops, by song time: a leg right round its pin (0 left, 1 right), or both
 * legs swivelled round together (2) before they go round one at a time */
static const struct { double at; int leg; } LOOPS[] = {
    {FILM(8, 21.1), 2}, {FILM(8, 22.2), 0}, {FILM(8, 23.3), 1}, {FILM(8, 32.4), 1}, {FILM(8, 38.2), 0},
    {FILM(8, 53.4), 2}, {FILM(8, 54.6), 1}, {FILM(9, 3.9), 0}, {FILM(9, 12.8), 1},
};
static const double SPINS[] = {FILM(8, 19.5), FILM(8, 30.3), FILM(8, 44.3), FILM(9, 0.8)};
static const double TOSSES[][2] = {{FILM(8, 19.1), 1.5}, {FILM(9, 9.5), 1.4}};      /* when, how long up */
#define LOOP_TIME 0.9
#define SPIN_TIME 0.7

static Hat hat_at(double t, Pose *p)
{
    Hat h = {HAT_ON, 0, 0, 0};
    for (unsigned i = 0; i < sizeof TOSSES / sizeof *TOSSES; i++) {
        double u = ramp(t, TOSSES[i][0], TOSSES[i][0] + TOSSES[i][1]);
        if (u > 0 && u < 1) {                          /* up, round and round, and back down on */
            h.mode = HAT_FLYING;
            h.y = -0.62 * 4 * u * (1 - u); h.x = 0.04 * sin(u * 2 * M_PI); h.turn = 720 * u;
            p->arm_r = -150; p->arm_l = 150;
        }
    }
    int m = move_at(t);
    if (h.mode == HAT_ON && (m == M_HATWAVE || m == M_SWAY || (m == M_LOPE && t > FILM(9, 11.2)))) h.mode = HAT_HAND;
    if (h.mode == HAT_ON && m == M_MARK) {             /* the tip of the hat */
        double ph; int b = beat_at(t, &ph);
        if ((b / 4) % 8 == 6) h.mode = HAT_HAND;
    }
    if (t > END) h.mode = HAT_HAND;
    return h;
}

static Pose pose_at(double t, Hat *hat)
{
    double ph;
    int b = beat_at(t, &ph);
    Pose p = mix(key_pose(b - 1, t), key_pose(b, t), snap(ph));
    p.x += place_at(t);
    /* let down on the string at the start, with a bounce as he lands */
    p.drop = 1.4 * (1 - ease(ramp(t, 2.5, 5.5)));
    if (t > 5.5 && t < 7.5) p.hop += 0.09 * fabs(sin((t - 5.5) * 7)) * exp(-(t - 5.5) * 2.2);
    for (unsigned i = 0; i < sizeof LOOPS / sizeof *LOOPS; i++) {
        double u = ramp(t, LOOPS[i].at, LOOPS[i].at + LOOP_TIME);
        if (u <= 0 || u >= 1) continue;
        double turn = 360 * ease(u);
        if (LOOPS[i].leg == 0) { p.thigh_l = turn; p.shin_l = 25 * sin(M_PI * u); }
        else if (LOOPS[i].leg == 1) { p.thigh_r = -turn; p.shin_r = -25 * sin(M_PI * u); }
        else {                                     /* legs together, swivelled round as one */
            p.thigh_l = turn - 8; p.thigh_r = turn + 8; p.shin_l = p.shin_r = 0;
            p.arm_l = 150; p.arm_r = -150; p.body = 10 * sin(2 * M_PI * u);
            p.hang = fmin(1, 4 * sin(M_PI * u));
        }
        p.hop = fmax(p.hop, 0.06 * sin(M_PI * u));
    }
    for (unsigned i = 0; i < sizeof SPINS / sizeof *SPINS; i++) {
        double u = ramp(t, SPINS[i], SPINS[i] + SPIN_TIME);
        if (u <= 0 || u >= 1) continue;
        Pose still = p;                                 /* limbs drawn in while he turns */
        still.arm_l = 15; still.arm_r = -15; still.thigh_l = still.thigh_r = still.shin_l = still.shin_r = 0;
        still.body = still.head = 0;
        p = mix(p, still, sin(M_PI * u));
        p.spin = cos(2 * M_PI * u);
        p.hop = fmax(p.hop, 0.05 * sin(M_PI * u));
    }
    /* an arm windmill, twice round, now and then in the dance */
    {
        double u = ramp(t, FILM(8, 49.0), FILM(8, 50.5));
        if (u > 0 && u < 1) { p.arm_l = 720 * u + 20; p.arm_r = -720 * u - 20; }
        u = ramp(t, FILM(9, 5.7), FILM(9, 7.2));
        if (u > 0 && u < 1) { p.arm_l = 720 * u + 20; p.arm_r = -720 * u - 20; }
    }
    *hat = hat_at(t, &p);
    /* the bow at the end: a hop back, a deep nod, the hat swept wide */
    double bow = ease(ramp(t, END, END + 0.4)) * (1 - ease(ramp(t, END + 3.0, END + 3.4)));
    if (bow > 0) {
        Pose bw = p;
        bw.head = 30; bw.body = -8; bw.hop = 0; bw.arm_l = 110; bw.arm_r = -110;
        bw.thigh_l = bw.thigh_r = bw.shin_l = bw.shin_r = 0;
        p = mix(p, bw, bow);
    }
    return p;
}

/* ---- drawing ---- */
static double unit;
static int sw, sh;
static const Rgb RED = {150, 30, 34}, GOLD = {214, 168, 66}, BOARDS = {120, 82, 50};

/* a point of the doll at rest (in the pictures' pixels) turned by a */
static void turn(double x, double y, double a, double *ox, double *oy)
{
    double r = a * M_PI / 180, c = cos(r), s = sin(r);
    *ox = x * c - y * s; *oy = x * s + y * c;
}
/* One piece, its pin at (wx, wy), turned by angle. The whole doll is squeezed across
 * by `spin` about cx as it turns round, and drawn the other way round when spin < 0. */
static double spin_sx, spin_cx;
static void piece(int which, int idx, double wx, double wy, double angle, double k)
{
    flush(ren);
    if (!cur_tex || !cur_tex[which]) return;
    const Piece *pc = &PIECE[idx];
    double s = spin_sx, as = fabs(s) < 0.04 ? 0.04 : fabs(s);
    int back = s < 0;
    double w = sprite[which].w * k * as, h = sprite[which].h * k;
    double px = (back ? sprite[which].w - pc->pin_x : pc->pin_x) * k * as;
    wx = spin_cx + (wx - spin_cx) * s;
    SDL_FRect dst = {(float)(wx - px), (float)(wy - pc->pin_y * k), (float)w, (float)h};
    SDL_FPoint centre = {(float)px, (float)(pc->pin_y * k)};
    SDL_RenderCopyExF(ren, cur_tex[which], NULL, &dst, back ? -angle : angle, &centre, back ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
enum { P_HEAD, P_HAT, P_BODY, P_ARM_L, P_ARM_R, P_THIGH_L, P_THIGH_R, P_SHIN_L, P_SHIN_R };

/* The doll, standing on the stage floor at `floor`, `tall` high. Every piece hangs
 * from the one above it by its pin; the lower foot is put on the floor. */
static void doll(Pose p, Hat hat, double floor, double tall)
{
    const double HX = (PIECE[P_THIGH_L].at_x + PIECE[P_THIGH_R].at_x) / 2, HY = PIECE[P_THIGH_L].at_y;
    double k = tall / 916.0;
    double sx = p.x * sw;
    /* the rest positions, as offsets from between the hips */
    #define REL(i) (PIECE[i].at_x - HX), (PIECE[i].at_y - HY)
    double bx, by, nx, ny, hlx, hly, hrx, hry, alx, aly, arx, ary;
    turn(REL(P_BODY), p.body, &bx, &by);
    turn(REL(P_ARM_L), p.body, &alx, &aly);
    turn(REL(P_ARM_R), p.body, &arx, &ary);
    turn(REL(P_HEAD), p.body, &nx, &ny);
    turn(REL(P_THIGH_L), p.body, &hlx, &hly);
    turn(REL(P_THIGH_R), p.body, &hrx, &hry);
    double klx, kly, krx, kry, flx, fly, frx, fry;
    turn(PIECE[P_SHIN_L].at_x - PIECE[P_THIGH_L].at_x, PIECE[P_SHIN_L].at_y - PIECE[P_THIGH_L].at_y, p.body + p.thigh_l, &klx, &kly);
    turn(PIECE[P_SHIN_R].at_x - PIECE[P_THIGH_R].at_x, PIECE[P_SHIN_R].at_y - PIECE[P_THIGH_R].at_y, p.body + p.thigh_r, &krx, &kry);
    double foot_l = sprite[SP_shin_l].h - PIECE[P_SHIN_L].pin_y - 8, foot_r = sprite[SP_shin_r].h - PIECE[P_SHIN_R].pin_y - 8;
    turn(0, foot_l, p.body + p.thigh_l + p.shin_l, &flx, &fly);
    turn(0, foot_r, p.body + p.thigh_r + p.shin_r, &frx, &fry);
    klx += hlx; kly += hly; krx += hrx; kry += hry;
    flx += klx; fly += kly; frx += krx; fry += kry;
    /* his lowest foot is on the floor; held up on his string, his hips stay where they
     * are when he stands */
    double rest = PIECE[P_SHIN_L].at_y - HY + foot_l;
    double lowest = fmax(fly, fry) + (rest - fmax(fly, fry)) * p.hang;
    double ox = sx, oy = floor - lowest * k - (p.hop + p.drop) * tall;
    /* the hat: on his head, in his right hand, or flying */
    double hx, hy, ha;
    {
        double dx, dy;
        turn(PIECE[P_HAT].at_x - PIECE[P_HEAD].at_x, PIECE[P_HAT].at_y - PIECE[P_HEAD].at_y, p.body + p.head, &dx, &dy);
        hx = nx + dx; hy = ny + dy; ha = p.body + p.head;
        if (hat.mode == HAT_FLYING) { hx += hat.x * 916; hy += hat.y * 916; ha += hat.turn; }
        if (hat.mode == HAT_HAND) {                    /* at the end of his right sleeve */
            double tx, ty, len = sprite[SP_arm_r].h - PIECE[P_ARM_R].pin_y;
            turn(sprite[SP_arm_r].w / 2.0 - PIECE[P_ARM_R].pin_x, len, p.body + p.arm_r, &tx, &ty);
            hx = arx + tx; hy = ary + ty; ha = p.body + p.arm_r + 180;
        }
    }
    #undef REL
    spin_sx = p.spin; spin_cx = ox;
    /* his string, while he is let down on to the stage */
    if (T < 7.5 || p.hang > 0.01) {
        double a = T < 7.5 ? 1 - ramp(T, 6.0, 7.5) : p.hang;
        line(ox + nx * k * p.spin, 0, ox + nx * k * p.spin, oy + (ny - 150) * k, unit * 0.12, col((Rgb){230, 225, 210}, 0.9 * a));
        flush(ren);
    }
    /* his shadow on the boards */
    {
        int n = 30;
        double r = tall * 0.17 * (1 - fmin(0.8, (p.hop + p.drop) * 4));
        vb_need(n + 1, 3 * n);
        int b0 = nv;
        vert(ox, floor, col(INK, 0.4));
        for (int i = 0; i < n; i++) vert(ox + r * cos(i * 2 * M_PI / n), floor + r * 0.16 * sin(i * 2 * M_PI / n), col(INK, 0));
        for (int i = 0; i < n; i++) { ib[ni++] = b0; ib[ni++] = b0 + 1 + i; ib[ni++] = b0 + 1 + (i + 1) % n; }
        flush(ren);
    }
    /* back to front: legs, body, head, arms, the hat; then the pins over all */
    piece(SP_shin_l, P_SHIN_L, ox + klx * k, oy + kly * k, p.body + p.thigh_l + p.shin_l, k);
    piece(SP_shin_r, P_SHIN_R, ox + krx * k, oy + kry * k, p.body + p.thigh_r + p.shin_r, k);
    piece(SP_thigh_l, P_THIGH_L, ox + hlx * k, oy + hly * k, p.body + p.thigh_l, k);
    piece(SP_thigh_r, P_THIGH_R, ox + hrx * k, oy + hry * k, p.body + p.thigh_r, k);
    piece(SP_body, P_BODY, ox + bx * k, oy + by * k, p.body, k);
    piece(SP_head, P_HEAD, ox + nx * k, oy + ny * k, p.body + p.head, k);
    piece(SP_arm_l, P_ARM_L, ox + alx * k, oy + aly * k, p.body + p.arm_l, k);
    piece(SP_arm_r, P_ARM_R, ox + arx * k, oy + ary * k, p.body + p.arm_r, k);
    piece(SP_hat, P_HAT, ox + hx * k, oy + hy * k, ha, k);
    double pins[][2] = {{nx, ny}, {alx, aly}, {arx, ary}, {hlx, hly}, {hrx, hry}, {klx, kly}, {krx, kry}};
    for (int i = 0; i < 7; i++) {
        double r = 7 * k * 1.4 * fabs(p.spin);
        SDL_FRect d = {(float)(ox + pins[i][0] * k * p.spin - r), (float)(oy + pins[i][1] * k - 7 * k * 1.4), (float)(2 * r), (float)(2 * 7 * k * 1.4)};
        if (cur_tex && cur_tex[SP_pin]) SDL_RenderCopyF(ren, cur_tex[SP_pin], NULL, &d);
    }
}

/* the toy theatre: Pollock's painted backdrop (Scene 4 of "Jack the Giant Killer",
 * a sunrise over mountains and a river) and a floor of boards in front of it */
static double side_w, top_h;                 /* the proscenium's sides and top */
static void stage_back(int w, int h, double floor)
{
    side_w = w * 0.055; top_h = h * 0.12;
    double ow = w - 2 * side_w, oh = floor - top_h;
    /* the backdrop fills the opening above the boards, cut to fit: the band with the
     * sunrise, the mountains and the far shore, starting a tenth of the way down it */
    double s = fmax(ow / sprite[SP_backdrop].w, oh / sprite[SP_backdrop].h);
    double bw = sprite[SP_backdrop].w * s, bh = sprite[SP_backdrop].h * s;
    double from = fmin(0.10 * bh, bh - oh);
    pic_part(SP_backdrop, side_w + (ow - bw) / 2, top_h - from, bw, bh, 0, 1, 0, 0);
    rect(0, floor, w, h - floor, col(BOARDS, 1));
    for (int i = 0; i < 14; i++) line(0, floor + (h - floor) * (i / 14.0), w, floor + (h - floor) * (i / 14.0), unit * 0.08, col(INK, 0.35));
    flush(ren);
}
static void stage_front(int w, int h, double open)
{
    /* the curtains, Pollock's drapes (from Scene 11): closed, each covers half the
     * opening; drawn open, each is gathered up at its side */
    for (int sd = 0; sd < 2; sd++) {
        int which = sd ? SP_drape_r : SP_drape_l;
        double ch = h - top_h * 0.6, natural = 0.42 * ch * sprite[which].w / sprite[which].h;
        double cw = natural + (w / 2.0 - side_w + unit * 2 - natural) * (1 - open);
        double x0 = sd ? w - side_w - cw : side_w;
        pic_part(which, x0, top_h * 0.6, cw, ch, 0, 1, 0, 0);
    }
    flush(ren);
    /* the proscenium */
    double t = top_h, side = side_w;
    rect(0, 0, w, t, col(RED, 1));
    rect(0, 0, side, h, col(RED, 1));
    rect(w - side, 0, side, h, col(RED, 1));
    line(side, t, w - side, t, unit * 0.6, col(GOLD, 1));
    line(side, t, side, h, unit * 0.6, col(GOLD, 1));
    line(w - side, t, w - side, h, unit * 0.6, col(GOLD, 1));
    for (int i = 0; i < 2; i++) disc(i ? w - side / 2 : side / 2, t * 0.5, t * 0.32, col(GOLD, 1), col(GOLD, 1));
    flush(ren);
    const char *title = "HOORAY FOR CAPTAIN SPAULDING";
    double s = fmin(t * 0.075, (w - 4 * side) / (text_width(title, 1) + 1));
    text(title, w / 2.0, t * 0.5 - s * 3, s, 1.0, col(GOLD, 1));
    flush(ren);
    /* footlights */
    for (int i = 0; i < 12; i++) {
        double x = side + (w - 2 * side) * (i + 0.5) / 12;
        disc(x, h - unit * 0.8, unit * 3.5, col((Rgb){255, 230, 160}, 0.35), col((Rgb){255, 230, 160}, 0));
        disc(x, h - unit * 0.4, unit * 0.9, col((Rgb){255, 240, 200}, 1), col((Rgb){240, 200, 120}, 1));
    }
    flush(ren);
}

static void draw_scene(SDL_Renderer *r, int w, int h)
{
    ren = r; sw = w; sh = h;
    unit = (w < h ? w : h) * 0.0125;
    SDL_SetRenderDrawColor(r, BG.r, BG.g, BG.b, 255);
    SDL_RenderClear(r);
    double floor = h * 0.86;
    stage_back(w, h, h * 0.71);
    Hat hat;
    Pose p = pose_at(T, &hat);
    doll(p, hat, floor, h * 0.62);
    double open = ease(ramp(T, 0.3, 3.0)) * (1 - ease(ramp(T, END + 2.8, END + 4.6)));
    stage_front(w, h, open);
    double dark = fmax(1 - ramp(T, 0, 0.5), ramp(T, SONG_LENGTH + GAP - 1, SONG_LENGTH + GAP));
    if (dark > 0) { rect(0, 0, w, h, col(BG, dark)); flush(r); }
}

typedef struct { SDL_Window *win; SDL_Renderer *ren; SDL_Texture *scene, *tex[NSPRITE]; int w, h; } View;

static void size_view(View *v)
{
    int ow, oh;
    SDL_RendererInfo info;
    SDL_GetRendererOutputSize(v->ren, &ow, &oh);
    if (ow < 1 || oh < 1) return;
    int ss = 2; /* drawn at twice the size and scaled down, for smooth edges */
    if (SDL_GetRendererInfo(v->ren, &info) == 0) {
        if (!(info.flags & SDL_RENDERER_TARGETTEXTURE) || (info.flags & SDL_RENDERER_SOFTWARE)) ss = 0;
        else if (info.max_texture_width && (ow * 2 > info.max_texture_width || oh * 2 > info.max_texture_height)) ss = 0;
    }
    if (v->w == ow * (ss ? ss : 1) && v->h == oh * (ss ? ss : 1) && (v->scene || !ss)) return;
    if (v->scene) SDL_DestroyTexture(v->scene);
    v->scene = NULL; v->w = ow; v->h = oh;
    if (!ss) return;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    v->scene = SDL_CreateTexture(v->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ow * ss, oh * ss);
    if (v->scene) { v->w = ow * ss; v->h = oh * ss; }
}
static void render(View *v)
{
    if (v->scene) SDL_SetRenderTarget(v->ren, v->scene);
    cur_tex = v->tex;
    draw_scene(v->ren, v->w, v->h);
    if (v->scene) { SDL_SetRenderTarget(v->ren, NULL); SDL_RenderCopy(v->ren, v->scene, NULL, NULL); }
    SDL_RenderPresent(v->ren);
}
static int make_view(View *v, int display)
{
    Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;
    int x = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    if (opt_window) flags |= SDL_WINDOW_RESIZABLE;
    else flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
    memset(v, 0, sizeof *v);
    v->win = SDL_CreateWindow("Hooray for Captain Spaulding", x, x, 1200, 750, flags);
    if (!v->win) return 0;
    v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!v->ren) v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_SOFTWARE);
    if (!v->ren) return 0;
    SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);
    make_textures(v->ren, v->tex);
    return 1;
}

static void start_show(void)
{
    seed_state = opt_seed ? opt_seed * 0x9E3779B97F4A7C15ULL : ((uint64_t)time(NULL) << 20) ^ SDL_GetPerformanceCounter();
    if (!seed_state) seed_state = 1;
    for (int i = 0; i < 6; i++) next_rand();
    anim_t = 0; T = 0;
    start_song();
}

static int snapshot(void)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, opt_w, opt_h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!r) { fprintf(stderr, "captain-spaulding: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Texture *tex[NSPRITE];
    make_textures(r, tex);
    cur_tex = tex;
    start_show();
    for (double t = 0; t < opt_time; t += 1.0 / 60) update(1.0 / 60);
    draw_scene(r, opt_w, opt_h);
    SDL_RenderPresent(r);
    if (SDL_SaveBMP(surf, opt_snapshot) != 0) { fprintf(stderr, "captain-spaulding: %s\n", SDL_GetError()); return 1; }
    return 0;
}

static void usage(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Hooray for Captain Spaulding",
        "Hooray for Captain Spaulding screensaver\n\n"
        "  --window        run in a window (Esc or Q quits)\n"
        "  --mute          no music\n"
        "  --speed X       animation speed, 1 = normal\n"
        "  --seed N        repeatable sequence\n"
        "  --snapshot FILE.bmp [--time SECONDS] [--size WxH]\n"
        "                  write a picture of the screensaver after SECONDS and exit\n", NULL);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 's' || a[1] == 'S') && !a[2]) continue;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'p' || a[1] == 'P') && (!a[2] || a[2] == ':')) return 0;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'c' || a[1] == 'C') && (!a[2] || a[2] == ':')) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Hooray for Captain Spaulding", "Hooray for Captain Spaulding screensaver has no settings.", NULL);
            return 0;
        }
        else if (!strcmp(a, "--window")) opt_window = 1;
        else if (!strcmp(a, "--mute")) opt_mute = 1;
        else if (!strcmp(a, "--speed") && i + 1 < argc) opt_speed = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--seed") && i + 1 < argc) opt_seed = SDL_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--snapshot") && i + 1 < argc) opt_snapshot = argv[++i];
        else if (!strcmp(a, "--time") && i + 1 < argc) opt_time = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--size") && i + 1 < argc) {
            const char *x = SDL_strchr(argv[++i], 'x');
            opt_w = SDL_atoi(argv[i]); opt_h = x ? SDL_atoi(x + 1) : 0;
            if (opt_w < 64 || opt_h < 64) { usage(); return 1; }
        }
        else { usage(); return 1; }
    }
    if (opt_speed < 0.05) opt_speed = 0.05;
    load_sprites();
    load_song();

    if (opt_snapshot) return snapshot();
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "captain-spaulding: %s\n", SDL_GetError()); return 1; }

    View views[8];
    int nviews = 0, ndisp = opt_window ? 1 : SDL_GetNumVideoDisplays();
    if (ndisp < 1) ndisp = 1;
    for (int d = 0; d < ndisp && nviews < 8; d++)
        if (make_view(&views[nviews], d)) nviews++;
    if (!nviews) { fprintf(stderr, "captain-spaulding: %s\n", SDL_GetError()); return 1; }
    if (!opt_window) SDL_ShowCursor(SDL_DISABLE);
    start_show();

    Uint64 freqc = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter(), start = last;
    double moved = 0;
    int running = 1;
    while (running) {
        SDL_Event e;
        double since = (double)(SDL_GetPerformanceCounter() - start) / freqc;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                if (!opt_window || e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q) running = 0;
            } else if (!opt_window && since > 1.0) {
                if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERDOWN) running = 0;
                else if (e.type == SDL_MOUSEMOTION) {
                    moved += abs(e.motion.xrel) + abs(e.motion.yrel);
                    if (moved > 24) running = 0;
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / freqc;
        last = now;
        if (dt > 0.1) dt = 0.1;
        update(dt);
        for (int i = 0; i < nviews; i++) { size_view(&views[i]); render(&views[i]); }
        SDL_Delay(views[0].scene ? 1 : 12);
    }
    SDL_Quit();
    return 0;
}
