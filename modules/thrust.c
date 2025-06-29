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

char const * const * const M_THRUST_HELP = (char const * const[]) {
        "divisor: Accleration is `offset*64/divisor`",
        NULL
};

static void thrust_tick(gamestate *gs, ent *me) {
	int numWires = me->wires.num;
	if (!numWires || getButton(me, 0)) return;

	int32_t divisor = getSlider(me, 0);
	if (!divisor) return;

	int32_t offset[3] = {0, 0, 0}, tmp[3];
	wiresAnyOrder(w, me) {
		if (getButton(w, 2)) {
			getPos(tmp, w, me);
			range(i, 3) offset[i] += tmp[i];
		}
	}
	range(i, 3) {
		offset[i] = offset[i] * 64 / divisor;
	}
	uVel(me->holdRoot, offset);
}

static void cmdThrust(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 1);
	e->tick = tickHandlers.get(TICK_THRUST);
	e->tickHeld = tickHandlers.get(TICK_THRUST);
}

void module_thrust() {
	tickHandlers.reg(TICK_THRUST, thrust_tick);
	addEditHelp(&ent::tickHeld, thrust_tick, "thrust", M_THRUST_HELP);
	addEntCommand("thrust", cmdThrust);
}
