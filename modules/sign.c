#include <stdlib.h>

#include "../ent.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

// Since this isn't a normal behavior, the meanings of these sliders
// are effectively defined in ent.cpp, in the sign-drawing code.
char const * const * const M_SIGN_HELP = (char const * const[]){
	"text - index in /str of text to display",
	"size - size of text, e.g. 6000",
	NULL
};

static void sign_tick(gamestate *gs, ent *me) {
}

static void cmdSign(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tickHeld = tickHandlers.get(TICK_SIGN);
}

void module_sign() {
	tickHandlers.reg(TICK_SIGN, sign_tick);
	addEditHelp(&ent::tickHeld, sign_tick, "sign", M_SIGN_HELP);
	addEntCommand("sign", cmdSign);
}
