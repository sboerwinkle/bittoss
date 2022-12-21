#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ent.h"
#include "main.h"
#include "handlerRegistrar.h"
#include "ts_macros.h"
#include "tinyscheme/scheme.h"

void onTickDefault(ent *me) {}

/*
void onTickHeldDefault(ent *me) {}
*/

int tickTypeDefault(ent *a, ent *b) {return 1;}

/*
void onDrawDefault(ent *me, int layer, int dx, int dy) {
	registerSprite(me->img, dx-me->radius[0], dy-me->radius[1], 1);
	ent *i;
	for (i = me->holdee; i; i = i->LL.n) i->onDraw(i, layer, dx + i->center[0] - me->center[0], dy + i->center[1] - me->center[1]);
}

void onCrushDefault(ent *me) {}
*/

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
ent *initEnt(const int32_t *c, const int32_t *v, const int32_t *r, int numSliders, int numRefs) {
	ent *ret = (ent*) calloc(1, sizeof(ent));
	flushCtrls(ret);
	flushMisc(ret);
	memcpy(ret->center, c, sizeof(ret->center));
	memcpy(ret->vel, v, sizeof(ret->vel));
	memcpy(ret->radius, r, sizeof(ret->radius));

	setupEntState(&ret->state, numSliders, numRefs);

	sc->F->references += 6;
	ret->whoMoves = NULL;
	//ret->onTick = onTickDefault;
	ret->tick = sc->F;
	//ret->onTickHeld = onTickHeldDefault;
	ret->tickHeld = sc->F;
	ret->tickType = tickTypeDefault;
	//ret->onDraw = onDrawDefault;
	ret->draw = sc->F;
	//ret->onCrush = onCrushDefault;
	ret->crush = sc->F;
	ret->onFree = doNothing;
	ret->onPush = onPushDefault;
	//ret->onPushed = onPushedDefault;
	ret->pushed = NULL;
	ret->onFumble = onFumbleDefault;
	ret->onFumbled = onFumbledDefault;
	ret->onPickUp = onPickUpDefault;
	ret->onPickedUp = onPickedUpDefault;
	ret->okayFumble = okayFumbleDefault;
	ret->okayFumbleHim = okayFumbleHimDefault;
	addEnt(ret);
	return ret;
}

pointer createHelper(scheme *sc, pointer args, ent *parent, int32_t *r, int32_t typeMask, int32_t collideMask) {
	_size("create-tmp", 2);
	_vec(pos);
	_int(sliders);
	int32_t vel[3] = {0, 0, 0};
	if (parent) {
		memcpy(vel, parent->vel, sizeof(vel));
		// TODO Should this be "center" or "old" or what
		int32_t* p_pos = parent->center;
		for (int i = 0; i < 3; i++) pos[i] += p_pos[i];
	}
	ent *e = initEnt(pos, vel, r, sliders, 0);
	e->typeMask = typeMask;
	e->collideMask = collideMask;
	return mk_c_ptr(sc, e, 0);
}

pointer ts_create(scheme *sc, pointer args) {
	_size("create", 6);
	_opt_ent(parent);
	_vec(r);
	_int(typ);
	_int(col);
	return createHelper(sc, args, parent, r, typ, col);
}

static pointer setHandler(scheme *sc, pointer args, const char* name, pointer ent::*field) {
	if (list_length(sc, args) != 2) {
		fputs(name, stderr);
		fputs(" requires 2 args\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer E = pair_car(args);
	pointer S = pair_car(pair_cdr(args));
	if (!is_c_ptr(E, 0) || !is_closure(S)) {
		fputs(name, stderr);
		fputs(" args must be ent* and lambda\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer* target = &(((ent*)c_ptr_value(E))->*field);
	decrem(sc, *target);
	*target = S;
	S->references++;

	E->references++;
	return E;
}

static pointer ts_setTick(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-tick", &ent::tick);
}

static pointer ts_setTickHeld(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-tick-held", &ent::tickHeld);
}

static pointer ts_setWhoMoves(scheme *sc, pointer args) {
	_size("set-who-moves", 2);
	pointer ret = pair_car(args);
	_ent(e);
	_int(i);
	e->whoMoves = getWhoMovesHandler(i);
	ret->references++;
	return ret;
}

static pointer ts_setDraw(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-draw", &ent::draw);
}

static pointer ts_setPushed(scheme *sc, pointer args) {
	_size("set-pushed", 2);
	pointer ret = pair_car(args);
	_ent(e);
	_int(i);
	e->pushed = getPushedHandler(i);
	ret->references++;
	return ret;
}

static pointer ts_setCrush(scheme *sc, pointer args) {
	return setHandler(sc, args, "set-crush", &ent::crush);
}

void registerTsFuncSetters() {
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-tick"), mk_foreign_func(sc, ts_setTick));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-tick-held"), mk_foreign_func(sc, ts_setTickHeld));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-who-moves"), mk_foreign_func(sc, ts_setWhoMoves));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-draw"), mk_foreign_func(sc, ts_setDraw));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-pushed"), mk_foreign_func(sc, ts_setPushed));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-crush"), mk_foreign_func(sc, ts_setCrush));

	scheme_define(sc, sc->global_env, mk_symbol(sc, "create"), mk_foreign_func(sc, ts_create));
}
