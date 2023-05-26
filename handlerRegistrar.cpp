#include <string.h>

#include "list.h"
#include "main.h"
#include "ent.h"
#include "util.h"

#include "handlerRegistrar.h"

catalog<whoMoves_t> whoMovesHandlers;
catalog<tick_t> tickHandlers;
catalog<draw_t> drawHandlers;
catalog<pushed_t> pushedHandlers;
catalog<push_t> pushHandlers;

static void onPushDefault(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {}

void init_registrar() {
	whoMovesHandlers.init();
	tickHandlers.init();
	drawHandlers.init();
	pushedHandlers.init();
	pushHandlers.init();

	whoMovesHandlers.reg("nil", NULL);
	tickHandlers.reg("nil", NULL);
	drawHandlers.reg("nil", NULL);
	pushedHandlers.reg("nil", NULL);
	pushHandlers.reg("nil", onPushDefault);
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	tickHandlers.destroy();
	drawHandlers.destroy();
	pushedHandlers.destroy();
	pushHandlers.destroy();
}