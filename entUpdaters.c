#include <stdio.h>
#include <stdlib.h>
#include "ent.h"
#include "main.h"
#include "ts_macros.h"

void pushBtn(ent *who, int ix) {who->ctrl.btns[ix].v2 = 1;}

void pushAxis1(ent *who, int *x) {
	int i;
	for (i = 0; i < 2; i++) {
		if (x[i] > who->ctrl.axis1.max[i]) {
			who->ctrl.axis1.max[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] > axisMaxis) {
				printf("Invalid axis component %d on dimension %c\n", x[i], "XY"[i]);
			}
#endif
		}
		if (x[i] < who->ctrl.axis1.min[i]) {
			who->ctrl.axis1.min[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] < -axisMaxis) {
				printf("Invalid axis component %d on dimension %c\n", x[i], "XY"[i]);
			}
#endif
		}
	}
}
void pushEyes(ent *who, int *x) {
	int i;
	for (i = 0; i < 3; i++) {
		if (x[i] > who->ctrl.look.max[i]) {
			who->ctrl.look.max[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] > axisMaxis) {
				printf("Invalid look component %d on dimension %d\n", x[i], i);
			}
#endif
		}
		if (x[i] < who->ctrl.look.min[i]) {
			who->ctrl.look.min[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] < -axisMaxis) {
				printf("Invalid axis component %d on dimension %d\n", x[i], i);
			}
#endif
		}
	}
}

void uCenter(ent *e, int32_t *p) {
	for (int i = 0; i < 3; i++) {
		if (p[i] > e->d_center_max[i]) {
			e->d_center_max[i] = p[i];
		}
		if (p[i] < e->d_center_min[i]) {
			e->d_center_min[i] = p[i];
		}
	}
}

void uVel(ent *e, int32_t *a) {
	for (int i = 0; i < 3; i++) {
		e->d_vel[i] += a[i];
	}
	ent *child;
	for (child = e->holdee; child; child = child->LL.n) {
		uVel(child, a);
	}
}

static pointer ts_accel(scheme *sc, pointer args) {
	_size("accel", 2);
	pointer ret = pair_car(args); // What am I even trying to accomplish here? Isn't this just `e`?
	_ent(e);
	_vec(a);
	uVel(e, a);
	ret->references++;
	return ret;
}

void uDead(ent *e) {
	e->dead_max[flipFlop_death] = 1;
}

static pointer ts_kill(scheme *sc, pointer args) {
	_size("kill", 1);
	pointer ret = pair_car(args);
	_ent(e);
	uDead(e);
	ret->references++;
	return ret;
}

void uDrop(ent *e) {
	e->drop_max[flipFlop_drop] = 1;
}

void uPickup(ent *p, ent *e) {
	if (e->holder) {
#ifndef NODEBUG_SCRIPT
		puts("Can't pickup a non-root ent!");
#endif
		//And we're not going to write a ref to values that aren't checked for a while, that's just a bad plan.
		return;
	}
	//Got to be careful
	if (e->dead || p->dead) {
#ifndef NODEBUG_SCRIPT
		puts("No pickups invovling dead things! But you might not have known.");
#endif
		return;
	}
	p->holdRoot->pickup_max[flipFlop_pickup] = 1;
	if (e->holder_max[flipFlop_pickup]) {
		if (e->holder_max[flipFlop_pickup] != p) e->holder_max[flipFlop_pickup] = e;
		return;
	}
	e->holder_max[flipFlop_pickup] = p;
}

static pointer ts_pickup(scheme *sc, pointer args) {
	_size("pickup", 2);
	pointer ret = pair_car(args);
	_ent(a);
	_ent(b);
	uPickup(a, b);
	ret->references++;
	return ret;
}

static pointer ts_drop(scheme *sc, pointer args) {
	_size("drop", 1);
	pointer ret = pair_car(args);
	_ent(e);
	uDrop(e);
	ret->references++;
	return ret;
}

entRef* uStateRef(entState *s, int ix, ent *e, int numSliders, int numRefs) {
	if (ix < 0 || ix >= s->numRefs) {
#ifndef NODEBUG_SCRIPT
		printf("Invalid ref #%d (limit is %d)\n", ix, s->numRefs);
#endif
		return NULL;
	}
	if (e->dead) {
#ifndef NODEBUG_SCRIPT
		puts("No refs to dead things! But you might not have known.");
#endif
		return NULL;
	}
	entRef *ret = (entRef*) malloc(sizeof(entRef));
	ret->e = e;
	ret->status = 1;
	setupEntState(&ret->s, numSliders, numRefs);
	ret->n = s->refs[ix];
	s->refs[ix] = ret;
	return ret;
}

void uStateSlider(entState *s, int ix, int32_t value) {
	if (ix < 0 || ix >= s->numSliders) {
#ifndef NODEBUG_SCRIPT
		printf("Invalid slider #%d (limit is %d)\n", ix, s->numSliders);
#endif
		return;
	}
	if (value > s->sliders[ix].max) s->sliders[ix].max = value;
	if (value < s->sliders[ix].min) s->sliders[ix].min = value;
}

static pointer ts_setSlider(scheme *sc, pointer args) {
	if (list_length(sc, args) != 3) {
		fputs("set-slider requires exactly 3 args\n", stderr);
		return sc->NIL;
	}
	pointer S = pair_car(args);
	args = pair_cdr(args);
	pointer Ix = pair_car(args);
	args = pair_cdr(args);
	pointer Val = pair_car(args);

	if (!is_c_ptr(S, 1) || !is_integer(Ix) || !is_integer(Val)) {
		fputs("Wrong argument types to set-slider - requires entState*, int, int\n", stderr);
		return sc->NIL;
	}

	uStateSlider((entState*)c_ptr_value(S), ivalue(Ix), ivalue(Val));
	S->references++;
	return S;
}

void uTypeMask(ent *e, uint32_t mask, char turnOn) {
	if (turnOn) {
		e->d_typeMask_max |= mask;
	} else {
		e->d_typeMask_min |= mask;
	}
}

void uCollideMask(ent *e, uint32_t mask, char turnOn) {
	if (turnOn) {
		e->d_collideMask_max |= mask;
	} else {
		e->d_collideMask_min |= mask;
	}
}

//TODO: Requests to update orientation

void registerTsUpdaters() {
	scheme_define(sc, sc->global_env, mk_symbol(sc, "accel"), mk_foreign_func(sc, ts_accel));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "pickup"), mk_foreign_func(sc, ts_pickup));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "drop"), mk_foreign_func(sc, ts_drop));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "set-slider"), mk_foreign_func(sc, ts_setSlider));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "kill"), mk_foreign_func(sc, ts_kill));
}
