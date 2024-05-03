#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static void dust_tick(gamestate *gs, ent *me) {
	int32_t time = getSlider(me, 3);
	if (time <= -8) {
		uDead(gs, me);
		return;
	}
	if (time > 0) {
		int32_t v[3];
		range(i, 3) v[i] = getSlider(me, i);
		uVel(me, v);
	}
	uSlider(me, 3, time-1);
}

ent* mkDust(gamestate *gs, ent *owner, const int32_t *pos, const int32_t *vel, const int32_t *drift, int32_t size, int32_t color) {
	int time = random(gs) % 8 + 4;
	size = size * (random(gs) % 11 + 10) / 20;
	int32_t s[3] = {size, size, size};
	int32_t v[3];
	range(i, 3) {
		v[i] = vel[i] + time*drift[i];
	}
	ent *e = initEnt(
		gs, owner,
		pos, v, s,
		4,
		T_WEIGHTLESS, 0
	);
	e->tick = tickHandlers.get(TICK_DUST);
	e->color = color;
	e->sliders[0].v = -drift[0];
	e->sliders[1].v = -drift[1];
	e->sliders[2].v = -drift[2];
	e->sliders[3].v = time;

	return e;
}

void mkDustCloud(
		gamestate *gs,
		ent *owner,
		const int32_t *pos,
		const int32_t *vel,
		const int32_t *r,
		int32_t divisor,
		int32_t size,
		int32_t color,
		int32_t count
) {
	range(_c, count) {
		int32_t p[3];
		int32_t drift[3];
		range(i, 3) {
			p[i] = random(gs) % (2*r[i]+1) - r[i];
			drift[i] = p[i] / divisor;
			p[i] += pos[i];
		}
		mkDust(gs, owner, p, vel, drift, size, color);
	}
}

void module_dust() {
	tickHandlers.reg(TICK_DUST, dust_tick);
}
