#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

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

static void stackem_draw(ent *e) {
	int typ = type(e);
	drawEnt(
		e,
		(typ & TEAM_BIT) ? 0.5 : 0.3,
		(typ & (2*TEAM_BIT)) ? 0.5 : 0.3,
		(typ & (4*TEAM_BIT)) ? 0.5 : 0.3
	);
}

static int stackem_pushed(ent *me, ent *him, int axis, int dir, int dx, int dv) {
	if (axis == 2 && dir == -1) {
		int vel[3];
		getVel(vel, me, him);
		uStateSlider(&me->state, 0, bound(vel[0], 4));
		uStateSlider(&me->state, 1, bound(vel[1], 4));
	}
	// I'm using this more in the flag, which can actually be picked up.
	// Maybe I should change this?
	return r_move;
}

static void stackem_tick(ent *me) {
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

ent* mk_stackem(ent *owner, const int32_t *offset) {
	// Todo: This bit might become quite common, consider moving elsewhere?
	int32_t pos[3];
	range(i, 3) {
		pos[i] = offset[i] + owner->center[i];
	}
	ent *e = initEnt(
		pos, owner->vel, stackemSize,
		2, 0
		T_HEAVY + T_OBSTACLE + (TEAM_MASK & type(owner)), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = getWhoMovesHandler(handlerByName("stackem-whomoves"));
	e->draw = getDrawHandler(handlerByName("stackem-draw"));
	e->pushed = getPushedHandler(handlerByName("stackem-pushed"));
	e->tick = getTickHandler(handlerByName("stackem-tick"));

	return e;
}

void stackem_init() {
	regWhoMovesHandler("stackem-whomoves", stackem_whoMoves);
	regDrawHandler("stackem-draw", stackem_draw);
	regPushedHandler("stackem-pushed", stackem_pushed);
	regTickHandler("stackem-tick", stackem_tick);
}
