#pragma once

#include <cstdint>
#include "ent.h"

#define FRAMERATE 30

void showMessage(gamestate const * const gs, char const * const msg);

extern char globalRunning;
extern int32_t gravity;
