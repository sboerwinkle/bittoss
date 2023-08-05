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
		0,
		T_HEAVY + T_OBSTACLE + (TEAM_MASK & type(owner)), T_TERRAIN + T_OBSTACLE
	);
	// TODO Unsatisfied with how "modules" share stuff at the moment,
	//      need some better way to do this. We do want to be sure that
	//      all our handlers are registered, however.
	//      What about several giant enums of stuff pasted together at compile time,
	//      so we know every handler's index before they're even assigned?
	e->whoMoves = whoMovesHandlers.getByName("stackem-whomoves");
	e->color = (e->typeMask & TEAM_BIT) ? 0x805555 : 0x558055;

	return e;
}

void module_stackem() {
	whoMovesHandlers.reg("stackem-whomoves", stackem_whoMoves);

	// At some point I actually need a way to stop constantly breaking my savegame files.
	// For now, I need to do this so the spacing doesn't get off in these handler registrars,
	// even though I no longer need these handlers to exist.
	pushedHandlers.reg("stackem-pushed", NULL);
	tickHandlers.reg("stackem-tick", NULL);
}
