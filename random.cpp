#include <stdint.h>

// Should be the same as std::minstd_rand,
// just reimplemented it so I have access to the internal state
extern const int32_t randomMax = 2147483647; // 2^31 - 1

void stepRand(int32_t* x) {
	*x = (*x * (int64_t)48271) % randomMax;
}
