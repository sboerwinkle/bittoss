#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "bottle.h"
#include "player.h"
#include "dust.h"

#include "puddlejumper.h"

#define PJ_SPD 200
#define PJ_ACCEL 20

enum {
	s_fuel,
	s_fuel_max,
	s_vel,
	s_vel1,
	s_grounded,
	s_power_indicator,
	s_num
};

static_assert(s_power_indicator == PJ_POWER_SLIDER);

static char const * const * const HELP = (char const * const[]) {
	"fuel",
	"max fuel",
	"velocity tracker X",
	"velocity tracker Y",
	"grounded (like is it on the ground)",
	"power indicator (for use by HUD)",
	NULL
};

static tick_t bottleTick;

static char pj_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	int32_t vel[3];
	getVel(vel, him, me);
	// Reset what "stationary" is, according to ground speed
	// For now we bound this as a vector, like player.c does.
	// Movement logic's a little different for us though,
	// so I'm not sure if bounding it component-wise would be better?
	boundVec(vel, PJ_SPD, 2);
	if (axis == 2) {
		if (dir < 0) {
			uSlider(me, s_grounded, 1);
		}
	} else {
		// Argh.
		int32_t currentSlide = getSlider(me, s_vel + axis);
		vel[axis] = currentSlide + dir*dv;
		if (dir*vel[axis] > 0) vel[axis] = 0;
	}
	uSlider(me, s_vel, vel[0]);
	uSlider(me, s_vel1, vel[1]);
	return 0;
}

static char pj_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (him->tick == bottleTick && getSlider(him, BOTTLE_TYPE_SLIDER) == BOTTLE_FUEL) {
		int32_t capacity = me->sliders[s_fuel_max].v - me->sliders[s_fuel].v;
		int32_t avail = him->sliders[BOTTLE_AMT_SLIDER].v;
		if (avail < capacity) capacity = avail;

		him->sliders[BOTTLE_AMT_SLIDER].v -= capacity;
		me->sliders[s_fuel].v += capacity;
		bottleUpdate(gs, him);
	}
	return 0;
}

static void verticalDust(gamestate *gs, ent *me) {
	int32_t pos[3];
	int32_t vel[3];
	memcpy(pos, me->center, sizeof(pos));
	memcpy(vel, me->vel, sizeof(vel));
	pos[2] += me->radius[2]; // Center of bottom
	vel[2] += 300; // Shoot down fast
	int32_t drift[3];
	range(i, 3) {
		drift[i] = (random(gs) % 21) - 10;
	}
	mkDust(gs, me, pos, vel, drift, 200, 0x808080);
}

static void pj_tick(gamestate *gs, ent *me) {
	int32_t dvel[3] = {0, 0, 16};

	char flying = !getSlider(me, s_grounded);
	uSlider(me, s_grounded, 0);
	int32_t fuel = getSlider(me, s_fuel);
	if (fuel && flying) uSlider(me, s_fuel, fuel-1);

	if (me->wires.num == 1) {
		ent *input = me->wires[0];

		// Vertical thrust
		if (fuel && getTrigger(input, 0)) {
			dvel[2] = -8;
			verticalDust(gs, me);
		}

		int32_t axis[2];
		getAxis(axis, input);
		if (axis[0] || axis[1]) {
			int32_t sliders[2];
			sliders[0] = getSlider(me, s_vel);
			sliders[1] = getSlider(me, s_vel1);
			int32_t numerX, denomX, numerY, denomY;
			int signX = 2*(axis[0] >= 0) - 1;
			int signY = 2*(axis[1] >= 0) - 1;
			denomX = signX * axis[0]; // Basically just abs(axis[0])
			numerX = PJ_SPD - signX * sliders[0]; // How far we can go until sliders[0] reaches +/- PJ_SPD
			denomY = signY * axis[1];
			numerY = PJ_SPD - signY * sliders[1];

			// This is the "good acceleration" you get inside the PJ_SPD square.
			// It's capped at `PJ_ACCEL` units/frame/frame, which is pretty strong.
			if (numerX > PJ_ACCEL) numerX = PJ_ACCEL;
			else if (numerX < 0) numerX = 0; // Shouldn't happen, but prevents edge cases
			if (numerY > PJ_ACCEL) numerY = PJ_ACCEL;
			else if (numerY < 0) numerY = 0; // Same as for numerX

			// We'll use numerX/denomX as our scaling factor for thrust,
			// unless numerY/denomY is smaller. We do the comparison with
			// cross-multiplication (which shouldn't exceed size limits).
			// However, one denominator may be 0 (but not both, as we skip
			// horizontal movement completely if there are no inputs), so
			// we have to be aware of that when comparing our cross products.
			if (denomY && numerY*denomX <= numerX*denomY) {
				numerX = numerY;
				denomX = denomY;
			}

			range(i, 2) {
				dvel[i] = numerX * axis[i] / denomX;
				uSlider(me, s_vel + i, sliders[i] + dvel[i]);
				// A little extra acceleration for free, not tracked by the s_vel sliders.
				// (this is the "bad acceleration", only 4 u/f/f, but not bounded by a "square").
				dvel[i] += axis[i]/16;
			}

			// 2 = good acceleration,
			// 1 = bad acceleration,
			uSlider(me, s_power_indicator, numerX ? 2 : 1);
		} else {
			// 0 = no acceleration.
			uSlider(me, s_power_indicator, 0);
		}
	}

	uVel(me, dvel);
}

static void cmdPj(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, s_num);
	e->tick = tickHandlers.get(TICK_PUDDLEJUMP);
	e->push = pushHandlers.get(PUSH_PUDDLEJUMP);
	e->pushed = pushedHandlers.get(PUSHED_PUDDLEJUMP);
}

void module_puddlejumper() {
	bottleTick = tickHandlers.get(TICK_BOTTLE);

	tickHandlers.reg(TICK_PUDDLEJUMP, pj_tick);
	pushedHandlers.reg(PUSHED_PUDDLEJUMP, pj_pushed);
	pushHandlers.reg(PUSH_PUDDLEJUMP, pj_push);

	addEditHelp(&ent::tick, pj_tick, "puddlejumper", HELP);
	addEntCommand("puddlejumper", cmdPj);
}
