#include "tinyscheme/scheme.h"
#include "tinyscheme/scheme-private.h"

#define numLayers 3

#define FRAMERATE 30

#define range(var, lim) for(int var = 0; var < lim; var++)

extern scheme *sc;

extern int32_t gravity;

extern void loadFile(const char* file);
