#include <string.h>

#include "main.h"
#include "ent.h"
#include "util.h"

#include "handlerRegistrar.h"

catalog<whoMoves_t> whoMovesHandlers;
catalog<tick_t> tickHandlers;
catalog<crush_t> crushHandlers;
catalog<pushed_t> pushedHandlers;
catalog<push_t> pushHandlers;
catalog<entPair_t> entPairHandlers;

static char onPushDefault(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	return 0;
}

static void entPairDefault(gamestate *gs, ent *me, ent *him) {}

void init_registrar() {
	whoMovesHandlers.init(WHOMOVES_NUM);
	tickHandlers.init(TICK_NUM);
	crushHandlers.init(CRUSH_NUM);
	pushedHandlers.init(PUSHED_NUM);
	pushHandlers.init(PUSH_NUM);
	entPairHandlers.init(ENTPAIR_NUM);

	// All the entries are initialized to NULL,
	// we only have to update the NIL entry if it's
	// something else.
	// Serialization assumes that the default handlers (as set by initEnt)
	// live in position 0, FOO_NIL.
	pushHandlers.reg(PUSH_NIL, onPushDefault);
	entPairHandlers.reg(ENTPAIR_NIL, entPairDefault);
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	tickHandlers.destroy();
	crushHandlers.destroy();
	pushedHandlers.destroy();
	pushHandlers.destroy();
}
