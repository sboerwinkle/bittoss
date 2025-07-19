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

char const * const * const M_AIRBRAKE_HELP = (char const * const[]) {
        "accel",
        NULL
};

static void airbrake_tick(gamestate *gs, ent *me) {
	if (me->wires.num != 1 || getButton(me, 0)) return;

	int32_t v[3];
	getVel(v, me, me->wires[0]);
	boundVec(v, getSlider(me, 0), 3);
	uVel(me->holdRoot, v);
}

static void cmdAirbrake(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 1);
	e->tick = tickHandlers.get(TICK_AIRBRAKE);
	e->tickHeld = tickHandlers.get(TICK_AIRBRAKE);
}

void module_airbrake() {
	tickHandlers.reg(TICK_AIRBRAKE, airbrake_tick);
	addEditHelp(&ent::tickHeld, airbrake_tick, "airbrake", M_AIRBRAKE_HELP);
	addEntCommand("airbrake", cmdAirbrake);
}
