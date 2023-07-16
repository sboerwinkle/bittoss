#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static const int32_t thumbtackSize[3] = {100, 100, 100};
static const int32_t baubleRadius = 100;
static const int32_t baubleSize[3] = {baubleRadius, baubleRadius, baubleRadius};

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
	int32_t v[3];
	ent *parent = me->holder;
	getPos(v, parent, me->wires[0]);
	boundVec(v, parent->radius[2] + baubleRadius*3/2, 3);
	range(i, 3) {
		v[i] = v[i] - me->center[i] + parent->center[i];
	}
	uCenter(me, v);

	if (getTrigger(me, 0)) {
		int s = !getSlider(&me->state, 0);
		me->color = s ? 0xFF0000 : 0x0000FF;
		uStateSlider(&me->state, 0, s);
	}
}

static void bauble_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

ent* mkBauble(gamestate *gs, ent *parent, ent *target) {
	ent *ret = initEnt(
		gs, parent,
		parent->center, parent->vel, baubleSize,
		1,
		T_DECOR | T_NO_DRAW_FP, 0);
	ret->whoMoves = whoMovesHandlers.getByName("move-me");
	ret->color = 0x0000FF;
	ret->pushed = pushedHandlers.getByName("drop-on-pushed");
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

	pushedHandlers.reg("thumbtack-pushed", thumbtack_pushed);
}
