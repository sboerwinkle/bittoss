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

static void onPushDefault(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {}

void init_registrar() {
	whoMovesHandlers.init(WHOMOVES_NUM);
	tickHandlers.init(TICK_NUM);
	crushHandlers.init(CRUSH_NUM);
	pushedHandlers.init(PUSHED_NUM);
	pushHandlers.init(PUSH_NUM);

	pushHandlers.reg(PUSH_NIL, onPushDefault);
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	tickHandlers.destroy();
	crushHandlers.destroy();
	pushedHandlers.destroy();
	pushHandlers.destroy();
}
