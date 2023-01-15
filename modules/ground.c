#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

const int32_t groundSize[3] = {3200, 3200, 512};

ent* mkGround(int32_t *pos, draw_t d) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		pos, vel, groundSize,
		0, 0,
		T_TERRAIN + T_HEAVY + T_WEIGHTLESS, 0
	);
	ret->draw = d;
	return ret;
}

void ground_init() {
	loadFile("modules/ground.scm");
}
