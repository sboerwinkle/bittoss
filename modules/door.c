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
	if (!numActive) {
		numActive = me->wires.num;
		if (!numActive) return;
	}
	range(i, 3) {
		pos[i] /= numActive;
		vel[i] /= numActive;
	}
	int32_t speed = getSlider(me, 0);
	int32_t accel = getSlider(me, 1);
	int32_t dist = getMagnitude(pos, 3);
	if (dist && getStoppingDist(speed, accel) > dist) speed = getApproachSpeed(dist, accel);
	//if (dist && gs == rootState) printf("speed: %d\n", speed);

	// Right now `vel` is an attempt to match speed;
	// account for our displacement so we attempt to actually arrive
	boundVec(pos, speed, 3);
	range(i, 3) vel[i] = pos[i] + vel[i];

	// We're limited on how hard we can actually push
	boundVec(vel, accel, 3);
	uVel(me, vel);
	//if (vel[2] && gs == rootState) printf("accel: %d\n", vel[2]);
}

static void cmdDoor(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tick = tickHandlers.get(TICK_DOOR);
	e->tickHeld = tickHandlers.get(TICK_DOOR);
}

void module_door() {
	tickHandlers.reg(TICK_DOOR, door_tick);
	addEntCommand("door", cmdDoor);
}
