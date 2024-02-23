#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../raycast.h"

#include "player.h"

static const int32_t baubleRadius = 100;
static const int32_t baubleSize[3] = {baubleRadius, baubleRadius, baubleRadius};
#define BLUE_BUF_CLR 0x0000FF
#define RED_BUF_CLR 0xFF0000

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
		me->color = s ? RED_BUF_CLR : BLUE_BUF_CLR;
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
	ret->color = BLUE_BUF_CLR;
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

static void mkSparkles(gamestate *gs, ent *e, int32_t color) {
	// Sparkles appear at the corners of selected items, for 1 frame.
	int32_t pos[3];
	int32_t radius[3];
	int state[3];
	range(i, 3) {
		radius[i] = e->radius[i]/4 + 1;
		state[i] = -1;
		pos[i] = e->center[i] - e->radius[i];
	}
	// Iterate over corners.
	// `state` is basically just a glorified binary counter, so we hit all 2^3 corners.
	while(1) {
		ent *sparkle = initEnt(
			gs, e,
			pos, e->vel, radius,
			0, 0, 0);
		sparkle->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		sparkle->color = color;
		sparkle->tick = tickHandlers.get(TICK_BAUBLE); // Destroys self next frame, like an unclaimed bauble

		for (int i = 0;; i++) {
			if (i == 3) return;
			int s = (state[i] = -state[i]);
			pos[i] = e->center[i] + s*e->radius[i];
			if (s >= 0) break;
		}
	}
}

void edittool_select(gamestate *gs, ent *parent, int mode) {
	int32_t look[3];
	getLook(look, parent);
	ent *winner = selectCast(gs, parent, look);
	if (!winner) return;
	player_toggleBauble(gs, parent, winner, mode);
	mkSparkles(gs, winner, mode ? RED_BUF_CLR : BLUE_BUF_CLR);
}

void module_edittool() {
	tickHandlers.reg(TICK_BAUBLE, bauble_tick);
	tickHandlers.reg(TICK_HELD_BAUBLE, bauble_tick_held);
}
