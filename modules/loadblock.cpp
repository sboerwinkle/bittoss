#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"
#include "../main.h"

#include "loadblock.h"

char const * const * const M_LOADBLOCK_HELP = (char const * const[]){
	"file - index in /str of filename to load",
	"cooldown - internal counter to prevent rapid loads",
	NULL
};

static void loadblock_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	if (getSlider(me, 1) > 0) return;

	list<player> &ps = gs->players;
	range(i, ps.num) {
		player *p = &ps[i];
		if (p->entity == him) {
			requestLoad(gs, i, getSlider(me, 0));
			// Immediate slider update here prevents multiple loads
			// if some players trigger this block at the same time.
			me->sliders[1].v = 15;
		}
	}
}

static void loadblock_tick(gamestate *gs, ent *me) {
	int32_t cooldown = getSlider(me, 1);
	if (cooldown > 0) uSlider(me, 1, cooldown - 1);
}

static void cmdLoadblock(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->push = pushHandlers.get(PUSH_LOADBLOCK);
	e->tick = e->tickHeld = tickHandlers.get(TICK_LOADBLOCK);
}

void module_loadblock() {
	pushHandlers.reg(PUSH_LOADBLOCK, loadblock_push);
	tickHandlers.reg(TICK_LOADBLOCK, loadblock_tick);
	addEditHelp(&ent::push, loadblock_push, "loadblock", M_LOADBLOCK_HELP);
	addEntCommand("loadblock", cmdLoadblock);
}
