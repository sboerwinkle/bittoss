#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static int stackem_whoMoves(ent *a, ent *b, int axis, int dir) {
	int typ = type(b);
	if (typ & T_TERRAIN) {
		return MOVE_ME;
	}

	if (axis == 2) {
		if (dir == -1 && (typ & T_HEAVY)) return MOVE_ME;
		return MOVE_HIM;
	}

	if (TEAM_MASK & typ & type(a)) return MOVE_ME;
	return MOVE_HIM;
}

int stackem_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	if (axis == 2 && dir == -1) {
		int vel[3];
		getVel(vel, me, him);
		boundVec(vel, 4, 2);
		uStateSlider(&me->state, 0, vel[0]);
		uStateSlider(&me->state, 1, vel[1]);
	}
	// Todo: Should probably do away with this entirely in favor of specifying the behavior on pickup
	return r_move;
}

static void stackem_tick(gamestate *gs, ent *me) {
	int vel[3];
	entState *s = &me->state;
	vel[2] = 0;
	vel[0] = getSlider(s, 0);
	vel[1] = getSlider(s, 1);
	uVel(me, vel);
	// Reset sliders always
	uStateSlider(s, 0, 0);
	uStateSlider(s, 1, 0);
}

static const int32_t stackemSize[3] = {450, 450, 450};

ent* mkStackem(gamestate *gs, ent *owner, const int32_t *offset) {
	// Todo: This bit might become quite common, consider moving elsewhere?
	int32_t pos[3];
	range(i, 3) {
		pos[i] = offset[i] + owner->center[i];
	}
	ent *e = initEnt(
		gs, owner,
		pos, owner->vel, stackemSize,
		2,
		T_HEAVY + T_OBSTACLE + (TEAM_MASK & type(owner)), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = whoMovesHandlers.getByName("stackem-whomoves");
	e->color = (e->typeMask & TEAM_BIT) ? 0x805555 : 0x558055;
	e->pushed = pushedHandlers.getByName("stackem-pushed");
	e->tick = tickHandlers.getByName("stackem-tick");

	return e;
}

void module_stackem() {
	whoMovesHandlers.reg("stackem-whomoves", stackem_whoMoves);
	pushedHandlers.reg("stackem-pushed", stackem_pushed);
	tickHandlers.reg("stackem-tick", stackem_tick);
}
