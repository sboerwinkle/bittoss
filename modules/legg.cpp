#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

// Maybe this should be common?
static int32_t max(int32_t a, int32_t b) {
	// I'm being fancy here, maybe I should be dumb instead
	return (a + b + abs(a-b)) / 2;
}

static void legg_tick(gamestate *gs, ent *me) {
	if (!me->holder) return;

	int32_t offset = getSlider(me, 0);
	int32_t speed = getSlider(me, 1);
	int32_t accel = getSlider(me, 2);
	int32_t v_mult = getSlider(me, 3);
	int32_t v_accel = getSlider(me, 4);
	int32_t state = getSlider(me, 5);

	int32_t pos[3], vel[3];
	getPos(pos, me->holder, me);
	pos[2] += offset;
	int mult = state * 2 - 1;
	wiresAnyOrder(w, me) {
		if (!w->holder) continue;
		// We're abusing `vel` since it's a vector we have handy.
		// Don't worry, it'll actually represent velocity in a sec.
		getPos(vel, w->holder, w);
		range(i, 2) pos[i] += mult * vel[i];
	}
	getVel(vel, me->holder, me);

	// If it decelerates at rate `a` on every subsequent turn,
	// a velocity of `n*a+r` will come to rest in `n*(n+1)/2*a + r*(n+1)` units.
	// The trick here is reversing `n*(n+1)/2`... Solve for x:
	// x*(x+1)/2 = q
	// x^2 + x = 2*q
	// x^2 + x - 2*q = 0
	// quadratic formula, a=1,b=1,c=-2q
	// (-1 +- sqrt(1+8q)) / 2
	// There will always be a negative and non-negative root here, we always want the non-negative one.
	// We also want it truncated to an integer. We also don't lost anything by immediately truncating
	// the result of `sqrt` to an integer - since we're scaling it down by 2 anyway, the fractional part
	// only gets smaller (and stays < 1)

	int32_t dist;
	{
		int32_t a = abs(pos[0]);
		int32_t b = abs(pos[1]);
		// No idea if this faster than a naive `max`, but it's fun!
		dist = (a + b + abs(a-b)) / 2;
	}
	int32_t q = dist / accel;
	int32_t n = (((int32_t)sqrt(1 + 8*q)) - 1) / 2;
	// Now that we know the true value of `n`, we just solve for `r` in our original expression for stopping distance.
	int32_t r = (dist - n*(n+1)/2*a)/(n+1);
	int32_t maxSpeed = n*a + r;
	int32_t ease = speed / (2 * accel);
	if (ease < 1) ease = 1;
	range(i, 2) pos[i] = pos[i] / ease + (pos[i] > 0 ? 1 : (pos[i] < 0 ? -1 : 0));
	boundVec(pos, speed, 3);
	// `pos` is now my desired velocity
	range(i, 3) vel[i] = pos[i] + vel[i];
	boundVec(vel, accel, 3);
	// `vel` is now my desired acceleration
	uVel(me, vel);
}

void module_door() {
	tickHandlers.reg(TICK_DOOR, door_tick);
}
