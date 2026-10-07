#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

/* Copied from solver.c. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* Transition tables, kept global so the BFS and output code can use them. */
static uint16_t permutation[3][PERMUTATIONS];
static uint16_t orientation[3][ORIENTATIONS];

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

/* Copied from solver.c. */
static uint32_t rank_state(const state_t *state)
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
    return p * ORIENTATIONS + o;
}

/* Copied from solver.c. */
static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

/* The two loops from the start of build_table() in solver.c. */
static void build_transitions(void)
{
    state_t state;
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
}

/* Sanity check: four quarter-turns of the same face return to the start. */
static int check_transitions(void)
{
    for (uint8_t face = 0; face < 3; ++face) {
        for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
            uint16_t x = p;
            for (int t = 0; t < 4; ++t)
                x = permutation[face][x];
            if (x != p)
                return 0;
        }
        for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
            uint16_t x = o;
            for (int t = 0; t < 4; ++t)
                x = orientation[face][x];
            if (x != o)
                return 0;
        }
    }
    return 1;
}

/* Distance tables: fewest moves to solve only the permutation / orientation. */
static uint8_t perm_dist[PERMUTATIONS];
static uint8_t orient_dist[ORIENTATIONS];

/* BFS from rank 0 over one projection.
 * trans[face][rank] is the quarter-turn transition; n is the number of ranks.
 * Each of the 9 HTM moves is 1, 2 or 3 quarter-turns of one face.
 * Returns the largest distance found, or -1 if some rank was never reached.
 */
static int bfs(const uint16_t *trans, int n, uint8_t *dist)
{
    uint16_t queue[PERMUTATIONS];
    int head = 0, tail = 0, max = 0;
    for (int i = 0; i < n; ++i)
        dist[i] = UINT8_MAX;
    dist[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint16_t here = queue[head++];
        for (int face = 0; face < 3; ++face) {
            uint16_t next = here;
            for (int turn = 0; turn < 3; ++turn) {
                next = trans[face * n + next];
                if (dist[next] == UINT8_MAX) {
                    dist[next] = (uint8_t) (dist[here] + 1);
                    if (dist[next] > max)
                        max = dist[next];
                    queue[tail++] = next;
                }
            }
        }
    }
    return tail == n ? max : -1;
}

/* Print a byte table as assembler directives, 16 values per line. */
static void print_bytes(const char *label, const uint8_t *t, int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; ++i)
        printf("%s%u%s", i % 16 ? "" : "    .byte ", t[i],
               i % 16 == 15 || i == n - 1 ? "\n" : ", ");
}

/* Print a halfword table (one face after another) as .half directives. */
static void print_halves(const char *label, const uint16_t *t, int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; ++i)
        printf("%s%u%s", i % 12 ? "" : "    .half ", t[i],
               i % 12 == 11 || i == n - 1 ? "\n" : ", ");
}

/* Print all four tables as C arrays, so the C IDA* reads the same data. */
static void print_c_header(void)
{
    printf("/* Generated by gen_tables.c --header. Do not edit by hand. */\n");
    printf("#include <stdint.h>\n\n");
    printf("static const uint16_t perm_trans[3][%d] = {\n", PERMUTATIONS);
    for (int f = 0; f < 3; ++f) {
        printf("  {");
        for (int i = 0; i < PERMUTATIONS; ++i)
            printf("%s%u", i ? (i % 16 ? "," : ",\n   ") : "", permutation[f][i]);
        printf("},\n");
    }
    printf("};\n\nstatic const uint16_t orient_trans[3][%d] = {\n", ORIENTATIONS);
    for (int f = 0; f < 3; ++f) {
        printf("  {");
        for (int i = 0; i < ORIENTATIONS; ++i)
            printf("%s%u", i ? (i % 16 ? "," : ",\n   ") : "", orientation[f][i]);
        printf("},\n");
    }
    printf("};\n\nstatic const uint8_t perm_dist[%d] = {\n  ", PERMUTATIONS);
    for (int i = 0; i < PERMUTATIONS; ++i)
        printf("%s%u", i ? (i % 32 ? "," : ",\n  ") : "", perm_dist[i]);
    printf("\n};\n\nstatic const uint8_t orient_dist[%d] = {\n  ", ORIENTATIONS);
    for (int i = 0; i < ORIENTATIONS; ++i)
        printf("%s%u", i ? (i % 32 ? "," : ",\n  ") : "", orient_dist[i]);
    printf("\n};\n");
}

/* H1: check that h = max(perm_dist[p], orient_dist[o]) never exceeds the
 * true distance, for every one of the 3,674,160 states.
 * The true distances come from a full BFS over the combined state space,
 * the same search solver.c runs, but storing distances instead of moves.
 */
enum { STATES = PERMUTATIONS * ORIENTATIONS };

static int check_admissible(void)
{
    uint8_t *dist = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint32_t head = 0, tail = 0;
    if (!dist || !queue) {
        free(dist);
        free(queue);
        fputs("out of memory\n", stderr);
        return 0;
    }
    memset(dist, UINT8_MAX, STATES);
    dist[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (int face = 0; face < 3; ++face) {
            uint16_t np = p, no = o;
            for (int turn = 0; turn < 3; ++turn) {
                np = permutation[face][np];
                no = orientation[face][no];
                uint32_t there = (uint32_t) np * ORIENTATIONS + no;
                if (dist[there] == UINT8_MAX) {
                    dist[there] = (uint8_t) (dist[here] + 1);
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(dist);
        fputs("full BFS did not reach every state\n", stderr);
        return 0;
    }

    /* count[d][h]: how many states have true distance d and heuristic h. */
    static uint32_t count[12][12];
    uint32_t violations = 0, exact = 0;
    uint64_t gap_sum = 0;
    for (uint32_t s = 0; s < STATES; ++s) {
        uint8_t hp = perm_dist[s / ORIENTATIONS];
        uint8_t ho = orient_dist[s % ORIENTATIONS];
        uint8_t h = hp > ho ? hp : ho;
        uint8_t d = dist[s];
        if (h > d) {
            if (violations < 10)
                printf("VIOLATION: state %u has h = %u > d = %u\n", s, h, d);
            ++violations;
            continue;
        }
        ++count[d][h];
        gap_sum += (uint64_t) (d - h);
        if (h == d)
            ++exact;
    }
    free(dist);

    printf("H1 admissibility check over %u states\n", (unsigned) STATES);
    printf("violations (h > d): %u\n", violations);
    printf("h == d exactly:     %u (%.2f%%)\n", exact, 100.0 * exact / STATES);
    printf("mean d - h:         %.3f\n\n", (double) gap_sum / STATES);
    printf(" d \\ h");
    for (int h = 0; h <= 7; ++h)
        printf(" %8d", h);
    printf("    states   mean h\n");
    for (int d = 0; d <= 11; ++d) {
        uint32_t total = 0;
        uint64_t hsum = 0;
        for (int h = 0; h <= 11; ++h) {
            total += count[d][h];
            hsum += (uint64_t) h * count[d][h];
        }
        printf("%5d ", d);
        for (int h = 0; h <= 7; ++h)
            printf(" %8u", count[d][h]);
        printf(" %9u %8.2f\n", total, total ? (double) hsum / total : 0.0);
    }
    printf("\n%s\n", violations ? "H1 FAILED" : "H1 PASSED");
    return violations == 0;
}

int main(int argc, char **argv)
{
    build_transitions();
    if (!check_transitions()) {
        fputs("transition check failed\n", stderr);
        return 1;
    }
    int pmax = bfs(&permutation[0][0], PERMUTATIONS, perm_dist);
    int omax = bfs(&orientation[0][0], ORIENTATIONS, orient_dist);
    if (pmax < 0 || omax < 0) {
        fputs("distance table incomplete\n", stderr);
        return 1;
    }
    /* Summary goes to stderr so stdout stays a clean .s file. */
    fprintf(stderr, "transition tables OK\n");
    fprintf(stderr, "perm_dist:   %d entries, solved = %u, max = %d\n",
            PERMUTATIONS, perm_dist[0], pmax);
    fprintf(stderr, "orient_dist: %d entries, solved = %u, max = %d\n",
            ORIENTATIONS, orient_dist[0], omax);

    if (argc == 2 && !strcmp(argv[1], "--check"))
        return check_admissible() ? 0 : 1;
    if (argc == 2 && !strcmp(argv[1], "--header")) {
        print_c_header();
        return 0;
    }

    printf("# Generated by gen_tables.c. Do not edit by hand.\n");
    printf("    .section .rodata\n");
    print_halves("perm_trans", &permutation[0][0], 3 * PERMUTATIONS);
    print_halves("orient_trans", &orientation[0][0], 3 * ORIENTATIONS);
    print_bytes("perm_dist", perm_dist, PERMUTATIONS);
    print_bytes("orient_dist", orient_dist, ORIENTATIONS);
    return 0;
}