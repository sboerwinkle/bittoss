#include <stdlib.h>

#include "../ent.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

static void sign_tick(gamestate *gs, ent *me) {
}

static void cmdSign(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 2);
	e->tickHeld = tickHandlers.get(TICK_SIGN);
}

void module_sign() {
	tickHandlers.reg(TICK_SIGN, sign_tick);
	addEntCommand("sign", cmdSign);
}
