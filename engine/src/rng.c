#include "lilpet/lilpet.h"

void lp_rng_seed(lp_rng *rng, uint32_t seed)
{
    rng->state = seed;
}

/* mulberry32 (public domain), unsigned 32-bit arithmetic wraps like Math.imul in the prototype */
uint32_t lp_rng_next_u32(lp_rng *rng)
{
    rng->state += 0x6d2b79f5u;
    uint32_t t = rng->state;
    t = (t ^ (t >> 15u)) * (t | 1u);
    t ^= t + (t ^ (t >> 7u)) * (t | 61u);
    return t ^ (t >> 14u);
}

float lp_rng_next(lp_rng *rng)
{
    return (float)(lp_rng_next_u32(rng) >> 8u) * (1.0f / 16777216.0f);
}
