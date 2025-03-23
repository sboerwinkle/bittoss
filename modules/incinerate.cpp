#include <math.h>

#include "../util.h"
#include "../ent.h"
#include "../edit.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

char const * const * const M_INCINERATE_HELP = (char const * const[]) {
	"range - along all axes. Anything passing fully beyond this range dies.",
	NULL
};

static void incin_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) ^ getButton(me, 1)) return;

	int32_t range = getSlider(me, 0);
	if (range <= 0) return; // Invalid. Important to cover 0 so you can make one in edit mode without losing everything...
	int32_t vec[3];
	for (ent *e = gs->ents; e; e = e->ll.n) {
		getPos(vec, me, e);
		range(i, 3) {
			if (abs(vec[i]) - e->radius[i] >= range) {
				uDead(gs, e);
				goto next;
			}
		}
		next:;
	}
}

static void cmdIncinerate(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 1);
	e->tick = tickHandlers.get(TICK_INCINERATE);
	e->tickHeld = tickHandlers.get(TICK_INCINERATE);
}

void module_incinerate() {
	tickHandlers.reg(TICK_INCINERATE, incin_tick);
	addEditHelp(&ent::tickHeld, incin_tick, "incinerate", M_INCINERATE_HELP);
	addEntCommand("incinerate", cmdIncinerate);
}
