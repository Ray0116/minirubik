/* C reference for the Stage 2 solver: IDA* over (p, o) with
 * h = max(perm_dist[p], orient_dist[o]) and same-face pruning.
 * The search uses an explicit stack (no recursion) so it maps directly
 * onto RV32I. Tables come from tables.h (gen_tables --header).
 *
 *   ./ida PPPPPPPOOOOOOO   solve one state, print moves (like solver)
 *   ./ida -v PPPPPPPOOOOOOO  same, plus node counts on stderr
 *   ./ida --hard           solve the 2,644 distance-11 states (fast)
 *   ./ida --all            solve all 3,674,160 states, check optimality (H3, slow)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    MAX_DEPTH = 11
};

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};

static uint32_t expanded;  /* nodes whose children were generated */
static uint32_t generated; /* child states produced (one per move tried) */

static inline uint8_t heuristic(uint16_t p, uint16_t o)
{
    uint8_t hp = perm_dist[p], ho = orient_dist[o];
    return hp > ho ? hp : ho;
}

static int ida(uint16_t p0, uint16_t o0, uint8_t path[MAX_DEPTH])
{
    uint16_t sp[MAX_DEPTH + 1], so[MAX_DEPTH + 1];
    uint16_t tp[MAX_DEPTH + 1], to[MAX_DEPTH + 1];
    uint8_t next[MAX_DEPTH + 1];

    expanded = 0;
    generated = 0;

    if (p0 == 0 && o0 == 0)
        return 0;

    for (int bound = heuristic(p0, o0); bound <= MAX_DEPTH; ++bound) {
        int d = 0;
        sp[0] = p0;
        so[0] = o0;
        next[0] = 0;
        ++expanded;
        while (d >= 0) {
            if (next[d] == MOVES) { /* all moves tried: backtrack */
                --d;
                continue;
            }
            uint8_t m = next[d]++;
            uint8_t face = m / 3, turn = m % 3;

            /* Same-face pruning: never turn the face the parent just turned. */
            if (d > 0 && face == path[d - 1] / 3) {
                next[d] = (uint8_t) (face * 3 + 3);
                continue;
            }
            if (turn == 0) {
                tp[d] = sp[d];
                to[d] = so[d];
            }
            tp[d] = perm_trans[face][tp[d]];
            to[d] = orient_trans[face][to[d]];
            ++generated;

            uint8_t h = heuristic(tp[d], to[d]);
            if (d + 1 + h > bound)
                continue;
            path[d] = m;
            if (h == 0) /* h == 0 only at the solved state */
                return d + 1;
            ++d;
            sp[d] = tp[d - 1];
            so[d] = to[d - 1];
            next[d] = 0;
            ++expanded;
        }
    }
    return -1;
}

/* ---- Input parsing and ranking, copied from solver.c ---- */

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

static void rank_pair(const state_t *state, uint16_t *pr, uint16_t *or)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    *pr = (uint16_t) p;
    *or = (uint16_t) o;
}

/* ---- Verification over every state ---- */

/* Apply a move sequence with the transition tables; return 1 if solved. */
static int solves(uint16_t p, uint16_t o, const uint8_t *path, int len)
{
    for (int i = 0; i < len; ++i) {
        uint8_t face = path[i] / 3;
        for (int t = 0; t <= path[i] % 3; ++t) {
            p = perm_trans[face][p];
            o = orient_trans[face][o];
        }
    }
    return p == 0 && o == 0;
}

static int run_all(int only_hardest)
{
    /* True distances by full BFS, as in gen_tables --check. */
    uint8_t *dist = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint32_t head = 0, tail = 0;
    if (!dist || !queue) {
        fputs("out of memory\n", stderr);
        return 1;
    }
    memset(dist, UINT8_MAX, STATES);
    dist[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (int f = 0; f < 3; ++f) {
            uint16_t np = p, no = o;
            for (int t = 0; t < 3; ++t) {
                np = perm_trans[f][np];
                no = orient_trans[f][no];
                uint32_t there = (uint32_t) np * ORIENTATIONS + no;
                if (dist[there] == UINT8_MAX) {
                    dist[there] = (uint8_t) (dist[here] + 1);
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);

    uint32_t worst_exp[MAX_DEPTH + 1] = {0}, worst_state[MAX_DEPTH + 1] = {0};
    uint64_t sum_exp[MAX_DEPTH + 1] = {0};
    uint32_t worst_gen[MAX_DEPTH + 1] = {0};
    uint32_t count[MAX_DEPTH + 1] = {0}, failures = 0;
    uint8_t path[MAX_DEPTH];

    for (uint32_t s = 0; s < STATES; ++s) {
        uint16_t p = (uint16_t) (s / ORIENTATIONS);
        uint16_t o = (uint16_t) (s % ORIENTATIONS);
        uint8_t d = dist[s];
        if (only_hardest && d != MAX_DEPTH)
            continue;
        int len = ida(p, o, path);
        if (len != d || !solves(p, o, path, len)) {
            if (failures < 10)
                printf("FAIL: state %u, ida length %d, true distance %u\n",
                       s, len, d);
            ++failures;
            continue;
        }
        ++count[d];
        sum_exp[d] += expanded;
        if (generated > worst_gen[d])
            worst_gen[d] = generated;
        if (expanded > worst_exp[d]) {
            worst_exp[d] = expanded;
            worst_state[d] = s;
        }
    }
    free(dist);

    printf("IDA* over %s\n", only_hardest ? "the distance-11 states"
                                           : "all 3,674,160 states");
    printf("failures (wrong length or not solved): %u\n\n", failures);
    printf(" d     states   mean expanded    max expanded   max generated   worst state\n");
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        if (!count[d])
            continue;
        printf("%2d %10u %15.1f %15u %15u %13u\n", d, count[d],
               (double) sum_exp[d] / count[d], worst_exp[d], worst_gen[d],
               worst_state[d]);
    }
    if (!only_hardest)
        printf("\n%s\n", failures ? "H3 FAILED" : "H3 PASSED");
    else
        printf("\n%s\n", failures ? "FAILED" : "all distance-11 states solved optimally");
    return failures != 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--all"))
        return run_all(0);
    if (argc == 2 && !strcmp(argv[1], "--hard"))
        return run_all(1);

    int verbose = argc == 3 && !strcmp(argv[1], "-v");
    const char *input = argc == 2 ? argv[1] : verbose ? argv[2] : NULL;
    state_t state;
    if (!input || !parse_state(input, &state)) {
        fprintf(stderr, "usage: %s [-v] PPPPPPPOOOOOOO | --hard | --all\n",
                argc > 0 && argv[0] ? argv[0] : "ida");
        return 2;
    }

    uint16_t p, o;
    uint8_t path[MAX_DEPTH];
    rank_pair(&state, &p, &o);
    int len = ida(p, o, path);
    if (len < 0) {
        fputs("no solution found\n", stderr);
        return 1;
    }
    for (int i = 0; i < len; ++i)
        printf("%s%s", i ? " " : "", move_names[path[i]]);
    putchar('\n');
    if (verbose)
        fprintf(stderr, "length %d, h(root) %u, expanded %u, generated %u\n",
                len, heuristic(p, o), expanded, generated);
    return fflush(stdout) != 0 || ferror(stdout);
}