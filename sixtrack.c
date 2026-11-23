/* Six Track: one-button cricket batting for the Shanling Q2 running Q2 Pod, as a card game.
 *
 * You bat in a floodlit stadium, seen from behind the batter. Pick a match (1, 2 or 5 overs, 10
 * wickets) and chase the target the game sets. Centre, or a tap anywhere, swings the bat; when you
 * swing decides everything. A strip on the pitch shows it: very early to fine leg, early to
 * midwicket, on time straight, late to cover, very late to third man, and the closer to a band's
 * bright centre line, the further it goes (6, 4, 2, 1). Red ends are edges; no swing, bowled.
 * Return pauses (held, Q2 Pod closes the game). Play/Pause and the side buttons stay the music's.
 *
 * One file, no libraries: raw MIPS o32 syscalls, a software renderer on /dev/fb0 (320 x 375, 32 bpp,
 * two pages; the 375 x 320 landscape view stored turned clockwise), input as text datagrams on
 * INPUT_SOCK: "k d CODE" / "k u CODE", "t d X Y" (touch, landscape pixels), "w N" (wheel travel).
 * The device layer (syscalls, framebuffer, input socket) follows Buddy (DiamondBond/q2-fidget, MIT).
 * Built with -DHOST it plays a scripted session and writes BMP frames instead (preview.sh). */

#define W 375
#define H 320
#define INPUT_SOCK "/tmp/q2-sixtrack.input.sock"
#define KEY_RETURN 170
#define KEY_CENTER 218

typedef unsigned u32;
#define RGB(r, g, b) (0xff000000u | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))

/* ---- maths: no libm ---- */

#define PI 3.14159265f
static float absf(float x) { return x < 0 ? -x : x; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float sat(float x) { return clampf(x, 0, 1); }
static float mixf(float a, float b, float t) { return a + (b - a) * t; }
static float fsqrt(float x) { return x > 0 ? __builtin_sqrtf(x) : 0; }
static float fsin(float x) {
    float k = x * (1 / (2 * PI));
    x -= 2 * PI * (float)(int)(k < 0 ? k - 0.5f : k + 0.5f); /* to about -pi..pi */
    if (x > PI) x -= 2 * PI;
    if (x < -PI) x += 2 * PI;
    float y = 1.2732395f * x - 0.4052847f * x * absf(x);
    return 0.225f * (y * absf(y) - y) + y;
}
static float fcos(float x) { return fsin(x + PI / 2); }
static u32 rng = 2463534242u;
static u32 rnd(void) {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() & 0xffffff) * (1.0f / 16777216.0f); }

/* ---- the camera: behind the batter, high, looking down the pitch ----
 * A point X metres to the right, Y up, d metres down the pitch from the batting crease is drawn at
 * x = 187.5 + X * F / (d + C), y = HOR + (CAM_H - Y) * F / (d + C). */

#define CAM_F 200.0f
#define CAM_C 6.74f
#define HOR 76.1f
#define CAM_H 6.532f
#define CX 187.5f
static float scale_at(float d) { return CAM_F / maxf(d + CAM_C, 0.5f); }
static void proj(float X, float Y, float d, float *sx, float *sy) {
    float s = scale_at(d);
    *sx = CX + X * s, *sy = HOR + (CAM_H - Y) * s;
}
/* the ground's d under screen row y (the inverse of proj with Y 0) */
static float ground_d(float y) { return y > HOR + 0.5f ? CAM_F * CAM_H / (y - HOR) - CAM_C : 1e6f; }

/* ---- drawing on T, a W x H canvas ---- */

static u32 bg[W * H], cv[W * H];
static u32 *T = cv;

static u32 mixp(u32 d, u32 s, u32 a) { /* a 0..256 */
    u32 rb = (((d & 0xff00ffu) * (256 - a) + (s & 0xff00ffu) * a) >> 8) & 0xff00ffu;
    u32 g = (((d & 0xff00u) * (256 - a) + (s & 0xff00u) * a) >> 8) & 0xff00u;
    return 0xff000000u | rb | g;
}
static void plot(int x, int y, u32 c, float a) {
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0.004f) return;
    u32 *p = &T[y * W + x];
    *p = a >= 1 ? c : mixp(*p, c, (u32)(a * 256));
}
static u32 shade(u32 c, float k) { /* k < 1 darker, > 1 lighter */
    float r = (float)(c >> 16 & 255) * k, g = (float)(c >> 8 & 255) * k, b = (float)(c & 255) * k;
    return RGB((u32)clampf(r, 0, 255), (u32)clampf(g, 0, 255), (u32)clampf(b, 0, 255));
}
static void add_light(int x, int y, u32 c, float a) { /* additive glow */
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0.004f) return;
    u32 p = T[y * W + x];
    float r = (float)(p >> 16 & 255) + (float)(c >> 16 & 255) * a, g = (float)(p >> 8 & 255) + (float)(c >> 8 & 255) * a,
          b = (float)(p & 255) + (float)(c & 255) * a;
    T[y * W + x] = RGB((u32)minf(r, 255), (u32)minf(g, 255), (u32)minf(b, 255));
}
static void rect(int x0, int y0, int x1, int y1, u32 c, float a) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) plot(x, y, c, a);
}
static void bounds(float x0, float y0, float x1, float y1, int *a, int *b, int *c, int *d) {
    *a = (int)maxf(x0, 0), *b = (int)maxf(y0, 0), *c = (int)minf(x1 + 1, W), *d = (int)minf(y1 + 1, H);
}
static void disc(float cx, float cy, float r, u32 c, float a) {
    int x0, y0, x1, y1;
    bounds(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, &x0, &y0, &x1, &y1);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            plot(x, y, c, a * sat(r + 0.5f - fsqrt(dx * dx + dy * dy)));
        }
}
static void glow(float cx, float cy, float r, u32 c, float a) {
    int x0, y0, x1, y1;
    bounds(cx - r, cy - r, cx + r, cy + r, &x0, &y0, &x1, &y1);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, k = 1 - (dx * dx + dy * dy) / (r * r);
            if (k > 0) add_light(x, y, c, a * k * k);
        }
}
static void ellipse(float cx, float cy, float rx, float ry, u32 c, float a) {
    int x0, y0, x1, y1;
    bounds(cx - rx - 1, cy - ry - 1, cx + rx + 1, cy + ry + 1, &x0, &y0, &x1, &y1);
    float m = minf(rx, ry);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            float dx = ((float)x + 0.5f - cx) / rx, dy = ((float)y + 0.5f - cy) / ry;
            plot(x, y, c, a * sat((1 - fsqrt(dx * dx + dy * dy)) * m + 0.5f));
        }
}
static void capsule(float x0, float y0, float x1, float y1, float r, u32 c, float a) {
    int bx0, by0, bx1, by1;
    bounds(minf(x0, x1) - r - 1, minf(y0, y1) - r - 1, maxf(x0, x1) + r + 1, maxf(y0, y1) + r + 1, &bx0, &by0, &bx1, &by1);
    float vx = x1 - x0, vy = y1 - y0, vv = vx * vx + vy * vy;
    for (int y = by0; y < by1; ++y)
        for (int x = bx0; x < bx1; ++x) {
            float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0;
            float t = vv > 0 ? sat((px * vx + py * vy) / vv) : 0, dx = px - vx * t, dy = py - vy * t;
            plot(x, y, c, a * sat(r + 0.5f - fsqrt(dx * dx + dy * dy)));
        }
}
static void rrect(int x, int y, int w, int h, float r, u32 c, float a) {
    int bx0, by0, bx1, by1;
    bounds((float)x, (float)y, (float)(x + w - 1), (float)(y + h - 1), &bx0, &by0, &bx1, &by1);
    for (int py = by0; py < by1; ++py)
        for (int px = bx0; px < bx1; ++px) {
            float qx = maxf(maxf((float)(x + r) - ((float)px + 0.5f), ((float)px + 0.5f) - (float)(x + w - r)), 0),
                  qy = maxf(maxf((float)(y + r) - ((float)py + 0.5f), ((float)py + 0.5f) - (float)(y + h - r)), 0);
            plot(px, py, c, a * sat(r + 0.5f - fsqrt(qx * qx + qy * qy)));
        }
}

/* ---- text: Liberation Sans Bold 20 px (font.h, SIL OFL 1.1), drawn at any size ---- */

#include "font.h"
static float text_w(const char *s, float k) {
    int w = 0;
    for (; *s; ++s)
        if (*s >= 32 && *s < 127) w += font_adv[*s - 32];
    return (float)w * k;
}
static float glyph_at(int g, int w, float gx, float gy) { /* bilinear alpha, 0..1 */
    int x0 = (int)gx, y0 = (int)gy;
    float fx = gx - (float)x0, fy = gy - (float)y0, v = 0;
    const unsigned char *p = font_px + font_off[g];
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            int x = x0 + i, y = y0 + j;
            if (x < 0 || y < 0 || x >= w || y >= FONT_H) continue;
            v += (float)p[y * w + x] * (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
        }
    return v * (1 / 255.0f);
}
static void text(float x, float y, const char *s, float k, u32 c, float a) {
    for (; *s; ++s) {
        if (*s < 32 || *s > 126) continue;
        int g = *s - 32, w = font_adv[g];
        if (k == 1) {
            const unsigned char *p = font_px + font_off[g];
            for (int r = 0; r < FONT_H; ++r)
                for (int i = 0; i < w; ++i)
                    if (p[r * w + i]) plot((int)x + i, (int)y + r, c, a * (float)p[r * w + i] * (1 / 255.0f));
        } else {
            int x1 = (int)(x + (float)w * k), y1 = (int)(y + FONT_H * k);
            for (int py = (int)y; py <= y1; ++py)
                for (int px = (int)x; px <= x1; ++px)
                    plot(px, py, c, a * glyph_at(g, w, ((float)px + 0.5f - x) / k - 0.5f, ((float)py + 0.5f - y) / k - 0.5f));
        }
        x += (float)w * k;
    }
}
static void text_c(float cx, float y, const char *s, float k, u32 c, float a) { text(cx - text_w(s, k) / 2, y, s, k, c, a); }
static void text_shadow(float cx, float y, const char *s, float k, u32 c, float a) {
    text_c(cx + k, y + k, s, k, RGB(0, 0, 0), a * 0.6f);
    text_c(cx, y, s, k, c, a);
}
static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}
static char *puti(char *p, int v) {
    char b[12];
    int n = 0;
    if (v < 0) *p++ = '-', v = -v;
    do b[n++] = (char)('0' + v % 10), v /= 10;
    while (v);
    while (n) *p++ = b[--n];
    *p = 0;
    return p;
}

/* ---- the game ---- */

#define WICKETS 10
#define STUMPS_D (-1.2f)
#define RELEASE_D 20.0f
#define BAT_X 0.62f
#define BAT_HIT 0.06f      /* across, where the bat meets the ball: in line with the stumps */
#define STRIP_X (-0.3f) /* the strip's middle: left of the batter, under the ball's line */
/* The strip, early to late: a red end, five bands of BAND_W, a red end. */
#ifndef SHOW_STRIP
#define SHOW_STRIP 0
#endif
#define STRIP_FAR 8.0f
#define RED_W 0.8f
#define BAND_W 1.4f
#define BAND0 (STRIP_FAR - RED_W) /* the first band's far edge */
#define STRIP_NEAR (BAND0 - 5 * BAND_W - RED_W)
#define D_IDEAL (BAND0 - 2.5f * BAND_W) /* where the ball is seen at the perfect moment */

static const char *const band_name[5] = { "fine leg", "midwicket", "straight", "cover", "third man" };
static const u32 band_col[5] = { RGB(176, 96, 246), RGB(64, 150, 255), RGB(64, 210, 100), RGB(252, 208, 52),
                                 RGB(255, 140, 50) };
static const u32 RED = RGB(236, 64, 64);

enum { S_MENU, S_RUNUP, S_BALL, S_HIT, S_END, S_PAUSE, S_COUNT };
enum { H_NONE, H_RUNS, H_BOWLED, H_BEHIND, H_DEEP, H_WIDE };

typedef struct {
    float hx, hd, x, d; /* home and now, metres */
    float tx, td;       /* where it is running to */
    int caught;
} fielder;
#define FIELDERS 9
#define RUN_SPEED 6.5f /* a fielder's sprint, game metres a second, before their own pace */

static const int over_opts[3] = { 1, 2, 5 };

static struct {
    int state, paused_from, sel, quit;
    float t;       /* seconds into this state */
    float clock;   /* seconds since start, for idle motion */
    int overs, target, runs, wkts, balls;
    /* this ball */
    float v, bounce_d, six_half;
    int spin, kmh, swung, shown;
    float runup;
    /* the ball's flight: released at (X0, Y0), v down the pitch, vx across, vy0 up; ax its swing;
     * bouncing at time tb, rising at vyb after, turned across at turn; G the game's gravity */
    float X0, Y0, vx, vy0, ax, tb, vyb, turn, G, d_ideal;
    float swing_t; /* when the bat swung, in state time */
    float swing_age, ball_age; /* seconds since the swing, and since this ball was bowled */
    int how, got, band, edge_side;
    float ang, R, ymax, dur;
    float bx, by, bd; /* the ball now: X, Y, d */
    int catcher;
    int won;
    char banner[48];
    float banner_t;
    u32 banner_col;
    float bail_t;
    fielder f[FIELDERS];
    int bg_over; /* the over the background's strip was drawn for */
    struct { int num, left; } lineup[WICKETS + 1]; /* the batting order: shirt numbers, left-handers */
    /* the innings: each batter's card, who is on strike, who is next, the partnerships */
    struct { int runs, balls, fours, sixes, out; } card[WICKETS + 1]; /* out: 0 not out, else a H_ code */
    int striker, other, next_in, page;
    int stand, best_stand, best_a, best_b, extras;
    int wide; /* this ball would be called wide if not hit */
    /* the bowling: the attack's level for the match, and this over's bowler */
    float attack, skill;
    /* the field: each fielder's place for this over (degrees off straight for a right-hander, metres
     * out), their pace and their hands, for the match */
    float fpos[FIELDERS][2], fpace[FIELDERS], fhands[FIELDERS];
    int dropped, saved;
    int btype, bnum; /* 0 fast, 1 medium, 2 spin; the shirt number */
    float wheel;
} g;

static float deg(float a) { return a * (PI / 180); }

/* The batter in: +1 a right-hander, -1 a left-hander, whose stance, shots and field are mirrored */
static int batter_in(void) { return g.striker; }
static float hand(void) { return g.lineup[batter_in()].left ? -1.0f : 1.0f; }

/* Eleven batters, shuffled: numbers 1 to 99, none twice; about a third left-handed */
static void pick_lineup(void) {
    for (int i = 0; i <= WICKETS; ++i) {
        int n, dup;
        do {
            n = 1 + (int)(rnd() % 99), dup = 0;
            for (int j = 0; j < i; ++j) dup |= g.lineup[j].num == n;
        } while (dup);
        g.lineup[i].num = n, g.lineup[i].left = frand() < 0.33f;
    }
}

/* The green band's six window narrows each over; from over 3 slower balls and other lengths mix in. */
static int over_now(void) { return g.balls / 6; }
static float six_half_for(int over) { return maxf(0.30f - 0.035f * (float)over, 0.14f); }

/* The captain's field for an over, within the playing conditions: in the powerplay (the first 30% of
 * the overs) at most 2 fielders outside the 30-yard circle, after it at most 5; never more than 2
 * behind square on the leg side; none straight behind (the keeper's). The rest in the ring, spread
 * round the ground, none on top of another. Angles are for a right-hander, + to the leg side. */
static void set_field(void) {
    int powerplay = g.balls < (int)((float)(g.overs * 6) * 0.3f + 0.999f);
    int deep = powerplay ? 1 + (int)(rnd() % 2) : 3 + (int)(rnd() % 3), behind_leg = 0;
    for (int i = 0; i < FIELDERS; ++i) {
        float a, r = i < deep ? 46 + frand() * 12 : 20 + frand() * 8;
        int ok, tries = 0;
        do {
            a = -165 + frand() * 330, ok = 1;
            if (a > 90 && behind_leg >= 2) ok = 0; /* the leg-side limit */
            for (int j = 0; j < i; ++j) {
                float da = absf(a - g.fpos[j][0]), dr = absf(r - g.fpos[j][1]);
                if (da < (dr < 15 ? 26 : 10)) ok = 0;
            }
        } while (!ok && ++tries < 60);
        if (a > 90 && behind_leg >= 2) a = 180 - a; /* still over the limit: to in front of square */
        behind_leg += a > 90;
        g.fpos[i][0] = a, g.fpos[i][1] = r;
    }
}

static void place_fielders(void) {
    for (int i = 0; i < FIELDERS; ++i) {
        float a = deg(g.fpos[i][0] * hand()), r = g.fpos[i][1]; /* the field mirrored for a left-hander */
        g.f[i].hx = g.f[i].x = g.f[i].tx = -fsin(a) * r;
        g.f[i].hd = g.f[i].d = g.f[i].td = fcos(a) * r;
        g.f[i].caught = 0;
    }
}

static void make_bg(int over);
static void banner(const char *s, u32 c);
static void delivery_at(float t, float *X, float *Y, float *d);

/* Every ball its own: pace, length, line, swing and movement off the pitch drawn at random, then
 * flown as a projectile that bounces, so it can only be timed by watching it. The game runs in slow
 * motion (a fast ball takes about 2 s), so gravity is scaled by the same factor squared. */
#define GAME_G 0.62f
#define SLOW_MO 3.7f /* real pace to the game's: 150 km/h is 11.3 m/s here */

static float spread(void) { return frand() + frand() - 1; } /* -1..1, most near 0 */

static void pick_delivery(void) {
    float p = (float)g.balls / (float)(g.overs * 6); /* how far through the match */
    g.spin = g.btype == 2;
    (void)p;
    float v, bounce_d, e, Xt = spread() * 0.2f; /* Xt: where it would cross the stumps' line */
    float wild = frand(); /* now and then a wayward one: well wide of off, or down the leg side */
    if (wild < 0.05f) Xt = (0.75f + frand() * 0.5f) * hand();
    else if (wild < 0.08f) Xt = -(0.5f + frand() * 0.35f) * hand();
    g.X0 = 0.3f * hand() + spread() * 0.15f, g.Y0 = 2.1f + spread() * 0.12f, g.G = GAME_G;
    /* The speed, from real cricket's range (about 70 km/h slow spin to 161 km/h, the fastest recorded),
     * by the bowler's type, a sharper bowler a little quicker; the game's pace is that in slow motion. */
    float kmh;
    if (g.spin) kmh = 70 + frand() * 22 + (frand() < 0.12f ? 6 : 0);              /* spin, an arm ball quicker */
    else if (g.btype == 1) kmh = 110 + frand() * 20;                               /* medium pace */
    else kmh = 135 + frand() * 18 + (frand() < 0.12f ? frand() * 8 : 0);          /* fast, sometimes express */
    if (!g.spin && frand() < 0.15f) kmh = 95 + frand() * 23;                       /* a slower ball */
    kmh = minf(kmh * (0.97f + 0.03f * g.skill), 161);
    g.kmh = (int)kmh;
    if (g.spin) {
        v = kmh / 3.6f / SLOW_MO;
        bounce_d = 3 + frand() * 5;
        g.ax = spread() * 0.06f, g.turn = spread() * 0.55f * g.skill, e = 0.5f + frand() * 0.12f;
        g.runup = 0.75f;
    } else {
        v = kmh / 3.6f / SLOW_MO;
        bounce_d = 6 + spread() * 8; /* -2 (a full toss) to 14 (a bouncer), mostly a good length */
        g.ax = spread() * (g.btype == 1 ? 0.42f : 0.3f) * g.skill, g.turn = spread() * 0.12f * g.skill,
        e = 0.36f + frand() * 0.12f;
        g.runup = 1.0f + frand() * 0.3f;
    }
    g.v = v;
    float d_ball = RELEASE_D - bounce_d;
    g.tb = d_ball / v;
    g.vy0 = (0.5f * g.G * g.tb * g.tb - g.Y0) / g.tb; /* to land at bounce_d */
    g.vyb = -(g.vy0 - g.G * g.tb) * e;
    /* aim across so swing and turn bring it to Xt at the stumps */
    float ts = (RELEASE_D - STUMPS_D) / v;
    if (ts <= g.tb) g.vx = (Xt - g.X0 - 0.5f * g.ax * ts * ts) / ts;
    else g.vx = (Xt - g.X0 - 0.5f * g.ax * g.tb * g.tb - (g.ax * g.tb + g.turn) * (ts - g.tb)) / ts;
    g.bounce_d = bounce_d;
    /* The perfect moment, as where the ball is seen then: a full ball met early on the front foot, a
     * short one late on the back foot, a yorker dug out late at the crease, spin met coming forward. */
    float meet = bounce_d < STUMPS_D ? 0.8f : bounce_d < 1.5f ? -0.5f : bounce_d < 4.5f ? 0.6f : bounce_d < 9 ? 0
               : bounce_d < 12 ? -0.6f : -0.9f;
    g.d_ideal = D_IDEAL + meet + (g.spin ? 0.4f : 0);
    /* Wide, by the limited-overs playing conditions, judged at the batter's crease: beyond the off-side
     * wide guideline (about 0.9 m from middle stump), down the leg side past the batter's legs, or over
     * his head. Off side is + for a right-hander, mirrored for a left-hander. */
    float ts0 = RELEASE_D / v, xc, yc, dc;
    {
        float X = 0, Y = 0, d = 0;
        g.bounce_d = bounce_d;
        delivery_at(ts0, &X, &Y, &d);
        xc = X * hand(), yc = Y, dc = d;
    }
    (void)dc;
    g.wide = xc > 0.9f || xc < -0.45f || yc > 1.9f;
}

/* The replay caption: pace, length and movement, said after the ball */
static void describe(char *s) {
    char *p = puti(s, g.kmh);
    p = put(p, " km/h  ");
    float b = g.bounce_d;
    p = put(p, b < STUMPS_D ? "full toss" : b < 1.5f ? "yorker" : b < 4.5f ? "full" : b < 9 ? "good length"
                                                                    : b < 12 ? "short" : "bouncer");
    float side = (g.spin ? g.turn : g.ax) * hand(); /* away from the bat: to the off side */
    if (absf(side) > (g.spin ? 0.15f : 0.12f))
        put(p, g.spin ? (side > 0 ? ", turning away" : ", turning in") : (side > 0 ? ", swinging away" : ", swinging in"));
}

static void new_ball(void) {
    int over = over_now();
    g.state = S_RUNUP, g.t = 0;
    pick_delivery();
    if (g.balls % 6 == 0) { /* a new over: a new bowler, fast, medium or spin, sharper or not, and a new field */
        int last = g.bnum;
        set_field();
        place_fielders();
        g.btype = frand() < 0.45f ? 0 : frand() < 0.55f ? 1 : 2;
        g.skill = clampf(g.attack * (0.85f + frand() * 0.3f), 0.6f, 1.5f);
        do g.bnum = 1 + (int)(rnd() % 99);
        while (g.bnum == last);
        char s[48], *p = put(s, g.balls ? "Over " : "");
        if (g.balls) p = puti(p, over + 1), p = put(p, ": ");
        else p = put(p, g.attack < 0.9f ? "Gentle attack. " : g.attack < 1.12f ? "Steady attack. " : "Fiery attack. ");
        p = put(p, "#"), p = puti(p, g.bnum), p = put(p, g.btype == 0 ? ", fast" : g.btype == 1 ? ", medium" : ", spin");
        put(p, g.skill < 0.9f ? ", gentle" : g.skill < 1.12f ? "" : ", sharp");
        banner(s, RGB(255, 255, 255));
    }
    g.six_half = clampf(six_half_for(over) / g.skill, 0.1f, 0.4f);
    g.swing_age = g.ball_age = 0;
    g.swung = g.shown = 0, g.how = H_NONE, g.got = 0, g.catcher = -1, g.bail_t = -1;
    g.bx = g.X0, g.by = g.Y0, g.bd = RELEASE_D;
    place_fielders();
    if (g.bg_over != over) make_bg(over);
}

static void start_match(int overs) {
    g.overs = overs, g.runs = g.wkts = g.balls = 0;
    pick_lineup();
    g.attack = 0.75f + frand() * 0.55f, g.bnum = 0; /* the match's attack: gentle to fiery */
    for (int i = 0; i < FIELDERS; ++i) g.fpace[i] = 0.8f + frand() * 0.4f, g.fhands[i] = 0.72f + frand() * 0.26f;
    set_field();
    /* the target: anything from 6 to 15 an over, a little lower against a fiery attack */
    float rpo = (6 + frand() * 8) * mixf(1.08f, 0.92f, (g.attack - 0.75f) / 0.55f);
    g.target = (int)((float)overs * rpo + 0.5f) + 1;
    for (int i = 0; i <= WICKETS; ++i) g.card[i].runs = g.card[i].balls = g.card[i].fours = g.card[i].sixes = g.card[i].out = 0;
    g.striker = 0, g.other = 1, g.next_in = 2, g.page = 0;
    g.stand = g.best_stand = 0, g.best_a = 0, g.best_b = 1, g.extras = 0;
    int balls = overs * 6;
    (void)balls;
    g.banner_t = 0;
    g.bg_over = -1;
    make_bg(0);
    place_fielders();
    g.swung = 0, g.state = S_COUNT, g.t = 0; /* 3, 2, 1, then the first ball */
}

/* The ball in flight at state time t: a projectile to its bounce, then off the pitch. */
static void delivery_at(float t, float *X, float *Y, float *d) {
    *d = RELEASE_D - g.v * t;
    if (t <= g.tb) {
        *Y = g.Y0 + g.vy0 * t - 0.5f * g.G * t * t;
        *X = g.X0 + g.vx * t + 0.5f * g.ax * t * t;
    } else {
        float u = t - g.tb;
        *Y = g.vyb * u - 0.5f * g.G * u * u;
        if (*Y < 0) *Y = -*Y * 0.4f; /* a second bounce, low */
        *X = g.X0 + g.vx * g.tb + 0.5f * g.ax * g.tb * g.tb + (g.vx + g.ax * g.tb + g.turn) * u;
    }
}

static void banner(const char *s, u32 c) {
    g.banner[0] = 0;
    for (int i = 0; s[i] && i < (int)sizeof g.banner - 1; ++i) g.banner[i] = s[i], g.banner[i + 1] = 0;
    g.banner_t = 1.6f, g.banner_col = c;
}

/* Where the shot goes and who gets to it. A lofted ball lands at `land` metres: a fielder who can
 * run there in its flight time may catch it, surer the shorter their run and the better their hands,
 * or drop it. A ball along the ground is cut off by a fielder who reaches its line before it passes:
 * a four stopped is one or two. A six clears the rope. */
static void field_the_shot(void) {
    g.dropped = g.saved = 0;
    if (g.got == 6) return;
    int lofted = g.got == 2 || (g.got == 4 && frand() < 0.45f) || (g.got == 1 && frand() < 0.3f);
    float a = deg(g.ang), ox = BAT_HIT * hand(), dx = -fsin(a), dd = fcos(a);
    if (lofted) {
        float land = g.got == 4 ? 46 + frand() * 10 : g.got == 2 ? 28 + frand() * 10 : 14 + frand() * 6;
        float T = g.got == 4 ? 1.5f : g.got == 2 ? 1.3f : 1.0f; /* its flight time */
        float lx = ox + dx * land, ld = 0.4f + dd * land, best = 1e9f;
        int k = -1;
        for (int i = 0; i < FIELDERS; ++i) {
            float fx = g.f[i].hx - lx, fd = g.f[i].hd - ld, dist = fsqrt(fx * fx + fd * fd),
                  reach = g.fpace[i] * RUN_SPEED * T + 1.5f;
            if (dist <= reach && dist / reach < best) best = dist / reach, k = i;
        }
        if (k < 0) return; /* in a gap */
        g.f[k].tx = lx, g.f[k].td = ld;
        float p = g.fhands[k] * (1 - 0.4f * best) * clampf(g.skill, 0.8f, 1.2f);
        if (frand() < p) { /* caught */
            g.how = H_DEEP, g.got = 0, g.catcher = k;
            g.R = land, g.ymax = maxf(g.ymax, 8), g.dur = T;
        } else g.dropped = 1, g.R = land, g.ymax = maxf(g.ymax, 8), g.dur = T; /* dropped: the runs stand */
        return;
    }
    /* along the ground: the first fielder who reaches its line in time */
    float best_s = g.R;
    int k = -1;
    for (int i = 0; i < FIELDERS; ++i) {
        float fx = g.f[i].hx - ox, fd = g.f[i].hd - 0.4f, along = fx * dx + fd * dd;
        if (along < 3 || along > g.R) continue;
        float across = absf(fx * dd - fd * dx), t_ball = g.dur * (along / g.R), t_run = across / (g.fpace[i] * RUN_SPEED);
        if (t_run <= t_ball + 0.15f && along < best_s) best_s = along, k = i;
    }
    if (k < 0) return;
    g.f[k].tx = ox + dx * best_s, g.f[k].td = 0.4f + dd * best_s;
    if (g.got == 4) g.got = best_s > 38 ? 2 : 1, g.saved = 1;
    g.dur *= best_s / g.R, g.R = best_s;
}

/* Centre or a tap while the ball comes: the swing, judged by the strip under the ball as it is seen.
 * The ball flies above the pitch, so it is drawn higher than the ground it is over: the band the
 * player sees it in is the ground under its picture, not under the ball. */
static void swing(void) {
    if (g.state != S_BALL || g.swung) return;
    g.swung = 1, g.swing_t = g.t;
    float X, Y, d, sx, sy;
    delivery_at(g.t, &X, &Y, &d);
    proj(X, Y, d, &sx, &sy);
    d = ground_d(sy);
    g.band = -1;
    /* How far from the perfect moment (ms, + late): that decides the shot's quality, and early or late
     * steers it, leg side or off side. A sharper bowler leaves less time. */
    float e = (g.d_ideal - d) / g.v * 1000, ae = absf(e) * clampf(g.skill, 0.75f, 1.35f);
    if (ae > 100) return; /* beaten: on to the stumps */
    if (ae > 68) { /* an edge */
        g.edge_side = e > 0;
        if (frand() < 0.16f * g.skill) {
            g.how = H_BEHIND, g.got = 0;
            g.ang = (g.edge_side ? -160 : 160) * hand(), g.R = 18, g.ymax = 1.5f, g.dur = 0.55f;
        } else {
            g.how = H_RUNS, g.got = frand() < 0.6f;
            g.ang = (g.edge_side ? -150 : 150) * hand(), g.R = 22, g.ymax = 0.6f, g.dur = 0.8f;
        }
    } else {
        /* Where real batters score: the line and length set the shot (on the pads flicked to leg, outside
         * off driven or cut, short pulled, full driven straight), the timing pushes it squarer: early to
         * the leg side (+), late to the off side. */
        float lx = X * hand(), base = clampf(-lx * 320, -95, 95) + 16 + (frand() - 0.5f) * 90; /* the line, his choice */
        if (g.bounce_d > 9) base = lx > 0.3f ? -90 : 60 + frand() * 40;      /* short: cut if wide of off, else pulled */
        else if (g.bounce_d < 4.5f) base *= 0.55f;                          /* full: driven, straighter */
        float a = clampf(base + clampf(-e / 40, -1, 1) * 95, -168, 168);
        g.band = a > 100 ? 0 : a > 30 ? 1 : a > -30 ? 2 : a > -100 ? 3 : 4;
        g.how = H_RUNS, g.ang = a * hand();
        /* a bad ball (a full toss, a half-volley, a short one) is easier to hit for six */
        int bad = g.bounce_d < STUMPS_D || (g.bounce_d > 2.5f && g.bounce_d < 4.5f) || g.bounce_d > 10.5f;
        float r = frand();
        if (ae < 14) g.got = bad ? (r < 0.7f ? 6 : 4) : (r < 0.35f ? 6 : r < 0.8f ? 4 : 2);
        else if (ae < 34) g.got = bad ? (r < 0.25f ? 6 : r < 0.65f ? 4 : 2) : (r < 0.4f ? 4 : r < 0.75f ? 2 : 1);
        else g.got = r < 0.3f ? 2 : r < 0.7f ? 1 : 0;
        if (absf(a) > 110 && g.got == 6) g.got = 4; /* fine: not over the rope */
        if (g.got == 6) g.R = 80, g.ymax = 22, g.dur = 1.7f;
        else if (g.got == 4) g.R = 64, g.ymax = 1.2f, g.dur = 1.35f;
        else if (g.got == 2) g.R = 40, g.ymax = 7, g.dur = 1.15f;
        else if (g.got == 1) g.R = 20, g.ymax = 2, g.dur = 0.9f;
        else g.R = 12, g.ymax = 0.4f, g.dur = 0.7f; /* straight to a fielder: a dot */
        if (g.got) field_the_shot();
    }
    g.state = S_HIT, g.t = 0;
    g.bx = BAT_HIT * hand(), g.by = 0.8f, g.bd = 0.4f;
    if (g.how == H_RUNS && g.got != 6 && !g.dropped && !g.saved) { /* the nearest fielder chases it */
        float a = deg(g.ang), lx = BAT_HIT * hand() - fsin(a) * g.R, ld = 0.4f + fcos(a) * g.R, best = 1e9f;
        int k = 0;
        for (int i = 0; i < FIELDERS; ++i) {
            float dx = g.f[i].hx - lx, dd = g.f[i].hd - ld, q = dx * dx + dd * dd;
            if (q < best) best = q, k = i;
        }
        g.f[k].tx = lx, g.f[k].td = ld;
    }
}

static void end_ball(void) {
    if (g.how == H_WIDE) { /* an extra, not a ball: bowled again, the strike unchanged */
        g.runs++, g.extras++, g.stand++;
        if (g.runs >= g.target) g.state = S_END, g.won = 1, g.t = 0, g.page = 0;
        else new_ball();
        return;
    }
    g.balls++;
    int out = g.how != H_RUNS;
    g.card[g.striker].balls++;
    if (!out) {
        g.runs += g.got, g.stand += g.got, g.card[g.striker].runs += g.got;
        g.card[g.striker].fours += g.got == 4, g.card[g.striker].sixes += g.got == 6;
    } else g.wkts++, g.card[g.striker].out = g.how;
    if (g.stand > g.best_stand || (out && g.best_stand == 0))
        g.best_stand = g.stand, g.best_a = g.striker, g.best_b = g.other;
    if (out) { /* the next batter in, at the striker's end */
        g.stand = 0;
        if (g.next_in <= WICKETS) g.striker = g.next_in++;
    } else if (g.got & 1) { /* an odd number: they have crossed */
        int t = g.striker;
        g.striker = g.other, g.other = t;
    }
    int over_up = g.balls % 6 == 0;
    if (g.runs >= g.target) g.state = S_END, g.won = 1, g.t = 0, g.page = 0;
    else if (g.wkts >= WICKETS || g.balls >= g.overs * 6) g.state = S_END, g.won = 0, g.t = 0, g.page = 0;
    else {
        if (over_up) { /* the end of an over: the bowling changes ends, so the strike does */
            int t = g.striker;
            g.striker = g.other, g.other = t;
        }
        new_ball(); /* after the strike is settled: the field is set for the batter now facing */
        if (out) {
            char s[40], *p = put(s, "New batter #");
            int nb = g.next_in - 1;
            p = puti(p, g.lineup[nb].num);
            put(p, g.lineup[nb].left ? ", left-handed" : ", right-handed");
            banner(s, RGB(255, 255, 255));
        }
    }
}

static void step(float dt) {
    g.clock += dt;
    if (g.banner_t > 0) g.banner_t -= dt;
    if (g.state == S_MENU || g.state == S_PAUSE) return;
    g.t += dt;
    if (g.state == S_END) return;
    if (g.state == S_COUNT) {
        if (g.t >= 3.4f) new_ball();
        return;
    }
    if (g.swung) g.swing_age += dt;
    if (g.state != S_RUNUP) g.ball_age += dt;
    if (g.state == S_RUNUP) {
        if (g.t >= g.runup) g.state = S_BALL, g.t = 0;
        return;
    }
    if (g.state == S_BALL) {
        delivery_at(g.t, &g.bx, &g.by, &g.bd);
        if (g.bd <= STUMPS_D) {
            g.state = S_HIT, g.t = 0, g.dur = 0, g.shown = 1;
            if (g.wide) /* not hit, and out of his reach: a wide, one run, bowled again */
                g.how = H_WIDE, g.got = 0, banner("Wide! +1", RGB(255, 214, 64));
            else if (absf(g.bx) < 0.15f && g.by < 0.75f) /* on the stumps, under the bails: bowled */
                g.how = H_BOWLED, g.got = 0, g.bail_t = 0, banner("BOWLED!", RED);
            else /* past the off or leg stump: a dot ball */
                g.how = H_RUNS, g.got = 0, banner(g.swung ? "Beaten!" : "Left alone", RGB(200, 210, 230));
        }
        return;
    }
    /* S_HIT: the ball across the field, fielders after it */
    if (g.bail_t >= 0) g.bail_t += dt;
    if (g.dur > 0) {
        float u = sat(g.t / g.dur), e = g.ymax > 3 ? u : 1 - (1 - u) * (1 - u), a = deg(g.ang);
        float r = g.R * e;
        g.bx = BAT_HIT * hand() - fsin(a) * r, g.bd = 0.4f + fcos(a) * r;
        g.by = 0.8f * (1 - u) + 4 * g.ymax * u * (1 - u);
        if (u >= 1 && g.how == H_DEEP && g.catcher >= 0) g.f[g.catcher].caught = 1;
    }
    for (int i = 0; i < FIELDERS; ++i) {
        fielder *f = &g.f[i];
        float dx = f->tx - f->x, dd = f->td - f->d, q = fsqrt(dx * dx + dd * dd), sp = 7 * dt;
        if (q > sp) f->x += dx / q * sp, f->d += dd / q * sp;
        else f->x = f->tx, f->d = f->td;
    }
    float show = g.how == H_BOWLED ? 0.1f : g.dur;
    if (!g.shown && g.t >= show) { /* the result, once the ball arrives */
        g.shown = 1;
        if (g.how == H_RUNS) {
            char s[48];
            char *q = g.dropped ? put(s, "Dropped! ") : g.saved ? put(s, "Saved! ") : s;
            q = g.got == 6 ? put(q, "SIX! ") : g.got == 4 ? put(q, "FOUR! ") : put(puti(q, g.got), g.got == 1 ? " run, " : " runs, ");
            put(q, g.band >= 0 ? band_name[g.band] : "edge");
            banner(s, g.got == 6 ? RGB(255, 214, 64) : g.got == 4 ? RGB(110, 220, 255) : RGB(255, 255, 255));
        } else if (g.how == H_BEHIND) banner("CAUGHT BEHIND", RED);
        else if (g.how == H_DEEP) banner("CAUGHT!", RED);
    }
    if (g.t >= show + 1.3f) end_ball();
}

/* ---- the stadium, drawn once (again each over, for its strip) ---- */

static void make_bg(int over) {
    T = bg;
    u32 seed = rng;
    rng = 99991;
    /* sky: evening blue to navy, dithered so the panel shows no bands */
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < W; ++x) {
            float k = (float)y / 40 + (frand() - 0.5f) * 0.03f;
            bg[y * W + x] = RGB((u32)mixf(10, 34, k), (u32)mixf(16, 52, k), (u32)mixf(42, 98, k));
        }
    for (int i = 0; i < 30; ++i) disc(frand() * W, frand() * 24, 0.6f, RGB(255, 255, 255), 0.5f + frand() * 0.4f);
    /* stands: three tiers of coloured seats, a speckled crowd */
    static const u32 tier[4] = { RGB(30, 156, 160), RGB(236, 120, 40), RGB(204, 52, 140), RGB(236, 196, 48) };
    for (int y = 34; y < 92; ++y) {
        int row = (y - 34) / 19;
        for (int x = 0; x < W; ++x) {
            int seg = ((x + row * 23) / 47) & 3;
            u32 c = shade(tier[seg], 0.55f + 0.06f * (float)row + (frand() - 0.5f) * 0.08f);
            bg[y * W + x] = c;
        }
        if ((y - 34) % 19 == 0) for (int x = 0; x < W; ++x) bg[y * W + x] = RGB(20, 22, 34);
        else if ((y - 34) % 4 == 0) for (int x = 0; x < W; ++x) bg[y * W + x] = shade(bg[y * W + x], 0.8f);
    }
    static const u32 crowd[6] = { RGB(250, 230, 200), RGB(210, 160, 120), RGB(120, 80, 60), RGB(255, 255, 255),
                                 RGB(40, 90, 220), RGB(240, 60, 60) };
    for (int i = 0; i < 2600; ++i) {
        int x = (int)(frand() * W), y = 36 + (int)(frand() * 55);
        if ((y - 34) % 19 == 0) continue;
        plot(x, y, crowd[rnd() % 6], 0.85f);
        if (frand() < 0.5f) plot(x, y + 1, crowd[rnd() % 6], 0.7f);
    }
    for (int x = 20; x < W; x += 62) rect(x, 35, x + 3, 91, RGB(52, 54, 66), 0.9f); /* aisles */
    rect(0, 27, W, 35, RGB(22, 24, 36), 1); /* the roof's canopy, its lights under it */
    rect(0, 34, W, 35, RGB(70, 74, 92), 1);
    for (int x = 8; x < W; x += 16) disc((float)x, 33, 0.9f, RGB(255, 240, 200), 0.8f);
    /* a black sightscreen behind the bowler, as for a white ball under lights */
    rect((int)CX - 27, 47, (int)CX + 27, 92, RGB(14, 14, 18), 1);
    rect((int)CX - 28, 46, (int)CX + 28, 47, RGB(90, 94, 110), 1);
    /* the boards */
    static const u32 led[3] = { RGB(24, 40, 96), RGB(16, 120, 128), RGB(180, 70, 30) };
    for (int x = 0; x < W; x += 46) rect(x, 91, x + 46, 97, led[(x / 46) % 3], 1);
    rect(0, 91, W, 92, RGB(160, 180, 230), 0.8f);
    /* floodlights: towers at the sides, their lamps glowing */
    for (int s = 0; s < 2; ++s) {
        float x = s ? 344 : 31;
        capsule(x, 8, x + (s ? 6 : -6), 90, 1.6f, RGB(60, 64, 80), 1);
        rrect((int)x - 17, 2, 34, 12, 3, RGB(48, 52, 66), 1);
        for (int i = 0; i < 4; ++i) disc(x - 11 + 7.3f * (float)i, 8, 2.6f, RGB(255, 248, 220), 1);
        glow(x, 8, 46, RGB(255, 236, 180), 0.55f);
    }
    /* the outfield: mowing stripes by distance, a faint grain */
    for (int y = 97; y < H; ++y) {
        float d = ground_d((float)y + 0.5f);
        int stripe = ((int)((d + 400) / 6)) & 1;
        float s = scale_at(d);
        for (int x = 0; x < W; ++x) {
            float n = (frand() - 0.5f) * 0.05f, X = ((float)x - CX) / s;
            int across = ((int)((X + 400) / 9)) & 1;
            bg[y * W + x] = shade(stripe ? RGB(70, 160, 60) : RGB(58, 142, 52), 1 + n + (across ? 0.035f : -0.035f));
        }
    }
    /* the floodlights' pools on the grass */
    glow(70, 150, 190, RGB(255, 236, 190), 0.10f);
    glow(305, 150, 190, RGB(255, 236, 190), 0.10f);
    glow(CX, 230, 230, RGB(255, 244, 220), 0.06f);
    /* the 30-yard circle: dashes 27.4 m round the pitch's middle */
    for (int i = 0; i < 360; ++i) {
        if ((i / 3) & 1) continue;
        float a = deg((float)i), X = fsin(a) * 27.4f, d = 9.5f + fcos(a) * 27.4f, sx, sy;
        if (d < -2) continue;
        proj(X, 0, d, &sx, &sy);
        disc(sx, sy, maxf(0.6f, 0.05f * scale_at(d)), RGB(245, 245, 245), 0.8f);
    }
    /* the rope, 62 m round the batter */
    for (int i = 0; i <= 720; ++i) {
        float a = deg(-90 + 0.25f * (float)i), X = -fsin(a) * 62, d = fcos(a) * 62, sx, sy;
        if (d < -2) continue;
        proj(X, 0, d, &sx, &sy);
        disc(sx, sy, maxf(0.7f, 0.06f * scale_at(d)), RGB(250, 250, 250), 0.9f);
    }
    /* the pitch: tan, worn at the ends */
    for (int y = 97; y < H; ++y) {
        float d = ground_d((float)y + 0.5f);
        if (d > 22 || d < -3.5f) continue;
        float s = scale_at(d), hw = 1.52f * s;
        for (int x = (int)(CX - hw); x <= (int)(CX + hw); ++x) {
            float edge = sat(hw + 0.5f - absf((float)x + 0.5f - CX));
            float X = ((float)x + 0.5f - CX) / s;
            float worn = (absf(d - 1) < 3 || absf(d - 18) < 3) ? 0.92f : 1, mid = absf(X) < 0.55f ? 1.06f : 1;
            u32 c = absf(X) > 1.3f ? RGB(150, 170, 90) : RGB(204, 172, 116); /* the edges grassier */
            plot(x, y, shade(c, worn * mid * (1 + (frand() - 0.5f) * 0.08f)), edge);
        }
    }
    /* footmarks: the bowler's landing, the batter's guard */
    for (int i = 0; i < 9; ++i) {
        float X = (frand() - 0.5f) * 0.9f + 0.35f, d = 18.6f + frand() * 1.6f, sx, sy, s2 = scale_at(d);
        proj(X, 0, d, &sx, &sy);
        ellipse(sx, sy, 0.16f * s2, 0.05f * s2, RGB(120, 92, 60), 0.35f);
    }
    for (int i = 0; i < 5; ++i) {
        float X = (frand() - 0.5f) * 0.4f + 0.1f, d = -0.5f + frand() * 0.4f, sx, sy, s2 = scale_at(d);
        proj(X, 0, d, &sx, &sy);
        ellipse(sx, sy, 0.1f * s2, 0.03f * s2, RGB(120, 92, 60), 0.3f);
    }
    /* creases at both ends: popping, bowling, and the return creases back from the popping crease */
    for (int k = 0; k < 2; ++k) {
        float pop = k ? 18.9f : 0, bowl = k ? 20.12f : -1.22f, back = k ? 21.4f : -2.5f, x0, y0, x1, y1;
        float w = maxf(0.5f, 0.025f * scale_at(pop));
        proj(-1.9f, 0, pop, &x0, &y0), proj(1.9f, 0, pop, &x1, &y1);
        capsule(x0, y0, x1, y1, w, RGB(250, 250, 245), 0.95f);
        proj(-1.32f, 0, bowl, &x0, &y0), proj(1.32f, 0, bowl, &x1, &y1);
        capsule(x0, y0, x1, y1, w, RGB(250, 250, 245), 0.9f);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            proj(1.32f * (float)sgn, 0, pop, &x0, &y0), proj(1.32f * (float)sgn, 0, back, &x1, &y1);
            capsule(x0, y0, x1, y1, w, RGB(250, 250, 245), 0.9f);
        }
    }
    /* the strip (red ends, five bands, each with its six window and bright centre) is not drawn: the
     * timing is felt, not read. SHOW_STRIP draws it, for tuning. */
    float six_half = six_half_for(over);
    for (int y = SHOW_STRIP ? 97 : H; y < H; ++y) {
        float d = ground_d((float)y + 0.5f);
        if (d > STRIP_FAR || d < STRIP_NEAR) continue;
        float s = scale_at(d), hw = 0.5f * s, mx = CX + STRIP_X * s;
        u32 c;
        float a = 0.55f;
        if (d > BAND0 || d < BAND0 - 5 * BAND_W) c = RED;
        else {
            int b = (int)((BAND0 - d) / BAND_W);
            if (b > 4) b = 4;
            float centre = BAND0 - BAND_W * ((float)b + 0.5f), off = absf(d - centre);
            c = band_col[b];
            if (off < six_half) c = shade(c, 1.45f), a = 0.75f;
            if (off < 0.06f) c = RGB(255, 255, 255), a = 0.9f;
        }
        for (int x = (int)(mx - hw); x <= (int)(mx + hw); ++x)
            plot(x, y, c, a * sat(hw + 0.5f - absf((float)x + 0.5f - mx)));
    }
    /* the bowler's stumps */
    for (int i = -1; i <= 1; ++i) {
        float x0, y0, x1, y1;
        proj(0.11f * (float)i, 0, 20.12f, &x0, &y0), proj(0.11f * (float)i, 0.71f, 20.12f, &x1, &y1);
        capsule(x0, y0, x1, y1, 0.6f, RGB(240, 230, 200), 1);
    }
    {
        float x0, y0, x1, y1;
        proj(-0.13f, 0.73f, 20.12f, &x0, &y0), proj(0.13f, 0.73f, 20.12f, &x1, &y1);
        capsule(x0, y0, x1, y1, 0.5f, RGB(255, 245, 220), 1); /* bails */
    }
    rng = seed;
    T = cv;
    g.bg_over = over;
}

/* ---- people ---- */

typedef struct { u32 shirt, trim, pants, skin, cap; } kit;
static const kit BATTING = { RGB(40, 84, 214), RGB(250, 200, 46), RGB(32, 66, 172), RGB(200, 150, 110), RGB(22, 36, 96) };
static const kit FIELDING = { RGB(30, 150, 84), RGB(250, 250, 250), RGB(24, 116, 66), RGB(176, 124, 90), RGB(20, 90, 50) };
static const kit UMPIRE = { RGB(250, 250, 250), RGB(40, 40, 40), RGB(40, 40, 46), RGB(210, 160, 120), RGB(20, 20, 24) };

enum { P_IDLE, P_RUN, P_BOWL, P_SIX, P_FOUR, P_OUT, P_CATCH, P_WIDE };

/* A figure standing at (X, d), k pixels to the metre (boosted far away, so it reads), in a pose. */
/* The other players: drawn frames (art/gen_people.py, packed into sprites.h), facing us. */
#include "sprites.h"

static u32 frame_buf[128 * 128]; /* one people frame, decoded */

/* People frame f, decoded and scaled smoothly (bilinear, premultiplied) to h pixels tall, standing at
 * (x, y). */
static void blit_scaled(int f, float x, float y, float h) {
    const sprite *s = &people_frames[f];
    const unsigned *p = people_px + s->off;
    int n = s->w * s->h, i = 0;
    if (n > (int)(sizeof frame_buf / sizeof *frame_buf)) return;
    while (i < n) {
        unsigned w = *p++;
        if (w & 0x80000000u) {
            for (unsigned k = w & 0x7fffffffu; k--;) frame_buf[i++] = 0;
            continue;
        }
        for (unsigned k = 0; k < w; ++k) frame_buf[i++] = *p++;
    }
    float sc = h / PEOPLE_H, inv = 1 / sc;
    int x0 = (int)(x - (float)s->ax * sc) - 1, x1 = (int)(x + (float)(s->w - s->ax) * sc) + 1;
    int y0 = (int)(y - (float)s->ay * sc) - 1, y1 = (int)(y + (float)(s->h - s->ay) * sc) + 1;
    for (int py = y0; py <= y1; ++py) {
        if ((unsigned)py >= H) continue;
        float sy = (float)s->ay + ((float)py + 0.5f - y) * inv - 0.5f;
        int iy = (int)(sy + 1) - 1;
        float fy = sy - (float)iy;
        for (int px = x0; px <= x1; ++px) {
            if ((unsigned)px >= W) continue;
            float sx = (float)s->ax + ((float)px + 0.5f - x) * inv - 0.5f;
            int ix = (int)(sx + 1) - 1;
            float fx = sx - (float)ix, r = 0, gr = 0, b = 0, a = 0;
            for (int q = 0; q < 4; ++q) {
                int tx = ix + (q & 1), ty = iy + (q >> 1);
                if (tx < 0 || ty < 0 || tx >= s->w || ty >= s->h) continue;
                u32 c = frame_buf[ty * s->w + tx];
                float wgt = ((q & 1) ? fx : 1 - fx) * ((q >> 1) ? fy : 1 - fy), al = (float)(c >> 24) * wgt;
                r += (float)(c >> 16 & 255) * al, gr += (float)(c >> 8 & 255) * al, b += (float)(c & 255) * al, a += al;
            }
            if (a < 1) continue;
            plot(px, py, RGB((u32)(r / a), (u32)(gr / a), (u32)(b / a)), a * (1 / 255.0f));
        }
    }
}

/* A fielder, the bowler, the umpire or the non-striker (by kit), standing at (X, d), drawn boost times
 * life size so it reads far away, in a pose; ph drives the motion. */
static void figure(float X, float d, float boost, const kit *c, int pose, float ph) {
    float fx, fy;
    proj(X, 0, d, &fx, &fy);
    float h = 1.85f * scale_at(d) * boost;
    int f;
    if (c == &UMPIRE)
        f = pose == P_SIX ? PF_U_SIX : pose == P_WIDE ? PF_U_WIDE : pose == P_FOUR ? ((int)(ph * 4) & 1 ? PF_U_FOUR1 : PF_U_FOUR0)
          : pose == P_OUT ? PF_U_OUT : PF_U_STAND;
    else if (c == &BATTING)
        f = pose == P_RUN ? ((int)ph & 1 ? PF_N_JOG1 : PF_N_JOG0) : PF_N_STAND;
    else if (pose == P_BOWL)
        f = ph - PI < 1 ? PF_B_BOWL0 : ph - PI < 2 ? PF_B_BOWL1 : PF_B_BOWL2;
    else if (pose == P_RUN) {
        static const int cycle[4] = { PF_F_RUN0, PF_F_RUN1, PF_F_RUN2, PF_F_RUN3 };
        f = cycle[(int)(ph / (PI / 2)) & 3];
    } else if (pose == P_CATCH) f = PF_F_CATCH;
    else f = fsin(ph * 0.5f) > 0 ? PF_F_READY0 : PF_F_READY1;
    blit_scaled(f, fx, fy, h);
}

/* The batter: drawn frames (art/gen_batter.py, packed by art/pack.py into sprites.h), a right-hander
 * seen from behind; a left-hander is the same frames mirrored. He stands in his crease, a little to
 * the leg side of the stumps, the bat grounded in front of them; his shirt number is printed on. */
/* Frame f with its back foot at (x, y); flip mirrors it about that foot. */
static void blit(int f, float x, float y, int flip) {
    const sprite *s = &batter_frames[f];
    const unsigned *p = batter_px + s->off;
    int n = s->w * s->h, i = 0, ix = (int)(x + 0.5f), iy = (int)(y + 0.5f);
    while (i < n) {
        unsigned w = *p++;
        if (w & 0x80000000u) {
            i += (int)(w & 0x7fffffffu);
            continue;
        }
        for (unsigned k = 0; k < w; ++k, ++i) {
            unsigned c = *p++;
            int px = i % s->w - s->ax, py = i / s->w - s->ay;
            plot(flip ? ix - px : ix + px, iy + py, c | 0xff000000u, (float)(c >> 24) * (1 / 255.0f));
        }
    }
}

/* The frame for now: the stance (a bat tap as the bowler runs in), the backlift as the ball comes, the
 * shot's downswing, contact, follow-through and finish, a ball left (shoulder arms), out. */
static int batter_frame(void) {
    static const int shot[5][3] = { { BF_GLANCE0, BF_GLANCE1, BF_GLANCE2 }, { BF_PULL0, BF_PULL1, BF_PULL2 },
                                    { BF_DRIVE0, BF_DRIVE1, BF_DRIVE2 }, { BF_COVER0, BF_COVER1, BF_COVER2 },
                                    { BF_DAB0, BF_DAB1, BF_DAB2 } };
    if (g.state == S_MENU || g.state == S_END || g.state == S_COUNT) return BF_STANCE;
    if (g.state == S_RUNUP) return g.t < g.runup - 0.35f && ((int)(g.t * 5) & 1) ? BF_TAP : BF_STANCE;
    int out = g.how == H_BOWLED || g.how == H_BEHIND || g.how == H_DEEP;
    if (g.swung) {
        int b = g.band >= 0 ? g.band : g.edge_side ? 4 : 0;
        float a = g.swing_age;
        if (out && a > 0.9f) return BF_OUT;
        return a < 0.05f ? BF_DOWN1 : a < 0.1f ? BF_DOWN2 : a < 0.2f ? shot[b][0] : a < 0.34f ? shot[b][1] : shot[b][2];
    }
    if (g.state == S_HIT && g.how == H_RUNS) return BF_LEAVE;        /* left alone */
    if (g.state == S_HIT && g.how == H_BOWLED) return g.t > 0.4f ? BF_OUT : BF_LIFT2;
    return g.ball_age < 0.18f ? BF_LIFT1 : BF_LIFT2;
}

static void batter(void) {
    int f = batter_frame(), left = hand() < 0;
    float x, y;
    proj(0, 0, -0.55f, &x, &y); /* his back foot, between the crease and the stumps */
    x = CX + (left ? 8.5f : -8.5f);
    blit(f, x, y, left);
    const sprite *s = &batter_frames[f];
    char n[4];
    puti(n, g.lineup[batter_in()].num);
    float k = 0.42f, nx = x + (float)(left ? -s->nx : s->nx), ny = y + (float)s->ny - FONT_H * k / 2;
    text_c(nx + 0.6f, ny + 0.6f, n, k, RGB(16, 24, 51), 0.9f);
    text_c(nx, ny, n, k, RGB(245, 192, 48), 1);
}

static void stumps(void) {
    float t = g.bail_t;
    for (int i = -1; i <= 1; ++i) {
        float x0, y0, x1, y1, lean = t >= 0 ? (float)i * 0.08f + 0.1f * sat(t * 6) * (i == 0 ? 1 : (float)i) : 0;
        proj(0.11f * (float)i, 0, STUMPS_D, &x0, &y0);
        proj(0.11f * (float)i + lean, 0.71f, STUMPS_D + (t >= 0 ? -0.15f * sat(t * 6) : 0), &x1, &y1);
        capsule(x0, y0, x1, y1, 0.02f * scale_at(STUMPS_D), RGB(244, 236, 210), 1);
        capsule(x0, y0 - 3, x1, y1, 0.008f * scale_at(STUMPS_D), RGB(220, 40, 40), 0.35f);
    }
    for (int b = 0; b < 2; ++b) { /* bails, flying when bowled */
        float bx = -0.055f + 0.11f * (float)b, by = 0.72f, bd = STUMPS_D, rot = 0;
        if (t >= 0) bx += ((float)b - 0.5f) * 1.6f * t, by += 2.4f * t - 4.9f * t * t, bd -= 0.6f * t, rot = 8 * t;
        if (by < 0) by = 0;
        float sx, sy;
        proj(bx, by, bd, &sx, &sy);
        float L = 0.055f * scale_at(bd);
        capsule(sx - fcos(rot) * L, sy - fsin(rot) * L, sx + fcos(rot) * L, sy + fsin(rot) * L, 1.4f, RGB(250, 240, 210), 1);
    }
}

static void ball(void) {
    if (g.state != S_BALL && g.state != S_HIT) return;
    if (g.state == S_HIT && g.dur == 0) return; /* bowled, or past the stumps to the keeper */
    if (g.bd + CAM_C < 1.2f) return; /* behind the camera: off screen */
    float sx, sy, gx, gy;
    proj(g.bx, g.by, g.bd, &sx, &sy);
    proj(g.bx, 0, g.bd, &gx, &gy);
    float r = maxf(3.2f, 0.21f * scale_at(g.bd)); /* drawn large: a real ball would be a pixel or two */
    ellipse(gx, gy, r * 1.2f, r * 0.45f, RGB(0, 0, 0), 0.3f); /* shadow */
    if (g.state == S_BALL) { /* a short trail */
        for (int i = 1; i <= 4; ++i) {
            float X, Y, d, tx, ty;
            delivery_at(g.t - 0.012f * (float)i, &X, &Y, &d);
            proj(X, Y, d, &tx, &ty);
            disc(tx, ty, r * (1 - 0.18f * (float)i), RGB(255, 80, 70), 0.2f);
        }
    }
    disc(sx, sy, r + 0.6f, RGB(60, 0, 0), 0.5f); /* a dark rim, so it reads on the pitch */
    disc(sx, sy, r, RGB(214, 22, 30), 1);
    capsule(sx - r * 0.7f, sy + r * 0.2f, sx + r * 0.7f, sy - r * 0.2f, maxf(0.35f, r * 0.12f), RGB(250, 230, 200), 0.85f); /* the seam */
    disc(sx - r * 0.35f, sy - r * 0.35f, r * 0.3f, RGB(255, 170, 160), 0.7f);
}

static void people(void) {
    /* fielders, back to front */
    int order[FIELDERS];
    for (int i = 0; i < FIELDERS; ++i) order[i] = i;
    for (int i = 0; i < FIELDERS; ++i)
        for (int j = i + 1; j < FIELDERS; ++j)
            if (g.f[order[j]].d > g.f[order[i]].d) {
                int t = order[i];
                order[i] = order[j], order[j] = t;
            }
    /* umpire and bowler at the far end, first */
    int pose = P_IDLE;
    if (g.state == S_HIT && g.t > (g.how == H_BOWLED ? 0.2f : g.dur)) {
        if (g.how == H_RUNS) pose = g.got == 6 ? P_SIX : g.got == 4 ? P_FOUR : P_IDLE;
        else if (g.how == H_WIDE) pose = P_WIDE;
        else pose = P_OUT;
    }
    figure(-0.8f, 22.4f, 2.5f, &UMPIRE, pose, g.clock);
    float bd = 30, ph = 0;
    int bpose = P_IDLE;
    if (g.state == S_RUNUP) bd = mixf(g.runup < 1 ? 24 : 30, RELEASE_D + 0.6f, sat(g.t / g.runup)), bpose = P_RUN, ph = g.t * 14;
    else if (g.state == S_BALL) bd = RELEASE_D + 0.6f - minf(g.t, 0.4f) * 3, bpose = g.t < 0.25f ? P_BOWL : P_RUN,
                                ph = g.t < 0.25f ? PI + g.t * 12 : g.t * 10;
    else if (g.state == S_COUNT) bd = 30; /* at the top of his run */
    else bd = RELEASE_D - 0.6f;
    for (int n = 0; n < FIELDERS; ++n) {
        fielder *f = &g.f[order[n]];
        if (f->d < bd) break;
        int moving = absf(f->x - f->tx) + absf(f->d - f->td) > 0.05f;
        figure(f->x, f->d, 3.0f, &FIELDING, f->caught ? P_CATCH : moving ? P_RUN : P_IDLE, g.clock * 12);
    }
    if (g.state != S_MENU) { /* the non-striker, bat in hand, at the bowler's end */
        figure(-1.35f, 19.4f, 2.5f, &BATTING, g.state == S_BALL && g.t < 0.6f ? P_RUN : P_IDLE, g.clock * 8);
    }
    figure(0.55f, bd, 2.5f, &FIELDING, bpose, ph);
    for (int n = 0; n < FIELDERS; ++n) {
        fielder *f = &g.f[order[n]];
        if (f->d >= bd) continue;
        if (f->d + CAM_C < 3) continue;
        int moving = absf(f->x - f->tx) + absf(f->d - f->td) > 0.05f;
        figure(f->x, f->d, 2.4f, &FIELDING, f->caught ? P_CATCH : moving ? P_RUN : P_IDLE, g.clock * 12);
    }
}

/* ---- panels ---- */

static void panel(int x, int y, int w, int h) {
    rrect(x, y, w, h, 7, RGB(10, 14, 30), 0.78f);
}

static void hud(void) {
    /* one bar in the middle: the Q2's glass rounds its corners off */
    char s[48], *p;
    panel((int)CX - 128, 4, 256, 44);
    p = puti(s, g.runs), p = put(p, "/"), p = puti(p, g.wkts);
    text(CX - 118, 5, s, 1, RGB(255, 255, 255), 1);
    p = put(s, "Ov "), p = puti(p, g.balls / 6), p = put(p, "."), p = puti(p, g.balls % 6), p = put(p, "/"), p = puti(p, g.overs);
    p = put(p, "  #"), p = puti(p, g.lineup[g.striker].num), p = put(p, "* "), p = puti(p, g.card[g.striker].runs);
    p = put(p, "("), p = puti(p, g.card[g.striker].balls), put(p, ")");
    text(CX - 118, 27, s, 0.7f, RGB(190, 205, 235), 1);
    p = put(s, "Target "), puti(p, g.target);
    text(CX + 118 - text_w(s, 0.9f), 6, s, 0.9f, RGB(255, 214, 64), 1);
    if (g.state == S_HIT && g.t > 0.4f) { /* the speed gun, as a replay caption: after the ball */
        describe(s);
        float w = text_w(s, 0.62f);
        rrect((int)(CX - w / 2 - 8), 50, (int)w + 16, 18, 6, RGB(10, 14, 30), 0.7f);
        text_c(CX, 50, s, 0.62f, RGB(255, 255, 255), 1);
    }
    int need = g.target - g.runs, left = g.overs * 6 - g.balls;
    if (g.state != S_END) {
        p = put(s, "Need "), p = puti(p, need > 0 ? need : 0), p = put(p, " off "), puti(p, left);
        text(CX + 118 - text_w(s, 0.7f), 27, s, 0.7f, RGB(190, 205, 235), 1);
    }
}

/* The umpire's signal, close up, as television cuts to him: six, four, out, wide */
static void umpire_close_up(void) {
    if (g.state != S_HIT || !g.shown || g.banner_t < 0.25f) return;
    int f;
    if (g.how == H_RUNS && g.got == 6) f = PF_U_SIX;
    else if (g.how == H_RUNS && g.got == 4) f = (int)(g.clock * 4) & 1 ? PF_U_FOUR1 : PF_U_FOUR0;
    else if (g.how == H_WIDE) f = PF_U_WIDE;
    else if (g.how == H_BOWLED || g.how == H_BEHIND || g.how == H_DEEP) f = PF_U_OUT;
    else return;
    float a = sat((1.6f - g.banner_t) * 6) * sat(g.banner_t * 4);
    int x = 14, y = 110, w = 96, h = 132;
    rrect(x, y, w, h, 10, RGB(10, 14, 30), 0.8f * a);
    rrect(x + 3, y + 3, w - 6, h - 26, 8, RGB(60, 140, 60), 0.9f * a);
    if (a > 0.5f) blit_scaled(f, (float)x + w / 2.0f, (float)(y + h - 26), 96);
    text_c((float)x + w / 2.0f, (float)(y + h - 22), "UMPIRE", 0.6f, RGB(230, 236, 250), a);
}

static void result_banner(void) {
    if (g.banner_t <= 0) return;
    float u = 1.6f - g.banner_t, pop = u < 0.15f ? u / 0.15f : 1, a = sat(g.banner_t * 2.5f);
    float k = ((g.banner[0] == 'S' && g.banner[1] == 'I') || g.banner[0] == 'F') ? 1.7f * pop : 1.3f * pop;
    if (g.banner[0] == 'S' && g.banner[1] == 'I') /* a burst behind a six */
        for (int i = 0; i < 14; ++i) {
            float an = (float)i * (2 * PI / 14) + g.clock, r = 30 + 70 * u;
            disc(CX + fcos(an) * r, 120 + fsin(an) * r * 0.6f, 4 * (1 - u * 0.5f), band_col[i % 5], a);
        }
    k = minf(k, (W - 24) / maxf(text_w(g.banner, 1), 1)); /* a long one fits the screen */
    if (k > 0.05f) text_shadow(CX, 100 - FONT_H * k / 2, g.banner, k, g.banner_col, a);
}

static const char *const menu_rows[4] = { "1 Over", "2 Overs", "5 Overs", "Quit" };

static void menu_screen(void) {
    for (int i = 0; i < W * H; ++i) cv[i] = mixp(cv[i], RGB(4, 6, 16), 120);
    text_shadow(CX, 34, "SIX TRACK", 2.2f, RGB(255, 214, 64), 1);
    text_c(CX, 92, "Chase the target. 10 wickets.", 0.8f, RGB(200, 214, 240), 1);
    for (int i = 0; i < 4; ++i) {
        int y = 126 + i * 40;
        if (i == g.sel) rrect((int)CX - 90, y - 4, 180, 34, 10, RGB(40, 84, 214), 0.95f);
        else rrect((int)CX - 90, y - 4, 180, 34, 10, RGB(20, 26, 50), 0.7f);
        text_c(CX, (float)y, menu_rows[i], 1, RGB(255, 255, 255), 1);
    }
    text_c(CX, 294, "Wheel: choose   Centre: play   Hold Return: exit", 0.62f, RGB(170, 180, 205), 1);
}

static void pause_screen(void) {
    for (int i = 0; i < W * H; ++i) cv[i] = mixp(cv[i], RGB(4, 6, 16), 130);
    panel((int)CX - 100, 96, 200, 130);
    text_c(CX, 104, "Paused", 1.2f, RGB(255, 255, 255), 1);
    static const char *const rows[2] = { "Resume", "Menu" };
    for (int i = 0; i < 2; ++i) {
        int y = 146 + i * 38;
        if (i == g.sel) rrect((int)CX - 80, y - 4, 160, 32, 9, RGB(40, 84, 214), 0.95f);
        text_c(CX, (float)y, rows[i], 1, RGB(255, 255, 255), 1);
    }
}

static void card_screen(void) {
    char s[64], *p;
    for (int i = 0; i < W * H; ++i) cv[i] = mixp(cv[i], RGB(4, 6, 16), 238);
    p = put(s, "Scorecard  "), p = puti(p, g.runs), p = put(p, "/"), p = puti(p, g.wkts), p = put(p, "  ("),
    p = puti(p, g.balls / 6), p = put(p, "."), p = puti(p, g.balls % 6), put(p, " ov)");
    text_c(CX, 30, s, 0.8f, RGB(255, 214, 64), 1);
    int y = 54, shown = 0;
    for (int i = 0; i <= WICKETS; ++i) {
        int in = i < g.next_in;
        if (!in) continue;
        const char *how = g.card[i].out == H_BOWLED ? "b" : g.card[i].out == H_BEHIND ? "c behind" : g.card[i].out == H_DEEP ? "c deep" : "not out";
        p = put(s, "#"), p = puti(p, g.lineup[i].num), put(p, g.lineup[i].left ? " L" : " R");
        u32 c = g.card[i].out ? RGB(200, 210, 230) : RGB(255, 255, 255);
        text(46, (float)y, s, 0.58f, c, 1);
        text(112, (float)y, how, 0.58f, g.card[i].out ? RGB(236, 120, 120) : RGB(120, 220, 140), 1);
        p = puti(s, g.card[i].runs), p = put(p, " ("), p = puti(p, g.card[i].balls), put(p, ")");
        text(250 - text_w(s, 0.58f), (float)y, s, 0.58f, c, 1);
        p = puti(s, g.card[i].fours), p = put(p, "x4  "), p = puti(p, g.card[i].sixes), put(p, "x6");
        text(262, (float)y, s, 0.52f, RGB(170, 186, 220), 1);
        y += 18, ++shown;
    }
    if (g.next_in <= WICKETS) { /* did not bat */
        p = put(s, "Did not bat:");
        for (int i = g.next_in; i <= WICKETS && p - s < 50; ++i) p = put(p, " #"), p = puti(p, g.lineup[i].num);
        text_c(CX, (float)y + 2, s, 0.52f, RGB(170, 186, 220), 1);
        y += 18;
    }
    p = put(s, "Extras "), p = puti(p, g.extras), put(p, " (wides)");
    text_c(CX, (float)y + 4, s, 0.52f, RGB(170, 186, 220), 1);
    y += 16;
    p = put(s, "Best stand "), p = puti(p, g.best_stand), p = put(p, ": #"), p = puti(p, g.lineup[g.best_a].num),
    p = put(p, " & #"), puti(p, g.lineup[g.best_b].num);
    text_c(CX, (float)y + 4, s, 0.58f, RGB(255, 214, 64), 1);
    text_c(CX, 292, "Centre: menu", 0.6f, RGB(255, 255, 255), 1);
    (void)shown;
}

static void end_screen(void) {
    if (g.page) {
        card_screen();
        return;
    }
    char s[48], *p;
    float a = sat(g.t * 3);
    for (int i = 0; i < W * H; ++i) cv[i] = mixp(cv[i], RGB(4, 6, 16), (u32)(110 * a));
    panel((int)CX - 140, 80, 280, 150);
    if (g.won) {
        text_shadow(CX, 88, "YOU WON!", 1.8f, RGB(255, 214, 64), a);
        p = put(s, "by "), p = puti(p, WICKETS - g.wkts), p = put(p, WICKETS - g.wkts == 1 ? " wicket" : " wickets");
        int left = g.overs * 6 - g.balls;
        p = put(p, ", "), p = puti(p, left), put(p, left == 1 ? " ball left" : " balls left");
    } else {
        text_shadow(CX, 88, "LOST", 1.8f, RGB(236, 90, 90), a);
        int by = g.target - 1 - g.runs;
        if (by <= 0) put(s, "Scores level: a tie!");
        else p = put(s, "by "), p = puti(p, by), put(p, by == 1 ? " run" : " runs");
    }
    text_c(CX, 140, s, 0.9f, RGB(230, 236, 250), a);
    p = puti(s, g.runs), p = put(p, "/"), p = puti(p, g.wkts), p = put(p, " chasing "), puti(p, g.target);
    text_c(CX, 166, s, 0.8f, RGB(170, 186, 220), a);
    text_c(CX, 198, "Centre: scorecard", 0.8f, RGB(255, 255, 255), a);
}

/* This ball's timing area, very faint across the pitch: the good window, the middled one inside it, and
 * a thin line at the perfect moment. The ball's picture over the line is the moment, as it is judged.
 * Shown while the ball comes, faded once the bat swings. */
static void timing_area(void) {
    if (g.state != S_BALL && g.state != S_RUNUP) return;
    float fade = g.state == S_RUNUP ? sat((g.t - (g.runup - 0.5f)) * 3) : g.swung ? sat(1 - (g.t - g.swing_t) * 5) : 1;
    if (fade <= 0) return;
    float k = 1 / clampf(g.skill, 0.75f, 1.35f), good = 0.034f * k * g.v, mid = 0.014f * k * g.v;
    for (int y = 97; y < H; ++y) {
        float d = ground_d((float)y + 0.5f), off = absf(d - g.d_ideal);
        if (off > good) continue;
        float a = (off < 0.03f * g.v ? 0.22f : off < mid ? 0.13f : 0.07f * (1 - off / good) + 0.03f) * fade;
        float s = scale_at(d), hw = 3.2f * s; /* across the grass both sides: the batter hides the pitch there */
        for (int x = (int)(CX - hw); x <= (int)(CX + hw); ++x)
            plot(x, y, RGB(255, 246, 220), a * sat(hw + 0.5f - absf((float)x + 0.5f - CX)) * sat((hw - absf((float)x + 0.5f - CX)) / (0.25f * hw)));
    }
}

/* The perfect line's ends, beside the pitch, over everything: two small soft ticks the batter can't hide */
static void timing_ticks(void) {
    if (g.state != S_BALL && g.state != S_RUNUP) return;
    float fade = g.state == S_RUNUP ? sat((g.t - (g.runup - 0.5f)) * 3) : g.swung ? sat(1 - (g.t - g.swing_t) * 5) : 1;
    if (fade <= 0) return;
    float x0, y0, x1, y1;
    proj(-2.0f, 0, g.d_ideal, &x0, &y0), proj(-1.6f, 0, g.d_ideal, &x1, &y1);
    capsule(x0, y0, x1, y1, 1.1f, RGB(255, 246, 220), 0.55f * fade);
    proj(1.6f, 0, g.d_ideal, &x0, &y0), proj(2.0f, 0, g.d_ideal, &x1, &y1);
    capsule(x0, y0, x1, y1, 1.1f, RGB(255, 246, 220), 0.55f * fade);
}

static void render(void) {
    for (int i = 0; i < W * H; ++i) cv[i] = bg[i];
    T = cv;
    timing_area();
    people();
    ball();
    batter();
    stumps();
    timing_ticks();
    if (g.state == S_MENU) {
        menu_screen();
        return;
    }
    hud();
    umpire_close_up();
    result_banner();
    if (g.state == S_COUNT) { /* 3, 2, 1, PLAY! */
        int n = 3 - (int)(g.t / 0.85f);
        float f = g.t - 0.85f * (float)(3 - n), pop = 1 + 0.6f * sat(1 - f * 5), a = sat((0.85f - f) * 4);
        char s[8];
        if (n > 0) puti(s, n);
        else put(s, "PLAY!"), a = sat((3.4f - g.t) * 4);
        text_shadow(CX, 104 - FONT_H * 2.6f * pop / 2, s, 2.6f * pop, n > 0 ? RGB(255, 255, 255) : RGB(255, 214, 64), a);
    }
    if ((g.state == S_RUNUP && g.balls == 0) || g.state == S_COUNT)
        text_shadow(CX, 230, "Centre or tap to swing", 0.8f, RGB(255, 255, 255), sat(1.2f - g.t));
    if (g.state == S_PAUSE) pause_screen();
    if (g.state == S_END) end_screen();
}

/* ---- input ---- */

static void choose(void) {
    if (g.state == S_MENU) {
        if (g.sel == 3) g.quit = 1;
        else start_match(over_opts[g.sel]);
    } else if (g.state == S_PAUSE) {
        if (g.sel == 0) g.state = g.paused_from;
        else g.state = S_MENU, g.sel = 1;
    } else if (g.state == S_END) {
        if (g.t > 0.6f) {
            if (!g.page) g.page = 1;
            else g.state = S_MENU, g.sel = 1;
        }
    } else swing();
}

static int parse_int(const char **s) {
    int v = 0, neg = 0;
    while (**s == ' ') ++*s;
    if (**s == '-') neg = 1, ++*s;
    while (**s >= '0' && **s <= '9') v = v * 10 + (*(*s)++ - '0');
    return neg ? -v : v;
}

static void input(const char *m) {
    if (m[0] == 'k' && m[2] == 'd') {
        const char *s = m + 3;
        int code = parse_int(&s);
        if (code == KEY_CENTER) choose();
        else if (code == KEY_RETURN) {
            if (g.state == S_RUNUP || g.state == S_BALL || g.state == S_HIT || g.state == S_COUNT) g.paused_from = g.state, g.state = S_PAUSE, g.sel = 0;
            else if (g.state == S_PAUSE) g.state = g.paused_from;
        }
    } else if (m[0] == 't' && m[2] == 'd') {
        const char *s = m + 3;
        int x = parse_int(&s), y = parse_int(&s);
        (void)x;
        if (g.state == S_MENU) {
            for (int i = 0; i < 4; ++i)
                if (y >= 122 + i * 40 && y < 156 + i * 40) g.sel = i, choose();
        } else if (g.state == S_PAUSE) {
            for (int i = 0; i < 2; ++i)
                if (y >= 142 + i * 38 && y < 174 + i * 38) g.sel = i, choose();
        } else choose();
    } else if (m[0] == 'w') {
        const char *s = m + 1;
        g.wheel += (float)parse_int(&s);
        int rows = g.state == S_MENU ? 4 : g.state == S_PAUSE ? 2 : 0;
        while (g.wheel >= 40) g.wheel -= 40, g.sel = rows ? (g.sel + rows - 1) % rows : g.sel;
        while (g.wheel <= -40) g.wheel += 40, g.sel = rows ? (g.sel + 1) % rows : g.sel;
    }
}

static void game_init(u32 seed) {
    rng ^= seed;
    g.state = S_MENU, g.sel = 1;
    g.overs = 2, g.target = 0;
    g.bg_over = -1;
    place_fielders();
    make_bg(0);
}

#ifdef HOST
/* ---- preview: a scripted session, a BMP every few frames ---- */
#include <stdio.h>
static void bmp(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned char h[54] = { 'B', 'M' };
    unsigned size = 54 + W * H * 3 + (unsigned)((4 - W * 3 % 4) % 4) * H;
    h[2] = (unsigned char)size, h[3] = (unsigned char)(size >> 8), h[4] = (unsigned char)(size >> 16);
    h[10] = 54, h[14] = 40, h[18] = W & 255, h[19] = W >> 8, h[22] = H & 255, h[23] = H >> 8, h[26] = 1, h[28] = 24;
    fwrite(h, 1, 54, f);
    for (int y = H - 1; y >= 0; --y) {
        for (int x = 0; x < W; ++x) {
            u32 p = cv[y * W + x];
            unsigned char b[3] = { (unsigned char)p, (unsigned char)(p >> 8), (unsigned char)(p >> 16) };
            fwrite(b, 1, 3, f);
        }
        for (int i = 0; i < (4 - W * 3 % 4) % 4; ++i) fputc(0, f);
    }
    fclose(f);
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/tmp/sixtrack";
    game_init(7);
    /* the menu, then a 2-over match; the swing aimed at a band's centre (or off it) per ball */
    static const float aim[] = { 3.7f, 2.3f, 6.5f, 0.9f, 99, 3.95f, 5.1f, 7.6f, 2.3f, 3.7f, 0.9f, 2.3f };
    int frame = 0, shot = 0;
    char path[256];
    render();
    snprintf(path, sizeof path, "%s/menu.bmp", dir), bmp(path);
    input("w -40"); /* 5 Overs */
    input("k d 218");
    int aimed_ball = -1;
    for (int f = 0; f < 60 * 300 && g.state != S_END; ++f) {
        if (g.state == S_BALL && aimed_ball != g.balls) {
            float X, Y, d;
            delivery_at(g.t, &X, &Y, &d);
            float want = aim[g.balls % (int)(sizeof aim / sizeof *aim)];
            if (d <= want) swing(), aimed_ball = g.balls;
        }
        step(1 / 60.0f);
        render();
        if (f % 6 == 0) snprintf(path, sizeof path, "%s/f%04d.bmp", dir, frame++), bmp(path);
        (void)shot;
    }
    step(1), render();
    snprintf(path, sizeof path, "%s/end.bmp", dir), bmp(path);
    g.page = 1, render();
    snprintf(path, sizeof path, "%s/card.bmp", dir), bmp(path);
    printf("%d frames, %d/%d chasing %d\n", frame, g.runs, g.wkts, g.target);
    return 0;
}
#else
/* ---- the device: raw MIPS o32 syscalls (as Buddy's) ---- */

long sc(long n, long a, long b, long c, long d, long e, long f);
__asm__(".text\n.set noreorder\n.globl sc\nsc:\n"
        "move $2, $4\nmove $4, $5\nmove $5, $6\nmove $6, $7\n"
        "lw $7, 16($29)\nlw $8, 20($29)\nlw $9, 24($29)\n"
        "addiu $29, $29, -32\nsw $8, 16($29)\nsw $9, 20($29)\n"
        "syscall\naddiu $29, $29, 32\n"
        "beqz $7, 1f\nnop\nsubu $2, $0, $2\n1: jr $31\nnop\n"
        ".globl __start\n__start:\n"
        "lw $4, 0($29)\naddiu $5, $29, 4\nli $8, -8\nand $29, $29, $8\naddiu $29, $29, -32\n"
        "jal main\nnop\nmove $4, $2\nli $2, 4246\nsyscall\n1: b 1b\nnop\n.set reorder\n");

#define SYS_open 4005
#define SYS_unlink 4010
#define SYS_ioctl 4054
#define SYS_mmap 4090
#define SYS_bind 4169
#define SYS_recv 4175
#define SYS_socket 4183
#define SYS_poll 4188
#define SYS_clock_gettime 4263
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606
#define O_RDWR 2
#define O_CLOEXEC 0x80000
#define MSG_DONTWAIT 0x40

void *memset(void *p, int v, __SIZE_TYPE__ n) {
    volatile unsigned char *d = p;
    while (n--) *d++ = (unsigned char)v;
    return p;
}
void *memcpy(void *p, const void *s, __SIZE_TYPE__ n) {
    volatile unsigned char *d = p;
    const unsigned char *q = s;
    while (n--) *d++ = *q++;
    return p;
}

static long long now_ms(void) {
    long ts[2];
    sc(SYS_clock_gettime, 1, (long)ts, 0, 0, 0, 0); /* CLOCK_MONOTONIC */
    return (long long)ts[0] * 1000 + ts[1] / 1000000;
}

int main(int argc, char **argv) {
    (void)argc, (void)argv;
    unsigned var[40], fix[17];
    int fb = (int)sc(SYS_open, (long)"/dev/fb0", O_RDWR | O_CLOEXEC, 0, 0, 0, 0);
    if (fb < 0 || sc(SYS_ioctl, fb, FBIOGET_VSCREENINFO, (long)var, 0, 0, 0) ||
        sc(SYS_ioctl, fb, FBIOGET_FSCREENINFO, (long)fix, 0, 0, 0))
        return 1;
    unsigned line = fix[11] / 4, y0 = var[5], back = y0 ? 0 : W; /* demo's other page */
    /* the Q2's panel only: 320 x 375 portrait, 32 bpp, red at 16, two pages */
    if (var[0] != H || var[1] != W || var[3] < 2 * W || var[6] != 32 || var[8] != 16 || line < H ||
        2 * W * line * 4 > fix[5])
        return 1;
    u32 *mem = (u32 *)sc(SYS_mmap, 0, (long)fix[5], 3, 1, fb, 0); /* RW, MAP_SHARED */
    if ((long)mem < 0 && (long)mem > -4096) return 1;

    struct { unsigned short family; char path[108]; } addr = { 1, INPUT_SOCK }; /* AF_UNIX */
    int sock = (int)sc(SYS_socket, 1, 1, 0, 0, 0, 0); /* SOCK_DGRAM, MIPS numbering */
    sc(SYS_unlink, (long)INPUT_SOCK, 0, 0, 0, 0, 0);
    if (sock >= 0) sc(SYS_bind, sock, (long)&addr, sizeof addr, 0, 0, 0);

    game_init((u32)now_ms());
    long long last = now_ms(), next = last;
    while (!g.quit) {
        long long t = now_ms();
        int wait = next > t ? (int)(next - t) : 0;
        struct { int fd; short events, revents; } p = { sock, 1, 0 };
        sc(SYS_poll, (long)&p, sock >= 0, wait, 0, 0, 0);
        /* each message judged at its own moment: the game stepped up to now first */
        char msg[64];
        for (long n; sock >= 0 && (n = sc(SYS_recv, sock, (long)msg, sizeof msg - 1, MSG_DONTWAIT, 0, 0)) > 0;) {
            long long now = now_ms();
            float dt = (float)(int)(now - last) * 0.001f;
            if (dt > 0) step(minf(dt, 0.1f)), last = now;
            msg[n] = 0, input(msg);
        }
        t = now_ms();
        if (t < next) continue;
        step(minf((float)(int)(t - last) * 0.001f, 0.1f)), last = t;
        render();
        /* turned clockwise onto the portrait panel: view (x, y) is panel (H-1-y, x) */
        u32 *page = mem + back * line;
        for (int py = 0; py < W; ++py) {
            u32 *row = page + py * line;
            const u32 *src = cv + (H - 1) * W + py;
            for (int px = 0; px < H; ++px, src -= W) row[px] = *src;
        }
        var[5] = back;
        sc(SYS_ioctl, fb, FBIOPAN_DISPLAY, (long)var, 0, 0, 0);
        back = back ? 0 : W;
        next = t + (g.state == S_BALL || g.state == S_HIT ? 16 : 33); /* 60 a second while the ball is live, else 30 */
    }
    if (var[5] != y0) var[5] = y0, sc(SYS_ioctl, fb, FBIOPAN_DISPLAY, (long)var, 0, 0, 0);
    sc(SYS_unlink, (long)INPUT_SOCK, 0, 0, 0, 0, 0);
    return 0;
}
#endif
