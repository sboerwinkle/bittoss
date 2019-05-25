#include "tinyscheme/scheme.h"

#define _size(name, count) \
const char* _name = name; \
if (list_length(sc, args) != count) { \
	fprintf(stderr, name " requires %d args\n", count); \
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
		return sc->NIL; \
	} \
	var = ivalue(tmp); \
}


#define _ent(var) \
_count++; \
ent *var; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (!is_c_ptr(tmp, 0)) { \
		fprintf(stderr, "%s arg #%d must be an ent*\n", _name, _count); \
		return sc->NIL; \
	} \
	var = (ent*)c_ptr_value(tmp); \
}


#define _pair(var1, var2) \
_count++; \
int var1, var2; \
{ \
	pointer tmp = pair_car(args); \
	args = pair_cdr(args); \
	if (list_length(sc, tmp) != 2) { \
		fprintf(stderr, "%s arg #%d must be a pair of ints\n", _name, _count); \
		return sc->NIL; \
	} \
	pointer t1 = pair_car(tmp); \
	pointer t2 = pair_car(pair_cdr(tmp)); \
	if (!is_integer(t1) || !is_integer(t2)) { \
		fprintf(stderr, "%s arg #%d is a pair, but components must be ints\n", _name, _count); \
		return sc->NIL; \
	} \
	var1 = ivalue(t1); \
	var2 = ivalue(t2); \
}
