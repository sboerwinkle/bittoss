#include "tinyscheme/scheme.h"

#define _size(name, count) \
const char* _name = name; \
if (list_length(sc, args) != count) { \
	fprintf(stderr, name " requires %d args\n", count); \
	sc->NIL->references++; \
	return sc->NIL; \
} \
int _count = 0


#define _int(var) \
_count++; \
int var; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (!is_integer(tmp)) { \
		fprintf(stderr, "%s arg #%d must be an int\n", _name, _count); \
		sc->NIL->references++; \
		return sc->NIL; \
	} \
	var = ivalue(tmp); \
}


#define _opt_ent(var) \
_count++; \
ent *var; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (tmp == sc->NIL) { \
		var = NULL; \
	} else { \
		if (!is_c_ptr(tmp, 0)) { \
			fprintf(stderr, "%s arg #%d must be an ent*\n", _name, _count); \
			sc->NIL->references++; \
			return sc->NIL; \
		} \
		var = (ent*)c_ptr_value(tmp); \
	} \
}


#define _ent(var) \
_count++; \
ent *var; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (!is_c_ptr(tmp, 0)) { \
		fprintf(stderr, "%s arg #%d must be an ent*\n", _name, _count); \
		sc->NIL->references++; \
		return sc->NIL; \
	} \
	var = (ent*)c_ptr_value(tmp); \
}


#define _func(var) \
_count++; \
pointer var = pair_car(args); \
args = pair_cdr(args); \
if (!is_closure(var)) { \
	fprintf(stderr, "%s arg #%d must be a function\n", _name, _count); \
	sc->NIL->references++; \
	return sc->NIL; \
}


#define _pair(var1, var2) \
_count++; \
int var1, var2; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (list_length(sc, tmp) != 2) { \
		fprintf(stderr, "%s arg #%d must be a pair of ints\n", _name, _count); \
		sc->NIL->references++; \
		return sc->NIL; \
	} \
	pointer t1 = pair_car(tmp); \
	pointer t2 = pair_car(pair_cdr(tmp)); \
	if (!is_integer(t1) || !is_integer(t2)) { \
		fprintf(stderr, "%s arg #%d is a pair, but components must be ints\n", _name, _count); \
		sc->NIL->references++; \
		return sc->NIL; \
	} \
	var1 = ivalue(t1); \
	var2 = ivalue(t2); \
}

#define _vec(var) \
_count++; \
int32_t var[3]; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (list_length(sc, tmp) != 3) { \
		fprintf(stderr, "%s arg #%d must be a list of 3 ints\n", _name, _count); \
		sc->NIL->references++; \
		return sc->NIL; \
	} \
	for (int _i = 0; _i < 3; _i++) { \
		pointer t = pair_car(tmp); \
		tmp = pair_cdr(tmp); \
		if (!is_integer(t)) { \
			fprintf(stderr, "%s arg #%d is a list, but component %d is not an int\n", _name, _count, _i); \
			sc->NIL->references++; \
			return sc->NIL; \
		} \
		var[_i] = ivalue(t); \
	} \
}
