#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "logic.h"

char const * const * const M_LOGIC_HELP = (char const * const[]) {
	"mode - 0:XOR  1:XNOR  2:AND",
	"output - bitfield of signal(s) to send. 0-3.",
	NULL
};

char const * const * const M_TIMER_HELP = (char const * const[]) {
	"delay - how many frames to stay OFF when it should be ON",
	"output - bitfield of signal(s) to send. 0-3.",
	"counter - internal delay counter",
	NULL
};

char const * const * const M_ADJUST_HELP = (char const * const[]) {
	"slider",
	"value",
	NULL
};

char const * const * const M_DEPLETE_HELP = (char const * const[]) {
	"slider - which slider of the holder to subtract from",
	"amt - amount to require and subtract",
	"output - bitfield of signal(s) to send. 0-3.",
	NULL
};

char const * const * const M_DEMOLISH_HELP = (char const * const[]) {
	NULL // no sliders at all here actually
};

static void logic_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	// All kinds of stuff can be pushed, some of it invisible or otherwise not "real".
	// This should be good enough to not have phantom activations
	if (type(him) & (T_TERRAIN | T_OBSTACLE | T_DEBRIS)) pushBtn(me, 0);
}

static char logic_common_input(ent *me, int32_t mode) {
	const char a = getButton(me, 0);
	const char b = getButton(me, 1);
	if ((mode | 1) == 1) {
		// mode is 0 (XOR) or 1 (XNOR)
		return a ^ b ^ mode;
	} else {
		// mode is something else (presumably 2 (AND))
		return a && b;
	}
}

static void logic_common_output(ent *me, int sl) {
	int32_t outputs = getSlider(me, sl);
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

static void timer_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) ^ getButton(me, 1)) {
		// No output, reset timer
		uSlider(me, 2, getSlider(me, 0));
		return;
	}

	// Else, we *want* to output.
	int32_t timer = getSlider(me, 2);
	if (timer) {
		// Not ready to output yet.
		uSlider(me, 2, timer-1);
		return;
	}

	// Otherwise, output.
	logic_common_output(me, 1);
}

static void adjust_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) ^ getButton(me, 1)) {
		int32_t slider = getSlider(me, 0);
		int32_t value = getSlider(me, 1);
		wiresAnyOrder(w, me) {
			uSlider(w, slider, value);
		}
	}
}

static void deplete_tick(gamestate *gs, ent *me) {
	ent *h = me->holder;
	if (!h) return;
	if (getButton(me, 0) ^ getButton(me, 1)) {
		int32_t sl = getSlider(me, 0);
		int32_t amt = getSlider(me, 1);
		if (sl < h->numSliders && h->sliders[sl].v >= amt) {
			h->sliders[sl].v -= amt;
			logic_common_output(me, 2);
		}
	}
}

static void logic_inner(ent *me, int32_t mode) {
	if (logic_common_input(me, mode)) {
		logic_common_output(me, 1);
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

static void randomazzo_tick(gamestate *gs, ent *me) {
	if (!logic_common_input(me, getSlider(me, 0))) return;
	// Now for the actual hard work...
	int options = 0;
	wiresAnyOrder(w, me) {
		if (!getButton(w, 0)) options++;
	}
	if (!options) return;
	int roll = random(gs) % options;
	wiresAnyOrder(w2, me) {
		if (getButton(w2, 0)) continue;
		if (roll--) continue;

		int32_t outputs = getSlider(me, 1);
		if (outputs & 1) pushBtn(w2, 0);
		if (outputs & 2) pushBtn(w2, 1);
		pushBtn(me, 2);
		return;
	}
}

static void demolish_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0)) uDead(gs, me);
}

static void cmdLogic(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tick = tickHandlers.get(TICK_LOGIC);
	e->tickHeld = tickHandlers.get(TICK_LOGIC);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdLogicDebug(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tick = tickHandlers.get(TICK_LOGIC_DEBUG);
	e->tickHeld = tickHandlers.get(TICK_LOGIC_DEBUG);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdRand(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tick = tickHandlers.get(TICK_RAND);
	e->tickHeld = tickHandlers.get(TICK_RAND);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdTimer(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 3);
	e->tick = tickHandlers.get(TICK_TIMER);
	e->tickHeld = tickHandlers.get(TICK_TIMER);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdAdjust(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tick = tickHandlers.get(TICK_ADJUST);
	e->tickHeld = tickHandlers.get(TICK_ADJUST);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdDeplete(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 3);
	e->tick = tickHandlers.get(TICK_DEPLETE);
	e->tickHeld = tickHandlers.get(TICK_DEPLETE);
	e->push = pushHandlers.get(PUSH_LOGIC);
}

static void cmdDemolish(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 0);
	e->tick = tickHandlers.get(TICK_DEMOLISH);
	e->tickHeld = tickHandlers.get(TICK_DEMOLISH);
}

void module_logic() {
	tickHandlers.reg(TICK_LOGIC, logic_tick);
	tickHandlers.reg(TICK_LOGIC_DEBUG, logic_tick_debug);
	tickHandlers.reg(TICK_RAND, randomazzo_tick);
	tickHandlers.reg(TICK_TIMER, timer_tick);
	tickHandlers.reg(TICK_ADJUST, adjust_tick);
	tickHandlers.reg(TICK_DEPLETE, deplete_tick);
	tickHandlers.reg(TICK_DEMOLISH, demolish_tick);
	pushHandlers.reg(PUSH_LOGIC, logic_push);
	addEditHelp(&ent::tickHeld, logic_tick, "logic", M_LOGIC_HELP);
	addEditHelp(&ent::tickHeld, logic_tick_debug, "logic_debug", M_LOGIC_HELP);
	addEditHelp(&ent::tickHeld, randomazzo_tick, "rand", M_LOGIC_HELP);
	addEditHelp(&ent::tickHeld, timer_tick, "timer", M_TIMER_HELP);
	addEditHelp(&ent::tickHeld, adjust_tick, "adjust", M_ADJUST_HELP);
	addEditHelp(&ent::tickHeld, deplete_tick, "deplete", M_DEPLETE_HELP);
	addEditHelp(&ent::tickHeld, demolish_tick, "demolish", M_DEMOLISH_HELP);
	addEntCommand("logic", cmdLogic);
	addEntCommand("logic_debug", cmdLogicDebug);
	addEntCommand("rand", cmdRand);
	addEntCommand("timer", cmdTimer);
	addEntCommand("adjust", cmdAdjust);
	addEntCommand("deplete", cmdDeplete);
	addEntCommand("demolish", cmdDemolish);
}
