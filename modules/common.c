#include "../handlerRegistrar.h"

static int move_me(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

void module_common() {
	whoMovesHandlers.reg("move-me", move_me);
}
