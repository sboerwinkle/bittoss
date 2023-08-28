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

static void logic_inner(ent *me, int32_t mode) {
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
		// I don't use this, actually almost nobody uses it,
		// but for now it's how logic gates show they're "active"
		// for anybody who cares to look (like doors)
		pushBtn(me, 2);
	}
}

static void logic_tick(gamestate *gs, ent *me) {
	int32_t mode = getSlider(me, 0);

	logic_inner(me, mode);
}

static void logic_tick_debug(gamestate *gs, ent *me) {
	int32_t mode = getSlider(me, 0);

	int32_t base;
	if (mode == 0) base = 0x007F00;
	else if (mode == 1) base = 0x7F0000;
	else base = 0x00007F;
	me->color = base + (getButton(me, 2) ? 0x808080 : 0);

	logic_inner(me, mode);
}

void module_logic() {
	tickHandlers.reg(TICK_LOGIC, logic_tick);
	tickHandlers.reg(TICK_LOGIC_DEBUG, logic_tick_debug);
	pushHandlers.reg(PUSH_LOGIC, logic_push);
}
