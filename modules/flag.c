#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static int flag_whoMoves(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static const int32_t flagSize[3] = {350, 350, 350};

static ent* mk_flag(ent *owner, int32_t team) {
	ent *e = initEnt(
		owner->center, owner->vel, flagSize,
		2, 0
		T_FLAG + T_OBSTACLE + (TEAM_BIT*team), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = getWhoMovesHandler(handlerByName("flag-whomoves"));
	e->draw = getDrawHandler(handlerByName("player-draw"));
	e->pushed = getPushedHandler(handlerByName("stackem-pushed"));
	e->tick = getTickHandler(handlerByName("stackem-tick"));

	return e;
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
		mkFlag(me, team);
	}
	uStateSlider(s, 0, cooldown - 1);
}

void flag_init() {
	regWhoMovesHandler("flag-whomoves", flag_whoMoves);
	loadFile("modules/flag.scm");
}
