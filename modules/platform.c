#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

static int platform_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_HIM;
}

void platform_init() {
	regWhoMovesHandler("platform-whomoves", platform_whoMoves);
	loadFile("modules/platform.scm");
}
