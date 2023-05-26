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

static ent* mkFlag(gamestate *gs, ent *owner, int32_t team) {
	ent *e = initEnt(
		gs,
		owner->center, owner->vel, flagSize,
		2, 0,
		T_FLAG + T_OBSTACLE + (TEAM_BIT*team), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = whoMovesHandlers.getByName("flag-whomoves");
	e->draw = drawHandlers.getByName("team-draw");
	e->pushed = pushedHandlers.getByName("stackem-pushed");
	e->tick = tickHandlers.getByName("stackem-tick");

	return e;
}


static void flagSpawner_tick(gamestate *gs, ent *me) {
	entState *s = &me->state;
	int cooldown = getSlider(s, 0);
	// Right now this always makes a flag when the
	// countdown expires; if we get ent references
	// fully working, it could wait for the destruction
	// of the existing flag before making a replacement
	if (cooldown == 0) {
		cooldown = 90; // 3 seconds
		int team = getSlider(s, 1);
		mkFlag(gs, me, team);
	}
	uStateSlider(s, 0, cooldown - 1);
}

const int32_t spawnerSize[3] = {1, 1, 1};

void mkFlagSpawner(gamestate *gs, int32_t *pos, int32_t team) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs,
		pos, vel, spawnerSize,
		2, 0,
		T_WEIGHTLESS, 0
	);
	uStateSlider(&ret->state, 1, team);
	ret->tick = tickHandlers.getByName("flag-spawner-tick");
	ret->draw = drawHandlers.getByName("no-draw");
}

void flag_init() {
	whoMovesHandlers.reg("flag-whomoves", flag_whoMoves);
	tickHandlers.reg("flag-spawner-tick", flagSpawner_tick);
}
