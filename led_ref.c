/* led_ref.c - host-side reference for the LED-matrix renderer.
 *
 *   ./led_ref --emit            print .rodata tables for ida.s
 *   ./led_ref --show STATE PATH  ASCII-render STATE, then after each move
 *                                PATH is like "R B' D2"
 *   ./led_ref --check           self-test: after any single move, every
 *                                face that the move does not touch, and the
 *                                turned face itself, must stay one color
 *
 * Model: 8 positions, 0 = UFL anchor (never moves), 1..7 = solver.c index+1.
 * cp[pos] = home position of the cubie now at pos; co[pos] = its twist.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { U, L, F, R, B, D };
static const char face_ch[6] = {'U', 'L', 'F', 'R', 'B', 'D'};

/* Sticker k of the corner at each position, k = 0 is the U/D sticker,
 * then clockwise as seen from outside the cube. */
static const uint8_t sticker_face[8][3] = {
    {U, F, L}, /* 0 UFL */
    {U, R, F}, /* 1 UFR */
    {D, F, R}, /* 2 DFR */
    {D, L, F}, /* 3 DFL */
    {U, B, R}, /* 4 UBR */
    {D, R, B}, /* 5 DBR */
    {D, B, L}, /* 6 DBL */
    {U, L, B}, /* 7 UBL */
};

/* solver.c source[]/twist[] widened to 8 entries (entry 0 = anchor). */
static const uint8_t src8[3][8] = {
    {0, 2, 5, 3, 1, 4, 6, 7}, /* R */
    {0, 1, 2, 3, 5, 6, 7, 4}, /* B */
    {0, 1, 3, 6, 4, 2, 5, 7}, /* D */
};
static const uint8_t twist8[3][8] = {
    {0, 1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

/* Net: face slot (fx, fy) and the position at TL, TR, BL, BR. */
static const uint8_t slot[6][2] = {
    [U] = {1, 0}, [L] = {0, 1}, [F] = {1, 1},
    [R] = {2, 1}, [B] = {3, 1}, [D] = {1, 2},
};
static const uint8_t cell_pos[6][4] = {
    [U] = {7, 4, 0, 1}, [L] = {7, 0, 6, 3}, [F] = {0, 1, 3, 2},
    [R] = {1, 4, 2, 5}, [B] = {4, 7, 5, 6}, [D] = {3, 2, 6, 5},
};

enum { W = 35, H = 25 };

typedef struct {
    uint8_t off_face[24]; /* which face the facelet shows */
    uint16_t off[24];     /* byte offset into the LED matrix */
    uint8_t pos[24], k[24];
} facelets_t;

static void build_facelets(facelets_t *t)
{
    int n = 0;
    for (int f = 0; f < 6; ++f)
        for (int c = 0; c < 4; ++c, ++n) {
            int i = c >> 1, j = c & 1;
            int x = 9 * slot[f][0] + 4 * j, y = 7 * slot[f][1] + 3 * i;
            int p = cell_pos[f][c], k = -1;
            for (int s = 0; s < 3; ++s)
                if (sticker_face[p][s] == f)
                    k = s;
            t->off_face[n] = (uint8_t) f;
            t->off[n] = (uint16_t) (4 * (W * y + x));
            t->pos[n] = (uint8_t) p;
            t->k[n] = (uint8_t) k;
        }
}

typedef struct {
    uint8_t cp[8], co[8];
} cube_t;

static void quarter_turn(cube_t *s, int face)
{
    cube_t r;
    for (int i = 0; i < 8; ++i) {
        int from = src8[face][i];
        r.cp[i] = s->cp[from];
        r.co[i] = (uint8_t) ((s->co[from] + twist8[face][i]) % 3);
    }
    *s = r;
}

/* Color (face index) shown by sticker k at position pos. */
static int sticker_color(const cube_t *s, int pos, int k)
{
    int idx = (k + s->co[pos]) % 3; /* sign verified by --check */
    return sticker_face[s->cp[pos]][idx];
}

static void render(const cube_t *s, const facelets_t *t, char fb[H][W])
{
    memset(fb, '.', (size_t) H * W);
    for (int n = 0; n < 24; ++n) {
        int c = face_ch[sticker_color(s, t->pos[n], t->k[n])];
        int base = t->off[n] / 4;
        for (int dy = 0; dy < 3; ++dy)
            for (int dx = 0; dx < 4; ++dx)
                fb[base / W + dy][base % W + dx] = (char) c;
    }
}

static void print_fb(char fb[H][W])
{
    for (int y = 0; y < 20; ++y) /* rows 20..24 unused */
        printf("%.*s\n", W, fb[y]);
    putchar('\n');
}

static int parse(const char *in, cube_t *s)
{
    if (strlen(in) != 14)
        return 0;
    s->cp[0] = 0;
    s->co[0] = 0;
    for (int i = 0; i < 7; ++i) {
        s->cp[i + 1] = (uint8_t) (in[i] - '1' + 1);
        s->co[i + 1] = (uint8_t) (in[i + 7] - '1');
    }
    return 1;
}

static int face_uniform(const cube_t *s, const facelets_t *t, int f)
{
    int first = -1;
    for (int n = 0; n < 24; ++n) {
        if (t->off_face[n] != f)
            continue;
        int c = sticker_color(s, t->pos[n], t->k[n]);
        if (first < 0)
            first = c;
        else if (c != first)
            return 0;
    }
    return first == f;
}

static int check(const facelets_t *t)
{
    static const int turned[3] = {R, B, D};
    static const int opposite[3] = {L, F, U};
    cube_t solved;
    parse("12345671111111", &solved);
    for (int f = 0; f < 6; ++f)
        if (!face_uniform(&solved, t, f))
            return 0;
    for (int face = 0; face < 3; ++face)
        for (int q = 1; q <= 3; ++q) {
            cube_t s = solved;
            for (int i = 0; i < q; ++i)
                quarter_turn(&s, face);
            if (!face_uniform(&s, t, turned[face]) ||
                !face_uniform(&s, t, opposite[face]))
                return 0;
        }
    /* R moves F stickers up into the U face's right column. */
    cube_t s = solved;
    quarter_turn(&s, 0);
    return sticker_color(&s, 1, 0) == F && sticker_color(&s, 4, 0) == F;
}

static void emit(const facelets_t *t)
{
    printf("# generated by led_ref --emit\n");
    printf("led_off:   # byte offset of each facelet's top-left pixel\n");
    for (int n = 0; n < 24; n += 12) {
        printf("    .half ");
        for (int i = n; i < n + 12; ++i)
            printf("%d%s", t->off[i], i + 1 < n + 12 ? ", " : "\n");
    }
    printf("led_pos:   # position 0..7 of each facelet\n    .byte ");
    for (int n = 0; n < 24; ++n)
        printf("%d%s", t->pos[n], n < 23 ? ", " : "\n");
    printf("led_k:     # sticker index 0..2 of each facelet\n    .byte ");
    for (int n = 0; n < 24; ++n)
        printf("%d%s", t->k[n], n < 23 ? ", " : "\n");
    printf("sticker_face: # [home position][k] -> face 0..5 (U L F R B D)\n");
    for (int p = 0; p < 8; ++p)
        printf("    .byte %d, %d, %d\n", sticker_face[p][0],
               sticker_face[p][1], sticker_face[p][2]);
    printf("src8:      # [face][dest] -> source position\n");
    for (int f = 0; f < 3; ++f) {
        printf("    .byte ");
        for (int i = 0; i < 8; ++i)
            printf("%d%s", src8[f][i], i < 7 ? ", " : "\n");
    }
    printf("twist8:\n");
    for (int f = 0; f < 3; ++f) {
        printf("    .byte ");
        for (int i = 0; i < 8; ++i)
            printf("%d%s", twist8[f][i], i < 7 ? ", " : "\n");
    }
    printf("    .align 2\n");
    printf("face_rgb:  # U white, L orange, F green, R red, B blue, D yellow\n");
    printf("    .word 0xFFFFFF, 0xFF8000, 0x00C000, 0xFF0000, 0x0000FF, "
           "0xFFFF00\n");
}

int main(int argc, char **argv)
{
    facelets_t t;
    build_facelets(&t);
    if (argc == 2 && !strcmp(argv[1], "--emit")) {
        emit(&t);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--check")) {
        int ok = check(&t);
        puts(ok ? "check OK" : "check FAILED");
        return !ok;
    }
    if (argc == 4 && !strcmp(argv[1], "--show")) {
        cube_t s;
        char fb[H][W];
        if (!parse(argv[2], &s))
            return 2;
        render(&s, &t, fb);
        print_fb(fb);
        for (const char *m = argv[3]; *m;) {
            if (*m == ' ') {
                ++m;
                continue;
            }
            int face = *m == 'R' ? 0 : *m == 'B' ? 1 : 2;
            int q = m[1] == '2' ? 2 : m[1] == '\'' ? 3 : 1;
            for (int i = 0; i < q; ++i)
                quarter_turn(&s, face);
            printf("after %.*s\n", m[1] == '2' || m[1] == '\'' ? 2 : 1, m);
            render(&s, &t, fb);
            print_fb(fb);
            m += (m[1] == '2' || m[1] == '\'') ? 2 : 1;
        }
        return 0;
    }
    fprintf(stderr, "usage: %s --emit | --check | --show STATE PATH\n",
            argv[0]);
    return 2;
}
