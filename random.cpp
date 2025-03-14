#include <stdint.h>

// Should be the same as std::minstd_rand,
// just reimplemented it so I have access to the internal state
extern const int32_t randomMax = 2147483647; // 2^31 - 1

void stepRand(int32_t* x) {
	*x = (*x * (int64_t)48271) % randomMax;
}

// Not sure where this originates from, but I got it from this StackOverflow answer:
// https://stackoverflow.com/questions/17035441/looking-for-decent-quality-prng-with-only-32-bits-of-state/52056161#52056161
// I only use this the once I think - I needed a new "sequence" from the main gamestate RNG that wouldn't
// obviously re-use the main sequence but is determined by gamestate RNG state, so I run a random number
// from the main sequence through one step of this to get a new seed for `stepRand`.
// There are probably better ways to do this - I think maybe splitmix32 supports this on its own -
// but this is plenty good enough.
uint32_t splitmix32(uint32_t *state) {
    uint32_t z = (*state += 0x9e3779b9);
    z ^= z >> 16; z *= 0x21f0aaad;
    z ^= z >> 15; z *= 0x735a2d97;
    z ^= z >> 15;
    return z;
}
