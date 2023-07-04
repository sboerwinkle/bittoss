#include "../handlerRegistrar.h"

static int move_me(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

void common_init() {
	whoMovesHandlers.reg("move-me", move_me);
}
