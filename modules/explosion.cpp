#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static int explosion_whoMoves(ent *me, ent *him, int axis, int dir) {
	return (type(him) & T_TERRAIN) ? MOVE_ME : MOVE_HIM;
}

static int explosion_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	return r_die;
}

static void explosion_tick(gamestate *gs, ent *me) {
	int timer = getSlider(me, 0);
	if (timer < 5) {
		uStateSlider(me, 0, timer+1);
	} else {
		uDead(gs, me);
	}
}

static const int32_t explosionSize[3] = {100, 100, 100};

static void mkExplosion(gamestate *gs, ent *parent, int32_t *pos, int32_t *vel) {
	ent *ret = initEnt(
		gs, parent,
		pos, vel, explosionSize,
		1,
		T_OBSTACLE + T_HEAVY, T_TERRAIN + T_OBSTACLE);
	ret->whoMoves = whoMovesHandlers.getByName("explosion-whoMoves");
	ret->color = 0xFFB233;
	ret->pushed = pushedHandlers.getByName("explosion-pushed");
	ret->tick = tickHandlers.getByName("explosion-tick");
}

void explode(gamestate *gs, ent *parent, int count, int32_t force) {
	int32_t pos[3];
	int32_t vel[3];
	int32_t width[3];
	range(i, 3) {
		width[i] = parent->radius[i] - explosionSize[i];
	}
	// We're going to do the math for `pos` inside an int32_t, which assumes `2*count*width[i]` doesn't exceed size limits.
	// I really hope that's the case, or that's an incredibly insane explosion.
#define foo(ix, counter) \
pos[ix] = parent->center[ix] + 2 * width[ix] * counter/(count-1) - width[ix]; \
vel[ix] = parent->vel[ix] + 2 * force * counter/(count-1) - force

	range(i, count) {
		foo(0, i);
		range(j, count) {
			foo(1, j);
			range(k, count) {
				foo(2, k);
				mkExplosion(gs, parent, pos, vel);
			}
		}
	}
#undef foo
}

void module_explosion() {
	whoMovesHandlers.reg("explosion-whoMoves", explosion_whoMoves);
	pushedHandlers.reg("explosion-pushed", explosion_pushed);
	tickHandlers.reg("explosion-tick", explosion_tick);
}
