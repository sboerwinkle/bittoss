#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ent.h"
#include "main.h"
#include "ts_macros.h"
#include "tinyscheme/scheme.h"

int whoMovesDefault(ent *me, ent *him, byte axis, int dir) {
	int diff = (me->typeMask & (T_HEAVY|T_TERRAIN)) - (him->typeMask & (T_HEAVY|T_TERRAIN));
	if (diff > 0) return HIM;
	if (diff < 0) return ME;
	return BOTH;
}

void onTickDefault(ent *me) {}

void onTickHeldDefault(ent *me) {}

int tickTypeDefault(ent *a, ent *b) {return 1;}

/*
void onDrawDefault(ent *me, int layer, int dx, int dy) {
	registerSprite(me->img, dx-me->radius[0], dy-me->radius[1], 1);
	ent *i;
	for (i = me->holdee; i; i = i->LL.n) i->onDraw(i, layer, dx + i->center[0] - me->center[0], dy + i->center[1] - me->center[1]);
}
*/

void onCrushDefault(ent *me) {}

//default for onFree
void doNothing(ent *me) {}

void onPushDefault(ent *me, ent *him, byte axis, int dir, int displacement, int dv) {}

/*
int onPushedDefault(ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	return r_pass;
}
*/

/*
void onPushedFriction(ent *cd, ent *me, ent *leaf, ent *him) {
	if (onPushedEssentials(cd, me, leaf, him)) {
		(me->holder->onPushed)(cd, me->holder, leaf, him);
		return;
	}
	//Affect velocity
	int32_t d[2];
	int i;
	for (i = 0; i < 2; i++) {
		d[i] = him->staleVel[i] - leaf->staleVel[i];
	}
	if (cd->collisionMutual) {
		for (i = 0; i < 2; i++) {
			d[i] /= 2;
		}
	}
	accelRecursive(me, d);
}
*/

void onFumbleDefault(ent *me, ent *him) {}

void onFumbledDefault(ent *me, ent *im) {}

void onPickUpDefault(ent *me, ent *him) {}

void onPickedUpDefault(ent *me, ent *him) {}

char okayFumbleDefault(ent *me, ent *him) {
	return 0;
}

char okayFumbleHimDefault(ent *me, ent *him) {
	return 0;
}

//TODO: Once we've got scripting operating at a slightly higher level,
//this will probably just take a type identifier, and will index into a large list of handler sets.
//Alternatively, it could just take a list of handlers directly, which could be called from guile and would technically allow for more flexibility.
//TODO: Orientation is an arg here.
ent *initEnt(int32_t *c, int32_t *v, int32_t rx, int32_t ry, int numSliders, int numRefs) {
	ent *ret = (ent*) calloc(1, sizeof(ent));
	flushCtrls(ret);
	flushMisc(ret);
	memcpy(ret->center, c, sizeof(ret->center));
	memcpy(ret->vel, v, sizeof(ret->vel));
	ret->radius[0] = rx;
	ret->radius[1] = ry;

	setupEntState(&ret->state, numSliders, numRefs);

	//ret->whoMoves = whoMovesDefault;
	ret->whoMoves = sc->F;
	//ret->onTick = onTickDefault;
	ret->tick = sc->F;
	ret->onTickHeld = onTickHeldDefault;
	ret->tickType = tickTypeDefault;
	//ret->onDraw = onDrawDefault;
	ret->draw = sc->F;
	ret->onCrush = onCrushDefault;
	ret->onFree = doNothing;
	ret->onPush = onPushDefault;
	//ret->onPushed = onPushedDefault;
	ret->pushed = sc->F;
	ret->onFumble = onFumbleDefault;
	ret->onFumbled = onFumbledDefault;
	ret->onPickUp = onPickUpDefault;
	ret->onPickedUp = onPickedUpDefault;
	ret->okayFumble = okayFumbleDefault;
	ret->okayFumbleHim = okayFumbleHimDefault;
	addEnt(ret);
	return ret;
}

pointer createHelper(scheme *sc, pointer args, ent *parent, int32_t rx, int32_t ry, int32_t typeMask, int32_t collideMask) {
	_size("create-tmp", 2);
	_pair(x, y);
	_int(sliders);
	int32_t pos[2];
	pos[0] = x;
	pos[1] = y;
	int32_t vel[2] = {0, 0};
	if (parent) {
		memcpy(vel, parent->vel, sizeof(vel));
		printf("Initial velocity %d, %d\n", vel[0], vel[1]);
	}
	ent *e = initEnt(pos, vel, rx, ry, sliders, 0);
	e->typeMask = typeMask;
	e->collideMask = collideMask;
	return mk_c_ptr(sc, e, 0);
}

pointer ts_create(scheme *sc, pointer args) {
	_size("create", 7);
	_opt_ent(parent);
	_int(w);
	_int(h);
	_int(typ);
	_int(col);
	return createHelper(sc, args, parent, w, h, typ, col);
}

void setWhoMoves(ent *e, const char* func) {
	e->whoMoves = mk_symbol(sc, func);
}

static pointer setHandler(scheme *sc, pointer args, const char* name, pointer ent::*field) {
	if (list_length(sc, args) != 2) {
		fputs(name, stderr);
		fputs(" requires 2 args\n", stderr);
		return sc->NIL;
	}
	pointer E = pair_car(args);
	pointer S = pair_car(pair_cdr(args));
	if (!is_c_ptr(E, 0) || !is_symbol(S)) {
		fputs(name, stderr);
		fputs(" args must be ent* and symbol\n", stderr);
		return sc->NIL;
	}
	((ent*)c_ptr_value(E))->*field = S;
	return E;

}

static pointer ts_setTick(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-tick", &ent::tick);
}

static pointer ts_setWhoMoves(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-who-moves", &ent::whoMoves);
}

static pointer ts_setDraw(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-draw", &ent::draw);
}

static pointer ts_setPushed(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-pushed", &ent::pushed);
}

void registerTsFuncSetters() {
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-tick"), mk_foreign_func(sc, ts_setTick));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-who-moves"), mk_foreign_func(sc, ts_setWhoMoves));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-draw"), mk_foreign_func(sc, ts_setDraw));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-pushed"), mk_foreign_func(sc, ts_setPushed));

	scheme_define(sc, sc->global_env, mk_symbol(sc, "create"), mk_foreign_func(sc, ts_create));
}
