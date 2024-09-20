#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"

#include "platform.h"

const int32_t platformSize[3] = {3200, 3200, 500};

static void platform_tick(gamestate *gs, ent *me) {
	// Clear all special behavior if it survives to its first tick
	me->tick = tickHandlers.get(TICK_NIL);
	me->crush = crushHandlers.get(CRUSH_NIL);
	list<ent*> &wires = me->wires;
	range(i, wires.num) {
		uUnwire(me, wires[i]);
	}
}

// This only happens if it dies immediately
static void platform_crush(gamestate *gs, ent *me) {
	list<ent*> &wires = me->wires;
	range(i, wires.num) {
		ent *e = wires[i];
		// Tampering w/ other people's sliders shouldn't be done randomly,
		// but it is managed so as not to depend on iteration order
		// (so it should be fine if you know what you're doing!)
		uSlider(e, PLAYER_CHARGE_SLIDER, 180);
		uSlider(e, PLAYER_COOLDOWN_SLIDER, 5);
	}
}

ent* mkPlatform(gamestate *gs, ent *owner, int32_t *offset, int32_t color) {
	int32_t pos[3];
	range(i, 3) pos[i] = offset[i] + owner->center[i];
	ent *e = initEnt(
		gs, owner,
		pos, owner->vel, platformSize,
		0,
		T_TERRAIN + T_WEIGHTLESS, T_TERRAIN
	);
	e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
	e->tick = tickHandlers.get(TICK_PLATFORM);
	e->crush = crushHandlers.get(CRUSH_PLATFORM);
	e->color = color;
	uWire(e, owner);
	return e;
}

void module_platform() {
	whoMovesHandlers.reg(WHOMOVES_PLATFORM, whoMovesHandlers.get(WHOMOVES_ME));
	tickHandlers.reg(TICK_PLATFORM, platform_tick);
	crushHandlers.reg(CRUSH_PLATFORM, platform_crush);
}
