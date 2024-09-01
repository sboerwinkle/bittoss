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

#include "spider.h"

char const * const * const M_SPIDER_HELP = (char const * const[]){
	"d1 - foot elevation (at top)",
	"d2 - foot elevation (at bottom)",
	"scale - max horizontal displacement",
	"t1 - time to travel forward",
	"t2 - time to travel backward",
	"t - internal time counter",
	"spring - spring constant (64/x)",
	"drag",
	"max accel",
	"ratio - controls force sent to body. Approx = # of legs",
	NULL
};

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

static void computeAccel(int32_t *a, ent *me, int32_t *pos, int32_t *vel, int32_t spring, int32_t drag, int32_t a_max) {
	int32_t curr[3];

	getPos(curr, me->holder, me);
	range(i, 3) {
		pos[i] = (pos[i] - curr[i]) * 64 / spring;
	}
	boundVec(pos, a_max, 3);

	getVel(curr, me->holder, me);
	range(i, 3) vel[i] -= curr[i];

	boundVec(vel, drag, 3);

	range(i, 3) a[i] = pos[i] + vel[i];
}

static void spider_tick(gamestate *gs, ent *me) {
	if (!me->holder) return;

	int32_t height_top = getSlider(me, 0);
	int32_t height_bottom = getSlider(me, 1);
	int32_t width = getSlider(me, 2);
	int32_t t_top = getSlider(me, 3);
	int32_t t_bottom = getSlider(me, 4);
	int32_t counter = getSlider(me, 5);
	int32_t spring = getSlider(me, 6);
	int32_t drag = getSlider(me, 7);
	int32_t a_max = getSlider(me, 8);
	int32_t ratio_legs = getSlider(me, 9);

	if (spring <= 0) spring = 1;
	if (ratio_legs <= 0) ratio_legs = 1;

	int32_t v1[3];
	getInput(me, v1, width);
	char hasInput = v1[0] || v1[1];
	v1[2] = height_bottom;

	int32_t t_cycle = t_top + t_bottom;
	int32_t vel[3];
	vel[2] = 0; // Never aim for any vertical velocity
	if (hasInput && t_cycle) {
		// Todo: Should the counter advance even when stationary?
		counter++;
		while (counter >= t_cycle) counter -= t_cycle;
		uSlider(me, 5, counter);

		int32_t numer, denom;
		if (counter >= t_bottom) {
			v1[2] = height_top;

			denom = t_top;
			numer = counter - t_bottom;
		} else {
			// Going backwards on the bottom (so body moves forwards)
			v1[0] *= -1;
			v1[1] *= -1;
			denom = t_bottom;
			numer = counter;
		}
		range(i, 2) vel[i] = v1[i] / denom;
		range(i, 2) v1[i] = (2*numer-denom)*v1[i]/denom;
	} else {
		vel[0] = vel[1] = 0;
		// Don't need to set pos, v1[0,1] is already 0
	}

	int32_t a[3];
	computeAccel(a, me, v1, vel, spring, drag, a_max);

	// We do division and re-multiplication to make sure the impulse sent to the legs is always a multiple
	// of the impulse sent to the body. This prevents some weird situations, like being unable to come to a
	// total stop due to no impulse being sent to the body. Instead, the body will oscillate very slightly
	// in those situations, over the course of a few frames.

	range(i, 3) a[i] = a[i] / -ratio_legs;
	uVel(me->holdRoot, a);

	range(i, 3) a[i] = a[i] * -ratio_legs;
	uVel(me, a);
}

static void cmdSpider(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 10);
	e->tickHeld = tickHandlers.get(TICK_SPIDER);
}

void module_spider() {
	tickHandlers.reg(TICK_SPIDER, spider_tick);
	addEditHelp(&ent::tickHeld, spider_tick, "spider", M_SPIDER_HELP);
	addEntCommand("spider", cmdSpider);
}
