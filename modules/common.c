#include "../handlerRegistrar.h"
#include "../entUpdaters.h"

static int move_me(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static void cursed_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

void module_common() {
	whoMovesHandlers.reg(WHOMOVES_ME, move_me);
	tickHandlers.reg(TICK_CURSED, cursed_tick);
}
