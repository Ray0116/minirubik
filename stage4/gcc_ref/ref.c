/* gcc -O2 reference build of the final C algorithm (stage3/ida_v1.c),
 * freestanding for Ripes: no libc, ecall for output.
 * Same tests and same output as stage4/ida.s.
 * No *, /, % on variables, so gcc cannot call __mulsi3/__divsi3.
 */
#include <stdint.h>
#include "tables.h"

#define MAX_DEPTH 11

static uint8_t path_face[MAX_DEPTH + 1], path_turn[MAX_DEPTH + 1];

static const char *const move_name[9] = {"R", "R2", "R'", "B", "B2",
                                         "B'", "D", "D2", "D'"};

/* ---- Ripes ecalls ---- */
static void print_str(const char *s)
{
    register const char *a0 asm("a0") = s;
    register int a7 asm("a7") = 4;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}
static void print_char(int c)
{
    register int a0 asm("a0") = c;
    register int a7 asm("a7") = 11;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}
static void exit0(void)
{
    register int a7 asm("a7") = 10;
    asm volatile("ecall" : : "r"(a7) : "memory");
    for (;;) {}
}

static inline unsigned heuristic(unsigned p, unsigned o)
{
    unsigned hp = perm_dist[p], ho = orient_dist[o];
    return hp > ho ? hp : ho;
}

/* ---- IDA* (explicit stack, same-face pruning) ---- */
static int ida(unsigned p0, unsigned o0)
{
    uint16_t st_p[MAX_DEPTH + 1], st_o[MAX_DEPTH + 1];
    uint16_t tp[MAX_DEPTH + 1], to[MAX_DEPTH + 1];
    uint8_t face[MAX_DEPTH + 1], turn[MAX_DEPTH + 1];

    if (p0 == 0 && o0 == 0)
        return 0;

    for (int bound = heuristic(p0, o0); bound <= MAX_DEPTH; ++bound) {
        int d = 0;
        st_p[0] = p0;
        st_o[0] = o0;
        face[0] = 0;
        turn[0] = 0;
        while (d >= 0) {
            if (face[d] >= 3) {            /* all moves tried: backtrack */
                --d;
                continue;
            }
            unsigned f = face[d], t = turn[d];
            if (d > 0 && f == path_face[d - 1]) {   /* same-face pruning */
                face[d] = f + 1;
                turn[d] = 0;
                continue;
            }
            if (t == 0) {
                tp[d] = st_p[d];
                to[d] = st_o[d];
            }
            tp[d] = perm_trans[f][tp[d]];
            to[d] = orient_trans[f][to[d]];
            unsigned h = heuristic(tp[d], to[d]);

            if (t + 1 == 3) {
                turn[d] = 0;
                face[d] = f + 1;
            } else {
                turn[d] = t + 1;
            }
            if (d + 1 + (int) h > bound)
                continue;
            path_face[d] = f;
            path_turn[d] = t;
            if (h == 0)
                return d + 1;
            st_p[d + 1] = tp[d];
            st_o[d + 1] = to[d];
            face[d + 1] = 0;
            turn[d + 1] = 0;
            ++d;
        }
    }
    return -1;
}

/* ---- "PPPPPPPOOOOOOO" -> (p, o) ----
 * Horner form p = p*(7-i) + smaller_i, written with constant multipliers
 * so gcc emits shifts/adds instead of calling __mulsi3. (A repeated-add
 * loop is recognized by gcc as a multiply and turned into __mulsi3.)
 */
static unsigned smaller(const char *s, int i)
{
    unsigned n = 0;
    for (int j = i + 1; j < 7; ++j)
        if (s[j] < s[i])
            ++n;
    return n;
}

static void rank_input(const char *s, unsigned *pp, unsigned *po)
{
    unsigned p = smaller(s, 0);
    p = p * 6 + smaller(s, 1);
    p = p * 5 + smaller(s, 2);
    p = p * 4 + smaller(s, 3);
    p = p * 3 + smaller(s, 4);
    p = p * 2 + smaller(s, 5);
    unsigned o = 0;
    for (int i = 0; i < 6; ++i)
        o = o * 3 + (unsigned) (s[7 + i] - '1');
    *pp = p;
    *po = o;
}

static int verify(unsigned p, unsigned o, int len)
{
    for (int i = 0; i < len; ++i) {
        unsigned f = path_face[i];
        for (int t = 0; t <= path_turn[i]; ++t) {
            p = perm_trans[f][p];
            o = orient_trans[f][o];
        }
    }
    return p == 0 && o == 0;
}

/* ---- self-checking tests, same as ida.s ---- */
static const char *const tests[] = {"54721631111111", "12345671111111",
                                    "25314672313211", "21345671111111"};
static const int expect[] = {-1, 0, 1, 11};

int main(void)
{
    for (int k = 0; k < 4; ++k) {
        unsigned p0, o0;
        rank_input(tests[k], &p0, &o0);
        int len = ida(p0, o0);
        for (int i = 0; i < len; ++i) {
            unsigned idx = (path_face[i] << 1) + path_face[i] + path_turn[i];
            print_str(move_name[idx]);
            print_char(' ');
        }
        print_char('\n');
        int ok = (expect[k] < 0 || len == expect[k]) && verify(p0, o0, len);
        print_str(ok ? "PASS\n" : "FAIL\n");
    }
    exit0();
    return 0;
}

void __attribute__((naked, section(".text.start"))) _start(void)
{
    asm volatile("call main\n");
}