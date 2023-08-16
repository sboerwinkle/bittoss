#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static void logic_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	// All kinds of stuff can be pushed, some of it invisible or otherwise not "real".
	// This should be good enough to not have phantom activations
	if (type(him) & (T_TERRAIN | T_OBSTACLE | T_DEBRIS)) pushBtn(me, 0);
}

static void logic_tick(gamestate *gs, ent *me) {
	char inverted = getSlider(me, 1) > 0;
	if (getTrigger(me, !inverted)) {
		inverted = !inverted;
		uStateSlider(me, 1, inverted);
	}

	char active;

	if (inverted == getButton(me, 0)) {
		active = 0;
	} else {
		active = 1;
		int32_t outputs = getSlider(me, 0);
		range(i, 4) {
			if (outputs & (1 << i)) {
				wiresAnyOrder(w, me) {
					pushBtn(w, i);
				}
			}
		}
	}
	if (active != !!(type(me) & T_ACTIVE)) uMyTypeMask(me, me->typeMask ^ T_ACTIVE);
}

static void logic_tick_debug(gamestate *gs, ent *me) {
	me->color = (getSlider(me, 1) ? 0x7F0000 : 0x007F00) + ((type(me) & T_ACTIVE) ? 0x808080 : 0);
}

void module_logic() {
	tickHandlers.reg("logic-tick", logic_tick);
	tickHandlers.reg("logic-tick-debug", logic_tick_debug);
	pushHandlers.reg("logic-push", logic_push);
}
