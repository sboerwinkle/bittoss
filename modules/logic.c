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

static void logic_inner(ent *me, int32_t mode, char oldActive) {
	const char a = getButton(me, 0);
	const char b = getButton(me, 1);
	char active;
	if ((mode | 1) == 1) {
		// mode is 0 (XOR) or 1 (XNOR)
		active = a ^ b ^ mode;
	} else {
		// mode is something else (presumably 2) (AND)
		active = a && b;
	}

	if (active) {
		int32_t outputs = getSlider(me, 1);
		if (outputs & 1) {
			wiresAnyOrder(w, me) {
				pushBtn(w, 0);
			}
		}
		if (outputs & 2) {
			wiresAnyOrder(w, me) {
				pushBtn(w, 1);
			}
		}
	}
	if (active != oldActive) uMyTypeMask(me, me->typeMask ^ T_ACTIVE);
}

static void logic_tick(gamestate *gs, ent *me) {
	int32_t mode = getSlider(me, 0);
	char oldActive = !!(type(me) & T_ACTIVE);

	logic_inner(me, mode, oldActive);
}

static void logic_tick_debug(gamestate *gs, ent *me) {
	int32_t mode = getSlider(me, 0);
	char oldActive = !!(type(me) & T_ACTIVE);

	int32_t base;
	if (mode == 0) base = 0x007F00;
	else if (mode == 1) base = 0x7F0000;
	else base = 0x00007F;
	me->color = base + (oldActive ? 0x808080 : 0);

	logic_inner(me, mode, oldActive);
}

void module_logic() {
	tickHandlers.reg("logic-tick", logic_tick);
	tickHandlers.reg("logic-tick-debug", logic_tick_debug);
	pushHandlers.reg("logic-push", logic_push);
}
