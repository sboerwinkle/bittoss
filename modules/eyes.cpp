#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static const int32_t eyeSize[3] = {100, 100, 100};
static const int32_t eyeInset = 50;

static void eye_tick_held(gamestate *gs, ent *me) {
	ent *parent = me->holder;
	int32_t vec[3];
	getLook(vec, parent);
	// This kinda assumes the parent is a cube/sphere, and ofc yeah it should be
	int32_t r = parent->radius[2] - eyeInset;
	range(i, 3) {
		// Another multiplication where we don't upgrade to int64_t,
		// it should probably be in bounds given that axisMaxis is like 32
		vec[i] = r * vec[i] / axisMaxis + parent->center[i] - me->center[i];
	}
	uCenter(me, vec);
}

static void eye_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

ent* mkEye(gamestate *gs, ent *parent) {
	ent *ret = initEnt(
		gs, parent,
		parent->center, parent->vel, eyeSize,
		0,
		T_DECOR | T_NO_DRAW_FP, 0);
	ret->whoMoves = whoMovesHandlers.getByName("move-me");
	ret->color = 0x0000FF;
	ret->pushed = pushedHandlers.getByName("drop-on-pushed");
	ret->tick = tickHandlers.getByName("eye-tick");
	ret->tickHeld = tickHandlers.getByName("eye-tick-held");

	uPickup(gs, parent, ret);

	return ret;
}

void module_eyes() {
	tickHandlers.reg("eye-tick", eye_tick);
	tickHandlers.reg("eye-tick-held", eye_tick_held);
}
