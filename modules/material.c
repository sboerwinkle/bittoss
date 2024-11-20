#include "../ent.h"
#include "../entGetters.h"
#include "../handlerRegistrar.h"

static int paper_whoMoves(ent *a, ent *b, int axis, int dir) {
	if (dir == 1 && axis == 2 && !(type(b) & (T_TERRAIN | T_HEAVY))) return MOVE_BOTH;
	return MOVE_ME;
}

static int wood_whoMoves(ent *a, ent *b, int axis, int dir) {
	if (type(b) & T_TERRAIN) {
		return MOVE_ME;
	}

	if (axis == 2) {
		return dir == -1 ? MOVE_ME : MOVE_HIM;
	}

	return MOVE_BOTH;
}

static int metal_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_HEAVY) ? MOVE_ME : MOVE_HIM;
}

void module_material() {
	whoMovesHandlers.reg(WHOMOVES_PAPER, paper_whoMoves);
	whoMovesHandlers.reg(WHOMOVES_WOOD, wood_whoMoves);
	whoMovesHandlers.reg(WHOMOVES_METAL, metal_whoMoves);
}
