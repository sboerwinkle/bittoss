#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

static int flag_whoMoves(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static void flag_spawner_tick(ent *me) {
	entState *s = me->state;
	int cooldown = getSlider(s, 0);
	// Right now this always makes a flag when the
	// countdown expires; if we get ent references
	// fully working, it could wait for the destruction
	// of the existing flag before making a replacement
	if (cooldown == 0) {
		cooldown = 90; // 3 seconds
		int team = getSlider(s, 1);
		// TODO Actually create the flag,
		//      for now this can be a scm call.
	}
	uStateSlider(s, 0, cooldown - 1);
}

void flag_init() {
	regWhoMovesHandler("flag-whomoves", flag_whoMoves);
	loadFile("modules/flag.scm");
}
