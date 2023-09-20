#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"

static const int32_t thumbtackSize[3] = {100, 100, 100};
static const int32_t baubleRadius = 100;
static const int32_t baubleSize[3] = {baubleRadius, baubleRadius, baubleRadius};

static char thumbtack_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	if (me->holder) return 1;

	uPickup(gs, him, me, HOLD_MOVE);
	return 0;
}

static void thumbtack_tick_held(gamestate *gs, ent *me) {
	list<ent*> &wires = me->wires;
	range(i, wires.num) {
		player_toggleBauble(gs, wires[i], me->holder, getSlider(me, 0));
	}
	uDead(gs, me);
}

static void thumbtack_tick(gamestate *gs, ent *me) {
	if (!me->wires.num) uDead(gs, me);
	wiresAnyOrder(w, me) {
		if (getButton(w, 1) && getTrigger(w, getSlider(me, 0))) {
			uDead(gs, me);
		}
	}
}

static void bauble_tick_held(gamestate *gs, ent *me) {
	if (me->wires.num != 1) {
		uDrop(gs, me);
		return;
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
		int s = !getSlider(me, 0);
		me->color = s ? 0xFF0000 : 0x0000FF;
		uSlider(me, 0, s);
	}
}

static void bauble_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

ent* mkBauble(gamestate *gs, ent *parent, ent *target, int mode) {
	ent *ret = initEnt(
		gs, parent,
		parent->center, parent->vel, baubleSize,
		1,
		T_DECOR | T_NO_DRAW_FP, 0);
	ret->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
	ret->color = 0x0000FF;
	ret->tick = tickHandlers.get(TICK_BAUBLE);
	ret->tickHeld = tickHandlers.get(TICK_HELD_BAUBLE);
	// TODO: This is not how wires are supposed to be updated!
	//       Direct updates make the game flow more dependent on the internal ordering of objects,
	//       even if they won't cause desynchronization. It's fine in this case since it's freshly created.
	ret->wires.add(target);

	uPickup(gs, parent, ret, HOLD_DROP);
	if (mode) pushBtn(ret, 2);

	return ret;
}

ent* mkThumbtack(gamestate *gs, ent *parent, int mode) {
	int32_t pos[3];
	int32_t look[3];
	getLook(look, parent);
	range(i, 3) {
		// Hack: Once again using the raw look as an offset, that should be reasonably "small" given the size of axisMaxis
		pos[i] = parent->center[i] + look[i];
		look[i] = (double)(32*7)/axisMaxis * look[i] + parent->vel[i];
	}
	ent *ret = initEnt(
		gs, parent,
		pos, look, thumbtackSize,
		1,
		T_WEIGHTLESS, T_TERRAIN + T_OBSTACLE + T_DEBRIS
	);
	ret->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
	ret->color = 0x00FF33;
	ret->pushed = pushedHandlers.get(PUSHED_THUMBTACK);
	ret->tickHeld = tickHandlers.get(TICK_HELD_THUMBTACK);
	ret->tick = tickHandlers.get(TICK_THUMBTACK);
	// TODO: This is not how wires are supposed to be updated!
	//       Direct updates make the game flow more dependent on the internal ordering of objects,
	//       even if they won't cause desynchronization. It's fine in this case since it's freshly created.
	ret->wires.add(parent);
	// Once again we find ourselves directly manipulating state on a newly created item.
	// TODO We really need a better way to handle this, maybe flush new items???
	ret->sliders[0].v = mode;

	return ret;
}

void module_edittool() {
	tickHandlers.reg(TICK_BAUBLE, bauble_tick);
	tickHandlers.reg(TICK_HELD_BAUBLE, bauble_tick_held);

	pushedHandlers.reg(PUSHED_THUMBTACK, thumbtack_pushed);
	tickHandlers.reg(TICK_HELD_THUMBTACK, thumbtack_tick_held);
	tickHandlers.reg(TICK_THUMBTACK, thumbtack_tick);
}
