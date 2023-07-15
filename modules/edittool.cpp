#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static int bauble_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	return r_drop;
}

static int thumbtack_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	list<ent*> &wires = me->wires;
	range(i, wires.num) {
		// TODO I'd prefer this to be a toggle
		uWire(wires[i], him);
	}
	uDead(gs, me);
	return r_pass;
}

static void bauble_tick_held(gamestate *gs, ent *me) {
	if (me->wires.num != 1) {
		uDrop(gs, me);
	}
}

static void bauble_tick(gamestate *gs, ent *me) {
	int timer = getSlider(&me->state, 0);
	if (timer < 100) {
		uStateSlider(&me->state, 0, timer+1);
	} else {
		uDead(gs, me);
	}
}

static const int32_t thumbtackSize[3] = {100, 100, 100};
static const int32_t baubleRadius = 100;
static const int32_t baubleSize[3] = {baubleRadius, baubleRadius, baubleRadius};

ent* mkBauble(gamestate *gs, ent *parent, ent *target) {
	int32_t center[3];
	int32_t max = 0;
	range(i, 3) {
		center[i] = target->center[i] - parent->center[i];
		int32_t x = abs(center[i]);
		if (x > max) max = x;
	}
	if (!max) max = 1;

	center[0] = parent->center[0] + (int64_t)center[0] * (parent->radius[0] - baubleRadius) / max;
	center[1] = parent->center[1] + (int64_t)center[1] * (parent->radius[1] - baubleRadius) / max;
	center[2] = parent->center[2] - parent->radius[2] - baubleRadius*3/2;

	ent *ret = initEnt(
		gs, parent,
		center, parent->vel, baubleSize,
		2,
		T_DEBRIS, T_TERRAIN + T_OBSTACLE);
	ret->whoMoves = whoMovesHandlers.getByName("move-me");
	ret->color = 0x0000FF;
	ret->pushed = pushedHandlers.getByName("bauble-pushed");
	ret->tick = tickHandlers.getByName("bauble-tick");
	ret->tickHeld = tickHandlers.getByName("bauble-tick-held");
	// TODO: This is not how wires are supposed to be updated!
	//       Direct updates make the game flow more dependent on the internal ordering of objects,
	//       even if they won't cause desynchronization. It's fine in this case since it's freshly created.
	ret->wires.add(target);

	uPickup(gs, parent, ret);

	return ret;
}

ent* mkThumbtack(gamestate *gs, ent *parent) {
	int32_t pos[3];
	int32_t look[3];
	getLook(look, parent);
	range(i, 3) {
		pos[i] = parent->center[i] + look[i];
		look[i] = look[i] * 7 + parent->vel[i];
	}
	ent *ret = initEnt(
		gs, parent,
		pos, look, thumbtackSize,
		0,
		T_WEIGHTLESS, T_TERRAIN + T_OBSTACLE + T_DEBRIS
	);
	ret->whoMoves = whoMovesHandlers.getByName("move-me");
	ret->color = 0x00FF33;
	ret->pushed = pushedHandlers.getByName("thumbtack-pushed");
	// TODO: This is not how wires are supposed to be updated!
	//       Direct updates make the game flow more dependent on the internal ordering of objects,
	//       even if they won't cause desynchronization. It's fine in this case since it's freshly created.
	ret->wires.add(parent);

	return ret;
}

void module_edittool() {
	tickHandlers.reg("bauble-tick", bauble_tick);
	tickHandlers.reg("bauble-tick-held", bauble_tick_held);
	pushedHandlers.reg("bauble-pushed", bauble_pushed);

	pushedHandlers.reg("thumbtack-pushed", thumbtack_pushed);
}
