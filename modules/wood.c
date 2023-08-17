#include "../ent.h"
#include "../entGetters.h"
#include "../handlerRegistrar.h"

static int wood_whoMoves(ent *a, ent *b, int axis, int dir) {
	if (type(b) & T_TERRAIN) {
		return MOVE_ME;
	}

	if (axis == 2) {
		return dir == -1 ? MOVE_ME : MOVE_HIM;
	}

	return MOVE_BOTH;
}

void module_wood() {
	whoMovesHandlers.reg("wood-whomoves", wood_whoMoves);
}
