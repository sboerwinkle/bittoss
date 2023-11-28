#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

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

// Rational: numerator / denominator.
struct rat { int32_t n, d; };
// Only strictly rational when `d > 0`. `d` should never be negative.
// When `d == 0`, you get values which behave something like +/-Inf and NaN
//   (depending on how `n` compares to 0).
// Based on how we do comparisons, there are some surprises:
// +/-Inf == +/-Inf (regardless of sign)
// NaN == Anything
// However, we still have +/-Inf comparing correctly to rationals,
// so if we play our cards right we can use this fairly simple comparison operator with success.
#define lt(a, b) ((int64_t)a.n*b.d < (int64_t)b.n*a.d)

void edittool_select(gamestate *gs, ent *parent, int mode) {
	int32_t *pos = parent->center;
	ent *holdRoot = parent->holdRoot;

	int32_t look[3];
	getLook(look, parent);
	int32_t flip[3];
	range(i, 3) {
		if (look[i] >= 0) {
			flip[i] = 1;
		} else {
			flip[i] = -1;
			look[i] *= -1;
		}
	}

	// Check intersection of look ray with each ent.
	// We could do this w/ a very fast object and use the collision detection,
	// but this lets us select ghost things.
	ent *winner = NULL;
	rat best = {.n = INT32_MAX, .d = 1}; // Highest possible rational
	for (ent *e = gs->ents; e; e = e->ll.n) {
		if (e->holdRoot == holdRoot) continue;
		rat lower = {.n = 0, .d = 1}; // 0/1 == 0
		rat upper = best;
		range(d, 3) {
			int32_t x = flip[d]*(e->center[d] - pos[d]);

			rat f1, f2;
			f1.n = x - e->radius[d];
			f2.n = x + e->radius[d];
			f1.d = f2.d = look[d];
			// f1 and f2 might not be strictly rational, so be careful with phrasing here...
			if (lt(lower, f1)) lower = f1;
			if (lt(f2, upper)) upper = f2;
			if (!lt(lower, upper)) goto next_ent;
		}
		if (lower.n) {
			// We only take things a positive distance in front of us, not things we're already inside
			best = lower;
			winner = e;
		}
		next_ent:;
	}
	if (!winner) return;
	player_toggleBauble(gs, parent, winner, mode);
	mkSparkles(gs, winner, mode ? RED_BUF_CLR : BLUE_BUF_CLR);
}

void module_edittool() {
	tickHandlers.reg(TICK_BAUBLE, bauble_tick);
	tickHandlers.reg(TICK_HELD_BAUBLE, bauble_tick_held);
}
