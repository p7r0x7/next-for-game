// rand.h - sfc32 + pcg32, seeded, batch-filled as doubles. Header-only.
#ifndef NEXT_RAND_H
#define NEXT_RAND_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXT_U32_TO_UNIT 0x1p-32

/* ---------------- sfc32 ---------------- */
typedef struct { uint32_t a, b, c, d; } sfc32_t;

static inline uint32_t sfc32_next(sfc32_t* s)
{
    uint32_t t = s->a + s->b + s->d;
    s->d += 1;
    s->a = s->b ^ (s->b >> 9);
    s->b = s->c + (s->c << 3);
    s->c = ((s->c << 21) | (s->c >> 11)) + t;
    return t;
}

static inline void sfc32_seed3(sfc32_t* s, uint32_t a, uint32_t b, uint32_t c)
{
    s->a = a; s->b = b; s->c = c; s->d = 1;
    for (int i = 0; i < 12; ++i) sfc32_next(s); /* warm-up */
}

static inline void sfc32_seed(sfc32_t* s, uint32_t seed) { sfc32_seed3(s, 0, seed, seed); }

/* raw: uint32 value as double. unit: [0,1). */
static inline void sfc32_fill_f64(sfc32_t* s, double* out, size_t n)
{
    sfc32_t r = *s;
    for (size_t i = 0; i < n; ++i) out[i] = (double)sfc32_next(&r);
    *s = r;
}
static inline void sfc32_fill_unit(sfc32_t* s, double* out, size_t n)
{
    sfc32_t r = *s;
    for (size_t i = 0; i < n; ++i) out[i] = (double)sfc32_next(&r) * NEXT_U32_TO_UNIT;
    *s = r;
}

/* ---------------- pcg32 (XSH-RR, fixed stream) ---------------- */
typedef struct { uint64_t state; } pcg32_t;

#define PCG32_MULT 6364136223846793005ULL
#define PCG32_INC  1442695040888963407ULL /* odd */

static inline uint32_t pcg32_next(pcg32_t* p)
{
    uint64_t old = p->state;
    p->state = old * PCG32_MULT + PCG32_INC;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

static inline void pcg32_seed(pcg32_t* p, uint64_t seed)
{
    p->state = 0;
    pcg32_next(p);
    p->state += seed;
    pcg32_next(p);
}

static inline void pcg32_fill_f64(pcg32_t* p, double* out, size_t n)
{
    pcg32_t r = *p;
    for (size_t i = 0; i < n; ++i) out[i] = (double)pcg32_next(&r);
    *p = r;
}
static inline void pcg32_fill_unit(pcg32_t* p, double* out, size_t n)
{
    pcg32_t r = *p;
    for (size_t i = 0; i < n; ++i) out[i] = (double)pcg32_next(&r) * NEXT_U32_TO_UNIT;
    *p = r;
}

#ifdef __cplusplus
}
#endif
#endif
