#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "stackem.h"
#include "explosion.h"

static const int32_t flagSize[3] = {350, 350, 350};

static int flag_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	if (type(him) & T_FLAG) return r_die;
	return stackem_pushed(gs, me, him, axis, dir, dx, dv);
}

static void flag_crush(gamestate *gs, ent *me) {
	explode(gs, me, 4, 400);
}

static ent* mkFlag(gamestate *gs, ent *owner, int32_t team) {
	ent *e = initEnt(
		gs, owner,
		owner->center, owner->vel, flagSize,
		2,
		T_FLAG + T_OBSTACLE + (TEAM_BIT*team), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = whoMovesHandlers.getByName("move-me");
	e->draw = drawHandlers.getByName("team-draw");
	e->pushed = pushedHandlers.getByName("flag-pushed");
	e->tick = tickHandlers.getByName("stackem-tick");
	e->crush = crushHandlers.getByName("flag-crush");

	return e;
}


static void flagSpawner_tick(gamestate *gs, ent *me) {
	// TODO entState is an obsolete concept, should just be passing ent* around
	entState *s = &me->state;
	int cooldown = getSlider(s, 0);
	if (cooldown == 0) {
		if (me->wires.num) return;
		cooldown = 90; // 3 seconds
		int team = getSlider(s, 1);
		uWire(me, mkFlag(gs, me, team));
	}
	uStateSlider(s, 0, cooldown - 1);
}

const int32_t spawnerSize[3] = {1, 1, 1};

void mkFlagSpawner(gamestate *gs, int32_t *pos, int32_t team) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs, NULL,
		pos, vel, spawnerSize,
		2,
		T_WEIGHTLESS, 0
	);
	uStateSlider(&ret->state, 1, team);
	ret->tick = tickHandlers.getByName("flag-spawner-tick");
	ret->draw = drawHandlers.getByName("no-draw");
}

void flag_init() {
	tickHandlers.reg("flag-spawner-tick", flagSpawner_tick);
	crushHandlers.reg("flag-crush", flag_crush);
	pushedHandlers.reg("flag-pushed", flag_pushed);
}
