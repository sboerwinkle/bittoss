#pragma once

#include <cstdint>
#include "ent.h"

void showMessage(gamestate const * const gs, char const * const msg);
void requestReload(gamestate const * const gs);

extern char globalRunning;
extern int32_t gravity;
