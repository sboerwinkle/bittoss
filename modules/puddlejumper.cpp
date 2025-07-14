#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "common.h"
#include "bottle.h"
#include "player.h"
#include "dust.h"

#define PJ_SPD 200

enum {
	s_fuel,
	s_fuel_max,
	s_vel,
	s_vel1,
	s_grounded,
	s_axis,
	s_axis1,
	s_num
};

static char const * const * const HELP = (char const * const[]) {
	"fuel",
	"max fuel",
	"velocity tracker X",
	"velocity tracker Y",
	"grounded (like is it on the ground)",
	"input lock X",
	"input lock Y",
	NULL
};

static tick_t bottleTick;

static char pj_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	int32_t vel[3];
	getVel(vel, him, me);
	// Reset what "stationary" is, according to ground speed
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

static void horizDust(gamestate *gs, ent *me, int32_t thrustDir[2]) {
	int32_t pos[3];
	int32_t vel[3];
	memcpy(pos, me->center, sizeof(pos));
	memcpy(vel, me->vel, sizeof(vel));
	range(i, 2) {
		pos[i] += me->radius[0]*thrustDir[i]/-axisMaxis;
		vel[i] += 150*thrustDir[i]/-axisMaxis; // This is slower than the vertical dust
	}
	int32_t drift[3];
	range(i, 3) {
		drift[i] = (random(gs) % 21) - 10;
	}
	mkDust(gs, me, pos, vel, drift, 100, 0x808080);
}

static void pj_tick(gamestate *gs, ent *me) {
	int32_t dvel[3] = {0, 0, 16};

	char flying = !getSlider(me, s_grounded);
	uSlider(me, s_grounded, 0);
	int32_t fuel = getSlider(me, s_fuel);
	if (fuel && flying) uSlider(me, s_fuel, fuel-1);

	char boost = 0;

	if (me->wires.num == 1) {
		ent *input = me->wires[0];
		int32_t axis[2];
		getAxis(axis, input);

		if (fuel && getTrigger(input, 0)) {
			dvel[2] = -8;
			verticalDust(gs, me);
		}

		boost = fuel && getTrigger(input, 1);
		if (boost) {
			int32_t axisLock[2] = {getSlider(me, s_axis), getSlider(me, s_axis1)};
			if (axisLock[0] == -1 && axisLock[1] == -1) {
				axisLock[0] = axis[0];
				uSlider(me, s_axis , axisLock[0]);
				axisLock[1] = axis[1];
				uSlider(me, s_axis1, axisLock[1]);
			}
			// Let "moving" be the player-style movement,
			// and "thrusting" be the spaceship-style acceleration (with dust).
			// Then we have two options while `boost` is true:
			int32_t *thrustDir;
			if (axisLock[0] || axisLock[1]) {
				// Option 1: we're moving and thrusting in the locked direction.
				thrustDir = axisLock;
			} else {
				// Option 2: We're not moving (locked),
				//           but we thrust according to player inputs (if non-zero).
				if (axis[0] || axis[1]) {
					thrustDir = axis;
				} else {
					goto skipThrust;
				}
			}
			range(i, 2) {
				dvel[i] += thrustDir[i]/16;
			}
			horizDust(gs, me, thrustDir);
			skipThrust:;
			memcpy(axis, axisLock, sizeof(axis));
		}


		// This stuff mirror the player movement logic pretty closely
		if (flying || axis[0] || axis[1]) {
			int32_t sliders[2];
			sliders[0] = getSlider(me, s_vel);
			sliders[1] = getSlider(me, s_vel1);
			int32_t speed;
			if (flying) {
				boundVec(sliders, PJ_SPD, 2);
				speed = PJ_SPD;
			} else {
				speed = getMagnitude(sliders, 2);
				if (speed < PJ_SPD) speed = PJ_SPD;
			}

			int32_t moveVel[2];
			range(i, 2) {
				moveVel[i] = speed*axis[i]/axisMaxis - sliders[i];
			}
			boundVec(moveVel, 40, 2);
			range(i, 2) {
				uSlider(me, s_vel + i, sliders[i] + moveVel[i]);
				dvel[i] += moveVel[i];
			}
		}
	}
	if (!boost) {
		// Unlock any inputs
		uSlider(me, s_axis , -1);
		uSlider(me, s_axis1, -1);
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
