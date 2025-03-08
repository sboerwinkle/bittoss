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

#define PJ_SPD 200

enum {
	s_fuel,
	s_fuel_max,
	s_vel,
	s_vel1,
	s_grounded,
	s_num
};

static char const * const * const HELP = (char const * const[]) {
	"fuel",
	"max fuel",
	"velocity tracker X",
	"velocity tracker Y",
	"grounded (like is it on the ground)",
	NULL
};

static tick_t bottleTick;

static char pj_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	int32_t vel[3];
	getVel(vel, him, me);
	// Reset what "stationary" is, according to ground speed
	vel[0] = bound(vel[0], PJ_SPD);
	vel[1] = bound(vel[1], PJ_SPD);
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

static void pj_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (him->tick == bottleTick && getSlider(him, BOTTLE_TYPE_SLIDER) == BOTTLE_FUEL) {
		int32_t capacity = me->sliders[s_fuel_max].v - me->sliders[s_fuel].v;
		int32_t avail = him->sliders[BOTTLE_AMT_SLIDER].v;
		if (avail < capacity) capacity = avail;

		him->sliders[BOTTLE_AMT_SLIDER].v -= capacity;
		me->sliders[s_fuel].v += capacity;
		bottleUpdate(gs, him);
	}
}

static void pj_tick(gamestate *gs, ent *me) {
	int32_t dvel[3] = {0, 0, 4};

	char flying = !getSlider(me, s_grounded);
	uSlider(me, s_grounded, 0);
	char thrust = 0;

	if (me->wires.num == 1) {
		ent *input = me->wires[0];

		thrust = getTrigger(input, 0);

		int32_t axis[2];
		getAxis(axis, input);
		if (flying || axis[0] || axis[1]) {
			int sliders[2];
			sliders[0] = getSlider(me, s_vel);
			sliders[1] = getSlider(me, s_vel1);

			range(i, 2) {
				dvel[i] = PJ_SPD*axis[i]/axisMaxis - sliders[i];
			}
			boundVec(dvel, 40, 2);
			range(i, 2) {
				uSlider(me, s_vel + i, sliders[i] + dvel[i]);
				// A little extra acceleration for free, not tracked by the s_vel sliders
				dvel[i] += axis[i]/16;
			}
		}
	}

	int32_t fuel = getSlider(me, s_fuel);
	if (fuel) {
		if (thrust) dvel[2] = -4;
		if (flying) uSlider(me, s_fuel, fuel-1);
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
