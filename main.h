#include <cstdint>
#include "tinyscheme/scheme.h"
#include "tinyscheme/scheme-private.h"

#define FRAMERATE 30

extern scheme *sc;

typedef uint_fast32_t rand_t;
extern rand_t random_max;
extern rand_t get_random();

extern int32_t gravity;

extern void loadFile(const char* file);
