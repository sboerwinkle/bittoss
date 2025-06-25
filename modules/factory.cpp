#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "factory.h"

static ent* spawn(gamestate *gs, ent *me) {
	int numSliders = me->numSliders - FACT_SLIDERS;
	ent *e = initEnt(
		gs, me,
		me->center, me->vel, me->radius,
		numSliders, T_WEIGHTLESS, 0
	);

	e->friction = me->friction;
	e->whoMoves = whoMovesHandlers.get(getSlider(me, FACT_WHOMOVES));

	range(i, numSliders) {
		uSlider(e, i, getSlider(me, FACT_SLIDERS + i));
	}

	return e;
}

static void finalize(gamestate *gs, ent *me, ent *e) {
	e->color = getSlider(me, FACT_COLOR);
	int32_t vel[3];
	range(i, 3) vel[i] = getSlider(me, FACT_VEL + i);
	uVel(e, vel);

	int32_t misc = getSlider(me, FACT_MISC);
	if (misc & FACT_MISC_HOLD) {
		ent *h = me->holder;
		if (h && h->wires.num == 1) {
			uPickup(gs, h->wires[0], e, me->holdFlags);
		}
	}

	range(i, 4) {
		if (misc & (1 << i)) pushBtn(e, i);
	}

	uMyTypeMask(e, getSlider(me, FACT_TYPE));
	uMyCollideMask(e, getSlider(me, FACT_COLLIDE));

	e->tick = tickHandlers.get(getSlider(me, FACT_TICK));
	e->tickHeld = tickHandlers.get(getSlider(me, FACT_TICKHELD));
	e->crush = crushHandlers.get(getSlider(me, FACT_CRUSH));
	e->push = pushHandlers.get(getSlider(me, FACT_PUSH));
	e->pushed = pushedHandlers.get(getSlider(me, FACT_PUSHED));
	e->onFumble = entPairHandlers.get(getSlider(me, FACT_FUMBLE));
	e->onFumbled = entPairHandlers.get(getSlider(me, FACT_FUMBLED));
	e->onPickUp = entPairHandlers.get(getSlider(me, FACT_PICKUP));
	e->onPickedUp = entPairHandlers.get(getSlider(me, FACT_PICKEDUP));
}

static void factory_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) || getButton(me, 1)) {
		ent *e = spawn(gs, me);
		uWire(me, e);
		pushBtn(me, 2);
	}
	if (getButton(me, 2) && me->wires.num) {
		if (me->wires.num == 1) finalize(gs, me, me->wires[0]);
		wiresAnyOrder(w, me) uUnwire(me, w);
	}
}

static void factory_wires_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) || getButton(me, 1)) {
		pushBtn(me, 2);
	}
	if (getButton(me, 2)) {
		ent *h = me->holder;
		if (h && h->wires.num == 1) {
			ent *e = h->wires[0];
			wiresAnyOrder(w, me) {
				_wiresAnyOrder(__j, w2, w) {
					uWire(e, w2);
				}
			}
		}
	}
}

static const int32_t wires_radius[3] = {100, 100, 100};

ent* mkFactoryWires(gamestate *gs, ent *h) {
	int32_t pos[3];
	memcpy(pos, h->center, sizeof(pos));
	pos[2] -= h->radius[2];
	ent *e = initEnt(
		gs, h,
		pos, h->vel, wires_radius,
		0, T_WEIGHTLESS, 0
	);
	e->tick = e->tickHeld = tickHandlers.get(TICK_FACTORY_WIRES);
	e->color = 0x808080;
	uPickup(gs, h, e, HOLD_PASS);
	return e;
}

void module_factory() {
	tickHandlers.reg(TICK_FACTORY, factory_tick);
	tickHandlers.reg(TICK_FACTORY_WIRES, factory_wires_tick);
}
