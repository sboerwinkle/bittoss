#include "../ent.h"
#include "../entGetters.h"
#include "../handlerRegistrar.h"

static int metal_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_HEAVY) ? MOVE_ME : MOVE_HIM;
}

void module_metal() {
	whoMovesHandlers.reg(WHOMOVES_METAL, metal_whoMoves);
}
