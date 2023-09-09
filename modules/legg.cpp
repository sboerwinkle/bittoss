#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "common.h"

// Maybe this should be common?
static int32_t max(int32_t a, int32_t b) {
	// I'm being fancy here, maybe I should be dumb instead
	return (a + b + abs(a-b)) / 2;
}
static int32_t min(int32_t a, int32_t b) {
	// I'm being fancy here, maybe I should be dumb instead
	return (a + b - abs(a-b)) / 2;
}

static void legg_tick(gamestate *gs, ent *me) {
	if (!me->holder) return;

	int32_t v_offset = getSlider(me, 0);
	int32_t max_speed = getSlider(me, 1);
	int32_t max_accel = getSlider(me, 2);
	int32_t height = getSlider(me, 3);
	int32_t v_accel_limit = getSlider(me, 4);
	int32_t state = getSlider(me, 5);
	// State: 0 - reset; 1 - propel (no contact); 2 - propel (contact)

	int32_t v1[3] = {0, 0, 0}, vel[3], acc[3];
	// v1  - initially will be our desired offset from current position.
	// vel - eventually, our current velocity
	// acc - eventually, our desired acceleration

	// Some things are reversed based on if the legg is coming or going
	int flip;
	if (state) {
		flip = -1;
	} else {
		flip = 1;
		height *= -1;
	}

	if (state != 2) {
		// When not in contact, we give the leg some extra oomf.
		// This helps multi-leg walk cycles smooth out to optimum,
		// since legs just "following along" will fall out of sync faster.
		max_accel += (max_accel+1) / 2;
	}

	// We are expecting exactly one wire, and for wired ent to have a holder.
	// This will still behave predictably if that doesn't hold, however.
	wiresAnyOrder(w, me) {
		if (!w->holder) continue;
		// We're abusing `vel` since it's a vector we have handy.
		// Don't worry, it'll actually represent velocity in a sec.
		getPos(vel, w->holder, w);
		range(i, 2) v1[i] += vel[i];
	}
	char hasInput = 0; // We'll need this later
	// Abusing `vel` again...
	getPos(vel, me, me->holder);
	range(i, 2) {
		if (v1[i]) hasInput = 1;
		v1[i] = vel[i] + flip * v1[i];
	}
	v1[2] = vel[2];

	// Okay now `vel` is actually the vel
	getVel(vel, me->holder, me);

	int32_t dist = max(abs(v1[0]), abs(v1[1]));
	int32_t speed_limit = min(max_speed, getApproachSpeed(dist, max_accel));

	boundVec(v1, speed_limit, 2);
	// `v1` is now my desired velocity, though v1[2] is still the goal offset

	range(i, 2) acc[i] = v1[i] - vel[i];
	boundVec(acc, max_accel, 2);

	// Haven't been messing with the vertical components of our vectors yet, time to figure that out
	int32_t old_speed = max(abs(vel[0]), abs(vel[1]));
	int32_t new_speed = max(abs(vel[0] + acc[0]), abs(vel[1] + acc[1]));
	// This could be int overflow if we get titanically sized walkers, would have to go to int64_t
	int32_t goal_height = height * new_speed / max_speed;
	int32_t goal_climb = height * (new_speed - old_speed) / max_speed;
	// v1[2] is still a goal offset, not a goal velocity
	v1[2] += goal_height + v_offset;
	// Unlike the horizontal motion, our destination may not be stationary (goal_climb),
	// and we don't impose any restriction on our rate of climb (just acceleration).
	// We do still want to calculate how fast we can go without overshooting, though.
	int32_t v_speed = getApproachSpeed(abs(v1[2]), v_accel_limit);
	// Finish converting v1[2] from a desired offset to a desired velocity
	v1[2] = goal_climb + (v1[2] > 0 ? v_speed : -v_speed);
	acc[2] = bound(v1[2] - vel[2], v_accel_limit);
	//printf("dist: %d; desired speed: %d; old speed: %d; new speed: %d\n", dist, speed_limit, old_speed, new_speed);

	uVel(me, acc);

	char complete = !dist && !new_speed;
	// Now that that's out of the way...
	// We need to determine our state transition.
	// nonzero: propel. Flips when we *were* in contact (early reset) or the state is complete.
	//                  Note that even if we try to leave "propel" state, we are set back before next
	//                  tick if we're still in contact, so effectively we only leave once out of contact.
	//    zero: reset.  Flips when state is complete (and we have a non-zero input, to avoid flipping every frame)
	if (state) {
		if (state == 2 || complete) {
			uSlider(me, 5, 0);
		}
	} else {
		if (complete && hasInput) {
			uSlider(me, 5, 1);
		}
	}
}

static char legg_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	// Force this legg into "pushed (contact)" state
	uSlider(me, 5, 2);
	return 0;
}

void module_legg() {
	tickHandlers.reg(TICK_LEGG, legg_tick);
	pushedHandlers.reg(PUSHED_LEGG, legg_pushed);
}
