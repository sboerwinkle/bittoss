#include "tinyscheme/scheme.h"
#include "tinyscheme/scheme-private.h"

#define numLayers 3

#define PTS_PER_PX 32
#define FRAMERATE 30

extern scheme *sc;

extern int32_t gravity;

extern void rect(int32_t *p, int32_t *radius, float r, float g, float b);

extern void loadFile(const char* file);
