#include "list.h"
#include "main.h"
#include "ent.h"

static list<whoMoves_t> whoMovesHandlers;
static list<pushed_t> pushedHandlers;

void init_registrar() {
	whoMovesHandlers.init();
	pushedHandlers.init();
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	pushedHandlers.destroy();
}


void regWhoMovesHandler(const char* name, whoMoves_t handler) {
	int ix = whoMovesHandlers.num;
	scheme_define(sc, sc->global_env, mk_symbol(sc, name), mk_integer(sc, ix));
	whoMovesHandlers.add(handler);
}

whoMoves_t getWhoMovesHandler(int ix) {
	return whoMovesHandlers[ix];
}

void regPushedHandler(const char* name, pushed_t handler) {
	int ix = pushedHandlers.num;
	scheme_define(sc, sc->global_env, mk_symbol(sc, name), mk_integer(sc, ix));
	pushedHandlers.add(handler);
}

pushed_t getPushedHandler(int ix) {
	return pushedHandlers[ix];
}
