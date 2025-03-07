#include <stdlib.h>

#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

char const * const * const M_BOTTLE_HELP = (char const * const[]){
	"type - material/fuel (0/1)",
	"amount",
	"max amount",
	NULL
};

#define C_JUICE 0x40FFFF
#define C_FUEL 0x80FF80

void bottleUpdate(gamestate *gs, ent *e) {
	// I had meant for the bottles to shrink as they held less stuff,
	// but it looks like I don't currently support resizing. Oh well.
	if (e->sliders[1].v == 0) uDead(gs, e);
}

static void bottle_tick(gamestate *gs, ent *me) {
}

static void bottle_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	// bottle_push is only set for "bottle2" blocks.
	// It will fill itself from bottles of a compatible type.
	if (him->tick == bottle_tick && him->push != bottle_push) {
		int32_t myType = getSlider(me, 0);
		int32_t hisType = getSlider(him, 0);
		if (myType == -1) {
			me->sliders[0].v = hisType;
			myType = hisType;
			me->color = myType ? C_FUEL : C_JUICE;
		}
		if (myType == hisType) {
			int capacity = me->sliders[2].v - me->sliders[1].v;
			if (him->sliders[1].v < capacity) capacity = him->sliders[1].v;
			him->sliders[1].v -= capacity;
			me->sliders[1].v += capacity;
			bottleUpdate(gs, him);
			// not going to bottleUpdate myself, as I'm not planning for it to do anything
		}
	}
}

static void cmdBottle(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP_SM, 2);
	e->tick = tickHandlers.get(TICK_BOTTLE);
}

static void cmdBottle2(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP_SM, 3);
	e->tick = tickHandlers.get(TICK_BOTTLE);
	e->push = pushHandlers.get(PUSH_BOTTLE);
}

void module_bottle() {
	tickHandlers.reg(TICK_BOTTLE, bottle_tick);
	pushHandlers.reg(PUSH_BOTTLE, bottle_push);
	addEditHelp(&ent::push, bottle_push, "bottle2", M_BOTTLE_HELP);
	addEditHelp(&ent::tick, bottle_tick, "bottle", M_BOTTLE_HELP);
	addEntCommand("bottle", cmdBottle);
	addEntCommand("bottle2", cmdBottle2);
}
