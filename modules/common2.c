#include "../handlerRegistrar.h"
#include "../entUpdaters.h"

// Only reason this is distinct from "common.c" is because handler registration is a mess that needs to be cleaned up
// (Specifically, I can't currently assign the IDs of handlers outside of the order they're initialized in,
//  and I don't want to mess up other save games by inserting new handlers at the front)

static void cursed_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

void module_common2() {
	tickHandlers.reg("cursed-tick", cursed_tick);
}
