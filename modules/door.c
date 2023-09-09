#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "common.h"

static void door_tick(gamestate *gs, ent *me) {
	if (!me->wires.num) return;
	int32_t pos[3], vel[3], tmp[3];
	range(i, 3) {
		pos[i] = vel[i] = 0;
	}
	int numActive = 0;
	wiresAnyOrder(w, me) {
		if (getButton(w, 2)) {
			if (!numActive) {
				// We may have been accumulating velocity for the "no active" case,
				// zero that out when we hit our first active wire
				range(i, 3) vel[i] = 0;
			}
			numActive++;
			getPos(tmp, me, w);
			range(i, 3) pos[i] += tmp[i];
			getVel(tmp, me, w);
			range(i, 3) vel[i] += tmp[i];
		} else if (!numActive) {
			getVel(tmp, me, w);
			range(i, 3) vel[i] += tmp[i];
		}
	}
	// If nobody was active, we use the accumulated velocity to try to coast to a "halt"
	if (!numActive) numActive = me->wires.num;
	int32_t speed = getSlider(me, 0);
	int32_t accel = getSlider(me, 1);
	int32_t dist = getMagnitude(pos, 3);
	if (dist && getStoppingDist(speed, accel) > dist) speed = getApproachSpeed(dist, accel);
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
