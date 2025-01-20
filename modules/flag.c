#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "stackem.h"
#include "explosion.h"

static const int32_t flagSize[3] = {350, 350, 350};

static char flag_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	return (type(him) & T_FLAG) ? 1 : 0;
}

static void flag_crush(gamestate *gs, ent *me) {
	explode(gs, me, 4, 800);
}

static ent* mkFlag(gamestate *gs, ent *owner, int32_t team) {
	ent *e = initEnt(
		gs, owner,
		owner->center, owner->vel, flagSize,
		1, // Slider here is for handedness, since this is equipment
		T_FLAG + T_EQUIP + T_OBSTACLE + (TEAM_BIT*team) + T_NO_DRAW_SELF, T_TERRAIN + T_OBSTACLE
	);
	e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
	e->color = (e->typeMask & TEAM_BIT) ? 0xFF8080 : 0x80FF80;
	e->pushed = pushedHandlers.get(PUSHED_FLAG);
	e->crush = crushHandlers.get(CRUSH_FLAG);

	return e;
}

static void flagSpawner_tick(gamestate *gs, ent *me) {
	int cooldown = getSlider(me, 0);
	if (cooldown == 0) {
		if (me->wires.num) return;
		cooldown = 45; // 3 seconds
		int team = getSlider(me, 1);
		uWire(me, mkFlag(gs, me, team));
	}
	uSlider(me, 0, cooldown - 1);
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
	// TODO This may remove the need for the "do a flush after map load" if we find a more elegant way to handle state changes on newly created ents - it's a recurring problem.
	uSlider(ret, 1, team);
	ret->tick = tickHandlers.get(TICK_FLAG_SPAWNER);
	ret->color = -1;
}

void module_flag() {
	tickHandlers.reg(TICK_FLAG_SPAWNER, flagSpawner_tick);
	crushHandlers.reg(CRUSH_FLAG, flag_crush);
	pushedHandlers.reg(PUSHED_FLAG, flag_pushed);
}
