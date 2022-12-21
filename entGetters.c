#include "ent.h"
#include "ts_macros.h"
#include "main.h"
#include "util.h"

static pointer ts_typ_p(scheme *sc, pointer args) {
	if (list_length(sc, args) != 2) {
		fputs("typ? requires 2 args\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer E = pair_car(args);
	if (!is_c_ptr(E, 0)) {
		fputs("typ? arg 1 must be ent*\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer V = pair_car(pair_cdr(args));
	if (!is_integer(V)) {
		fputs("typ? arg 2 must be int\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer ret;
	if (((ent*) c_ptr_value(E))->typeMask & (int32_t) ivalue(V)) ret = sc->T;
	else ret = sc->F;
	ret->references++;
	return ret;
}

int bound(int x, int b) {
	if (x > b) return b;
	if (x < -b) return -b;
	return x;
}

int type(ent *e) {
	return e->typeMask;
}

static pointer ts_getType(scheme *sc, pointer args) {
	_size("get-typ", 1);
	_ent(e);
	return mk_integer(sc, type(e));
}

static pointer ts_getAxis(scheme *sc, pointer args) {
	_size("get-axis", 1);
	_ent(e);
	sc->NIL->references++;
	return cons(sc, mk_integer(sc, e->ctrl.axis1.v[0]), cons(sc, mk_integer(sc, e->ctrl.axis1.v[1]), sc->NIL));
}

static pointer ts_getLook(scheme *sc, pointer args) {
	_size("get-look", 1);
	_ent(e);
	sc->NIL->references++;
	return cons(sc, mk_integer(sc, e->ctrl.look.v[0]),
		cons(sc, mk_integer(sc, e->ctrl.look.v[1]),
		cons(sc, mk_integer(sc, e->ctrl.look.v[2]),
		sc->NIL)));
}

static pointer ts_getButton(scheme *sc, pointer args) {
	_size("get-button", 2);
	_ent(e);
	_int(i);
	pointer ret = e->ctrl.btns[i].v ? sc->T : sc->F;
	ret->references++;
	return ret;
}

static pointer ts_getTrigger(scheme *sc, pointer args) {
	_size("get-trigger", 2);
	_ent(e);
	_int(i);
	pointer ret;
	if (i >= 0 && i <= 1) {
		ret = e->ctrl.btns[i+2].v ? sc->T : sc->F;
	} else {
		ret = sc->F;
	}
	ret->references++;
	return ret;
}

static pointer ts_getPos(scheme *sc, pointer args) {
	_size("get-pos", 2);
	_ent(e1);
	_ent(e2);
	sc->NIL->references++;
	return cons(sc, mk_integer(sc, e2->center[0] - e1->center[0]),
		cons(sc, mk_integer(sc, e2->center[1] - e1->center[1]),
		cons(sc, mk_integer(sc, e2->center[2] - e1->center[2]),
		sc->NIL)));
}

void getVel(int *dest, ent *a, ent *b) {
	range(i, 3) {
		dest[i] = b->vel[i] - a->vel[i];
	}
}

static pointer ts_getVel(scheme *sc, pointer args) {
	/*
	if (list_length(sc, args) != 2) {
		fputs("get-vel requires 2 args\n", stderr);
		return sc->NIL;
	}
	pointer E1 = pair_car(args);
	pointer E2 = pair_car(pair_cdr(args));
	if (!is_c_ptr(E1, 0) || !is_c_ptr(E2, 0)) {
		fputs("get-vel args must both be ent*\n", stderr);
		return sc->NIL;
	}
	ent* e1 = (ent*) c_ptr_value(E1);
	ent* e2 = (ent*) c_ptr_value(E2);
	*/
	_size("get-vel", 2);
	_ent(e1);
	_ent(e2);
	int dest[3];
	sc->NIL->references++;
	getVel(dest, e1, e2);
	return cons(sc, mk_integer(sc, dest[0]),
		cons(sc, mk_integer(sc, dest[1]),
		cons(sc, mk_integer(sc, dest[2]),
		sc->NIL)));
}

static pointer ts_getAbsPos(scheme *sc, pointer args) {
	_size("get-abs-pos", 2);
	_ent(e);
	_vec(vec);
	int32_t *center = e->center;
	sc->NIL->references++;
	return cons(sc, mk_integer(sc, center[0] + vec[0]),
		cons(sc, mk_integer(sc, center[1] + vec[1]),
		cons(sc, mk_integer(sc, center[2] + vec[2]),
		sc->NIL)));
}

static pointer ts_getRadius(scheme *sc, pointer args) {
	_size("get-radius", 1);
	_ent(e);
	sc->NIL->references++;
	return cons(sc, mk_integer(sc, e->radius[0]),
		cons(sc, mk_integer(sc, e->radius[1]),
		cons(sc, mk_integer(sc, e->radius[2]),
		sc->NIL)));
}

static pointer ts_getHolder(scheme *sc, pointer args) {
	_size("get-holder", 1);
	_ent(e);
	if (e->holder) return mk_c_ptr(sc, e->holder, 0);
	sc->NIL->references++;
	return sc->NIL;
}

static pointer ts_countHoldees(scheme *sc, pointer args) {
	_size("count-holdees", 2);
	_ent(e);
	_func(f);
	int ret = 0;
	for (ent *child = e->holdee; child; child = child->LL.n) {
		save_from_C_call(sc);
		f->references++;
		sc->NIL->references++;
		scheme_call(sc, f, cons(sc, mk_c_ptr(sc, child, 0), sc->NIL));
		if (sc->value != sc->F && sc->value != sc->NIL) ret++;
	}
	return mk_integer(sc, ret);
}

static pointer ts_getState(scheme *sc, pointer args) {
	if (list_length(sc, args) != 1) {
		fputs("get-state requires 1 arg\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer E = pair_car(args);
	if (!is_c_ptr(E, 0)) {
		fputs("get-state arg must be an ent*\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	// TODO We need to make sure these c_ptr thinggies expire appropriately so they can't be stashed away
	return mk_c_ptr(sc, &((ent*) c_ptr_value(E))->state, 1);
}

static pointer ts_getSlider(scheme *sc, pointer args) {
	if (list_length(sc, args) != 2) {
		fputs("get-slider requires 2 args\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer S = pair_car(args);
	pointer Ix = pair_car(pair_cdr(args));
	if (!is_c_ptr(S, 1) || !is_integer(Ix)) {
		fputs("get-slider args must be entState* and int\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	entState *s = (entState*) c_ptr_value(S);
	int ix = ivalue(Ix);
	if (ix < 0 || ix >= s->numSliders) {
		fprintf(stderr, "Can't access slider %d (%d total)\n", ix, s->numSliders);
		sc->NIL->references++;
		return sc->NIL;
	}
	return mk_integer(sc, ((entState*) c_ptr_value(S))->sliders[ix].v);
}

void registerTsGetters() {
	scheme_define(sc, sc->global_env, mk_symbol(sc, "typ?"), mk_foreign_func(sc, ts_typ_p));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-type"), mk_foreign_func(sc, ts_getType));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-axis"), mk_foreign_func(sc, ts_getAxis));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-look"), mk_foreign_func(sc, ts_getLook));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-button"), mk_foreign_func(sc, ts_getButton));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-trigger"), mk_foreign_func(sc, ts_getTrigger));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-pos"), mk_foreign_func(sc, ts_getPos));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-vel"), mk_foreign_func(sc, ts_getVel));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-state"), mk_foreign_func(sc, ts_getState));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-slider"), mk_foreign_func(sc, ts_getSlider));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-radius"), mk_foreign_func(sc, ts_getRadius));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-holder"), mk_foreign_func(sc, ts_getHolder));
	scheme_define(sc, sc->global_env, mk_symbol(sc, "count-holdees"), mk_foreign_func(sc, ts_countHoldees));
	// TODO This is only for debugging, shouldn't be in the public scripting API
	scheme_define(sc, sc->global_env, mk_symbol(sc, "get-abs-pos"), mk_foreign_func(sc, ts_getAbsPos));
}
