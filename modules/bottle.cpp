#include <stdlib.h>

#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "bottle.h"
#include "common.h"
#include "player.h"

char const * const * const M_BOTTLE_HELP = (char const * const[]){
	"hand - internal use",
	"type - juice/fuel (0/1)",
	"amount",
	"max amount",
	NULL
};

enum {
	s_hand,
	s_type,
	s_amt,
	s_max,
	s_num
};

static_assert(s_type == BOTTLE_TYPE_SLIDER);
static_assert(s_amt == BOTTLE_AMT_SLIDER);

#define C_UNKNOWN 0x808080
#define C_JUICE 0x40FFFF
#define C_FUEL 0x80FF80

static void bottleRecolor(ent *e, int32_t btype) {
	if (btype == BOTTLE_JUICE) e->color = C_JUICE;
	else if (btype == BOTTLE_FUEL) e->color = C_FUEL;
	else e->color = C_UNKNOWN;
}

static void bottle_tick(gamestate *gs, ent *me) {
}

// Pretty similar to door logic.
// Bottle tries to hover at player's altitude, in the direction they're looking.
static void bottle_tick_held(gamestate *gs, ent *me) {
	// Doesn't do anything unless held by a person (or similar)
	ent *h = me->holder;
	if (!(type(h) & T_INPUTS)) return;

	// We never move on Z axis, so all our math is w/ 2D vectors here.
	int32_t dest[3];
	dest[2] = 0; // We'll need this at the end, but it's always 0.
	int32_t hand = getSlider(me, s_hand);
	if (!h->holder && getTrigger(h, hand)) {
		int32_t look[3];
		getLook(look, h);
		// Multiplier here affects how far up or down you can be looking and still get maximum extension.
		// This is pretty aggressive. Bear in mind that `look` is usually going to be in [-64, 64]
		// (bounds = the value of `axisMaxis`)
		dest[0] = look[0] * 30;
		dest[1] = look[1] * 30;
		boundVec(dest, 1000, 2);
	} else {
		dest[0] = dest[1] = 0;
	}
	// `dest` is our desired position (relative to holder)

	int32_t accel = 40;
	int32_t tmp[3];
	getPos(tmp, h, me);
	dest[0] -= tmp[0];
	dest[1] -= tmp[1];
	// `dest` is our desired displacement (from current position)
	int32_t mag = getMagnitude(dest, 2);
	// `mag` is the distance to target
	mag = getApproachSpeed(mag, accel);
	// `mag` is the approach speed to target
	boundVec(dest, mag, 2);
	// `dest` is our desired velocity (relative to holder)

	getVel(tmp, h, me);
	dest[0] -= tmp[0];
	dest[1] -= tmp[1];
	// `dest` is our desired acceleration
	boundVec(dest, accel, 2);
	// `dest` is our acceleration to actually apply
	uVel(me, dest);
}

static char bottle_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	// bottle_push is only set for "bottle2" blocks.
	// It will fill itself from bottles of a compatible type.
	if (him->tick == bottle_tick) {
		int32_t myType = getSlider(me, s_type);
		int32_t hisType = getSlider(him, s_type);
		if (myType == -1 || myType == hisType) {
			int32_t capacity = me->sliders[s_max].v - me->sliders[s_amt].v;
			int32_t hisAmt = him->sliders[s_amt].v;
			if (hisAmt < capacity) capacity = hisAmt;
			if (capacity <= 0) return 0; // Skip updating him and setting my type

			him->sliders[s_amt].v -= capacity;
			me->sliders[s_amt].v += capacity;
			bottleUpdate(gs, him);
			// not going to bottleUpdate myself, as I'm not planning for it to do anything
		}
		if (myType == -1) {
			myType = hisType;
			me->sliders[s_type].v = myType;
			bottleRecolor(me, myType);
		}
	}
	return 0;
}

void bottleUpdate(gamestate *gs, ent *e) {
	// I had meant for the bottles to shrink as they held less stuff,
	// but it looks like I don't currently support resizing. Oh well.
	if (e->sliders[s_amt].v == 0) {
		if (e->push == bottle_push) { // If this is a "bottle2", which can hold different things
			// Reset type to "unknown"
			e->sliders[s_type].v = -1;
			bottleRecolor(e, -1);
		} else {
			uDead(gs, e);
		}
	}
}

static void cmdBottle(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP_SM, s_num-1); // Regular bottles don't have the last slider (max amount)
	e->tick = tickHandlers.get(TICK_BOTTLE);
	e->tickHeld = tickHandlers.get(TICK_BOTTLE_HELD);
}

static void cmdBottle2(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP_SM, s_num);
	e->tick = tickHandlers.get(TICK_BOTTLE);
	e->tickHeld = tickHandlers.get(TICK_BOTTLE_HELD);
	e->push = pushHandlers.get(PUSH_BOTTLE);
	bottleRecolor(e, getSlider(e, s_type));
}

void module_bottle() {
	tickHandlers.reg(TICK_BOTTLE, bottle_tick);
	tickHandlers.reg(TICK_BOTTLE_HELD, bottle_tick_held);
	pushHandlers.reg(PUSH_BOTTLE, bottle_push);
	// Register "bottle2" help first, since those ents would also trip the "bottle" check.
	addEditHelp(&ent::push, bottle_push, "bottle2", M_BOTTLE_HELP);
	addEditHelp(&ent::tick, bottle_tick, "bottle", M_BOTTLE_HELP);
	addEntCommand("bottle", cmdBottle);
	addEntCommand("bottle2", cmdBottle2);
}
