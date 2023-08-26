#pragma once
#include <stdint.h>

#define range(var, lim) for(int var = 0; var < lim; var++)

extern char getNum(const char **c, int32_t *out);
