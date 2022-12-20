#include <cstdint>
#include "tinyscheme/scheme.h"
#include "tinyscheme/scheme-private.h"

#define numLayers 3

#define FRAMERATE 30

#define range(var, lim) for(int var = 0; var < lim; var++)

extern scheme *sc;

typedef uint_fast32_t rand_t;
extern rand_t random_max;
extern rand_t get_random();

extern int32_t gravity;

extern void loadFile(const char* file);
