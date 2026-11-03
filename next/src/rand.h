#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <bcrypt.h>   /* link bcrypt.lib / -lbcrypt */
#else
    #include <sys/random.h>   /* macOS + Linux (glibc 2.25+) */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------- entropy -------------------------------- */

static inline int entropy(char* dst, int n) {
#ifdef _WIN32
    return BCryptGenRandom(NULL, (PUCHAR)dst, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    return getentropy(dst, (size_t)n);  /* n <= 256 */
#endif
}

/* ---------------------- pcg32 (XSH-RR, fixed stream) --------------------- */

typedef struct { uint64_t state; } pcg32_t;

#define PCG32_MULT 6364136223846793005ULL
#define PCG32_INC  1442695040888963407ULL

static inline uint32_t pcg32_next(pcg32_t* p) {
    uint64_t old = p->state;
    p->state = old * PCG32_MULT + PCG32_INC;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}
static inline void pcg32_seed(pcg32_t* p, const char* seed8bytes) {
    uint64_t seed;
    memcpy(&seed, seed8bytes, 8);
    p->state = 0; pcg32_next(p); p->state += seed; pcg32_next(p);
}

static inline void pcg32_doubles(pcg32_t* p, double* dst, int n) {
    pcg32_t r = *p;
    for (int i = 0; i < n; ++i) dst[i] = (double)pcg32_next(&r);
    *p = r;
}

#ifdef __cplusplus
}
#endif

/* --------------------------------- sfc32 --------------------------------- */

typedef struct { uint32_t a, b, c, d; } sfc32_t;

static inline uint32_t sfc32_next(sfc32_t* s) {
    uint32_t t = s->a + s->b + s->d;
    s->d += 1;
    s->a = s->b ^ (s->b >> 9);
    s->b = s->c + (s->c << 3);
    s->c = ((s->c << 21) | (s->c >> 11)) + t;
    return t;
}
static inline void sfc32_seed(sfc32_t* s, const char* seed12bytes) {
    memcpy(&s->a, seed12bytes, 4);
    memcpy(&s->b, seed12bytes + 4, 4);
    memcpy(&s->c, seed12bytes + 8, 4);
    s->d = 1; for (int i = 0; i < 12; ++i) sfc32_next(s);
}

static inline void sfc32_doubles(sfc32_t* s, double* dst, int n) {
    sfc32_t r = *s;
    for (int i = 0; i < n; ++i) dst[i] = (double)sfc32_next(&r);
    *s = r;
}
