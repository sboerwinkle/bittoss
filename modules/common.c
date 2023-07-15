#include "../handlerRegistrar.h"

static int move_me(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static int drop_on_pushed(gamestate *gs, ent *a, ent *b, int axis, int dir, int dx, int db) {
	return r_drop;
}

void module_common() {
	whoMovesHandlers.reg("move-me", move_me);
	pushedHandlers.reg("drop-on-pushed", drop_on_pushed);
}
