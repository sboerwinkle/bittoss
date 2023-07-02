#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "platform.h"

static int platform_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_HIM;
}

const int32_t platformSize[3] = {3200, 3200, 512};

ent* mkPlatform(gamestate *gs, ent *owner, int32_t *offset, draw_t d) {
	int32_t pos[3];
	range(i, 3) pos[i] = offset[i] + owner->center[i];
	ent *e = initEnt(
		gs, owner,
		pos, owner->vel, platformSize,
		0,
		T_TERRAIN + T_HEAVY + T_WEIGHTLESS, T_TERRAIN
	);
	e->whoMoves = whoMovesHandlers.getByName("platform-whomoves");
	e->draw = d;
	return e;
}

void platform_init() {
	whoMovesHandlers.reg("platform-whomoves", platform_whoMoves);
}
