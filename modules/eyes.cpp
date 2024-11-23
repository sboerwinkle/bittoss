#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"

static const int32_t eyeSize[3] = {100, 100, 100};
static const int32_t eyeInset = 50;

// Deprecated, you probably want FUMBLED_DECOR instead (/decor)
static void eye_tick(gamestate *gs, ent *me) {
	// First time we have a tick while *not* held, set to actually collide
	// and then clear the tick handler as we have no further work
	uMyTypeMask(me, me->typeMask | T_DEBRIS);
	uMyCollideMask(me, me->collideMask | T_TERRAIN | T_OBSTACLE | T_DEBRIS);
	// It's fine to set this directly in a tick handler,
	// handlers are considered internal state that no other ent should be looking at anyway
	me->tick = tickHandlers.get(TICK_NIL);
}

static void eye_tick_held(gamestate *gs, ent *me) {
	ent *parent = me->holder;
	int32_t vec[3], pos[3], vel[3];
	getLook(vec, parent);
	int32_t component = abs(vec[2]);
	int32_t r = parent->radius[2] - eyeInset;
	range(i, 2) {
		int32_t i_comp = abs(vec[i]);
		int32_t i_r = parent->radius[i] - eyeInset;
		if (i_comp * r > component * i_r) {
			r = i_r;
			component = i_comp;
		}
	}
	if (!component) return;
	getPos(pos, parent, me);
	getVel(vel, parent, me);
	range(i, 3) {
		// Another multiplication where we don't upgrade to int64_t,
		// it should probably be in bounds given that axisMaxis is 64
		vec[i] = r * vec[i] / component - pos[i] - vel[i];
	}
	uVel(me, vec);
}

static void vehicle_eye_tick_held(gamestate *gs, ent *me) {
	int32_t vec[3] = {0, 0, 0};
	int32_t tmp[3];
	// It's assumed we're wired to a seat, which is wired to (at most) an ent w/ an axis
	wiresAnyOrder(w, me) {
		wiresAnyOrder(e, w) {
			getAxis(tmp, e);
			range(i, 2) vec[i] += tmp[i];
		}
	}

	ent *p = me->holder;
	getPos(tmp, p, me);
	range(i, 3) {
		vec[i] = vec[i] * p->radius[i] / axisMaxis - tmp[i];
	}

	uCenter(me, vec);
}

ent* mkEye(gamestate *gs, ent *parent) {
	ent *ret = initEnt(
		gs, parent,
		parent->center, parent->vel, eyeSize,
		0,
		T_DECOR | T_NO_DRAW_SELF, 0);
	ret->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
	ret->color = 0xFFFFFF;
	ret->tickHeld = tickHandlers.get(TICK_HELD_EYE);
	ret->onFumbled = entPairHandlers.get(FUMBLED_DECOR);

	uPickup(gs, parent, ret, HOLD_DROP);

	return ret;
}

void module_eyes() {
	tickHandlers.reg(TICK_EYE, eye_tick);
	tickHandlers.reg(TICK_HELD_EYE, eye_tick_held);
	tickHandlers.reg(TICK_HELD_VEH_EYE, vehicle_eye_tick_held);
}
