#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "ground.h"

const int32_t groundSize[3] = {3200, 3200, 500};

ent* mkGround(gamestate *gs, int32_t *pos, int32_t color) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs, NULL,
		pos, vel, groundSize,
		0,
		T_TERRAIN + T_HEAVY + T_WEIGHTLESS, 0
	);
	ret->color = color;
	return ret;
}

void module_ground() {
}
