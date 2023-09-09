#include <stdlib.h>

#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static void seat_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	if (me->wires.num) return;
	if ((type(him) & T_INPUTS) && !getButton(him, 0)) {
		uPickup(gs, me, him, HOLD_DROP + HOLD_FREEZE + HOLD_SINGLE);
	}
}

static void seat_fumble(gamestate *gs, ent *me, ent *him) {
	// No harm in doing this always, even if we weren't connected.
	// Not like we're going to have very many extraneous fumbles anyway.
	uUnwire(me, him);
}

static void seat_pickUp(gamestate *gs, ent *me, ent *him) {
	if (!me->wires.num && (type(him) & T_INPUTS)) {
		uWire(me, him);
	}
}

static void seat_tick(gamestate *gs, ent *me) {
	wiresAnyOrder(w, me) {
		if (getButton(w, 0)) {
			uDrop(gs, w);
		}
	}
}

void module_seat() {
	pushHandlers.reg(PUSH_SEAT, seat_push);
	tickHandlers.reg(TICK_SEAT, seat_tick);
	entPairHandlers.reg(FUMBLE_SEAT, seat_fumble);
	entPairHandlers.reg(PICKUP_SEAT, seat_pickUp);
}