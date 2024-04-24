#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "common.h"

#include "legg.h"

char const * const * const M_LEGG_HELP = (char const * const[]){
	"phase - where in the cycle it is. <= 2*time*v_time",
	"width - distance from the center to step",
	"height - distance below mount point to idle",
	"stepHeight - how far to rise while stepping",
	"tread - more or less, how deep to push while walking",
	"speed - max speed relative to mount point",
	"accel - acceleration relative to mount point",
	"time - time (in frames) to spend on moving forward/backwards",
	"v_time - time to spend moving up/down",
	NULL
};

// Maybe this should be common?
static int32_t max(int32_t a, int32_t b) {
	// I'm being fancy here, maybe I should be dumb instead.
	// This won't work on sufficiently large numbers as `a-b` overflows.
	return (a + b + abs(a-b)) / 2;
}
static int32_t min(int32_t a, int32_t b) {
	// I'm being fancy here, maybe I should be dumb instead.
	// This won't work on sufficiently large numbers as `a-b` overflows.
	return (a + b - abs(a-b)) / 2;
}

// `out` should have 2 elements, doesn't need to be initialized.
static void getInputOld(ent *me, int32_t *out) {
	out[0] = out[1] = 0;
	int32_t tmp[3];
	// We are expecting exactly one wire, and for wired ent to have a holder.
	// This will still behave predictably if that doesn't hold, however.
	wiresAnyOrder(w, me) {
		if (!w->holder) continue;
		getPos(tmp, w->holder, w);
		range(i, 2) out[i] += tmp[i];
	}
}

// `out` should have 2 elements, doesn't need to be initialized.
static void getInput(ent *me, int32_t *out, int32_t width) {
	out[0] = out[1] = 0;
	// We are expecting exactly one wire,
	// and for the wired ent to have at least 2 sliders (T_INPUT)
	// This will still behave predictably if that doesn't hold, however.
	wiresAnyOrder(w, me) {
		range(i, 2) out[i] += getSlider(w, i) * width / axisMaxis;
	}
}

static void move(ent *me, int32_t *v1, int32_t s, int32_t a, int32_t v_s, int32_t v_a, int32_t tread) {
	int32_t vel[3], acc[3];

	// `vel` will be the actual velocity in a sec, but we need to borrow it
	getPos(vel, me, me->holder);
	range(i, 3) {
		v1[i] += vel[i];
	}
	// v1 has been transformed from goal position to goal offset

	// Okay now `vel` is actually the vel
	getVel(vel, me->holder, me);

	int32_t dist = max(abs(v1[0]), abs(v1[1]));
	if (dist) s = min(s, getApproachSpeed(dist, a));

	boundVec(v1, s, 2);
	// `v1` is now my desired velocity, though v1[2] is still the goal offset

	range(i, 2) acc[i] = v1[i] - vel[i];
	boundVec(acc, a, 2);

	// Haven't been messing with the vertical components of our vectors yet, time to figure that out
	int32_t new_speed = max(abs(vel[0] + acc[0]), abs(vel[1] + acc[1]));
	// Remember, v1[2] is still a goal offset.
	// This could be int overflow if we get titanically sized walkers, would have to go to int64_t
	if (s) v1[2] += tread * new_speed / s;
	// We can skip this one if we're already at the goal elevation, which saves on `sqrt`s
	if (v1[2]) v_s = min(v_s, getApproachSpeed(abs(v1[2]), v_a));
	// Finish converting v1[2] from a desired offset to a desired velocity
	v1[2] = bound(v1[2], v_s);
	acc[2] = bound(v1[2] - vel[2], v_a);

	uVel(me, acc);
}

static void inner_legg_tick(gamestate *gs, ent *me, int32_t *v1, int o) {

	int32_t counter = getSlider(me, 0);
	// Old and new versions differ about if there's a slider here,
	// this is phrased in a way they can both use.
	int32_t height = getSlider(me, o++);
	int32_t stepHeight = getSlider(me, o++);
	int32_t tread = getSlider(me, o++);
	int32_t s = getSlider(me, o++);
	int32_t a = getSlider(me, o++);
	int32_t time = getSlider(me, o++);
	int32_t v_time = getSlider(me, o++);

	char hasInput = v1[0] || v1[1];

	int32_t half = time + v_time;
	if (hasInput && half) {
		counter++;
		while (counter >= 2*half) counter -= 2*half;
		uSlider(me, 0, counter);

		if (counter >= time) {
			// phase 2,3,4: no tread.
			tread = 0;
			if (counter < half + time) {
				// phase 2,3: legg goes high
				height -= stepHeight;
			}
		}

		if (counter < half) { // Phase 1 & 2, the foot goes backwards (so we go forwards)
			range(i, 2) v1[i] *= -1;
		}
	} else {
		// If no input, don't advance the counter, just have the leggs to go resting position.
		// With no tread, which legg stays in contact is dependent on gross physics engine arbitration,
		// but I'm not sure if we actually want to mess with like using a reverse tread or something here.
		// KISS for now
		tread = 0;
	}
	v1[2] = height;
	move(me, v1, s, a, s, a, tread);
}

static void legg_tick_old(gamestate *gs, ent *me) {
	if (!me->holder) return;
	int32_t v1[3];
	getInputOld(me, v1);

	inner_legg_tick(gs, me, v1, 1);
}

static void legg_tick(gamestate *gs, ent *me) {
	if (!me->holder) return;
	int32_t v1[3];
	int32_t width = getSlider(me, 1);
	getInput(me, v1, width);

	inner_legg_tick(gs, me, v1, 2);
}

static void cmdLegg(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 9);
	e->tickHeld = tickHandlers.get(TICK_LEGG);
}

void module_legg() {
	tickHandlers.reg(TICK_LEGG, legg_tick);
	tickHandlers.reg(TICK_LEGG_OLD, legg_tick_old);
	addEditHelp(&ent::tickHeld, legg_tick, "legg", M_LEGG_HELP);
	addEntCommand("legg", cmdLegg);
}
