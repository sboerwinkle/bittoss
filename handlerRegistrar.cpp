#include <string.h>

#include "list.h"
#include "main.h"
#include "ent.h"
#include "util.h"

static list<whoMoves_t> whoMovesHandlers;
static list<pushed_t> pushedHandlers;

struct card {
	const char *name;
	int ix;
};

static list<card> catalog;

void init_registrar() {
	whoMovesHandlers.init();
	pushedHandlers.init();
	catalog.init();
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	pushedHandlers.destroy();
	catalog.destroy();
}

// If we want to eventually migrate away from using Scheme,
// we will need some way for "modules" to communicate with each other that doesn't require
// all sorts of header files.
static void addCard(const char* name, int ix) {
	range(i, catalog.num) {
		if (!strcmp(catalog[i].name, name)) {
			fprintf(stderr, "handlerRegistrar: catalog name \"%s\" already in use\n", name);
			return;
		}
	}
	card &c = catalog.add();
	c.name = name;
	c.ix = ix;
}

int handlerByName(const char* name) {
	range(i, catalog.num) {
		if (!strcmp(catalog[i].name, name)) {
			return catalog[i].ix;
		}
	}
	fprintf(stderr, "handlerRegistrar: No entry for name \"%s\" found\n", name);
	return 0;
}

void regWhoMovesHandler(const char* name, whoMoves_t handler) {
	int ix = whoMovesHandlers.num;
	addCard(name, ix);
	scheme_define(sc, sc->global_env, mk_symbol(sc, name), mk_integer(sc, ix));
	whoMovesHandlers.add(handler);
}

whoMoves_t getWhoMovesHandler(int ix) {
	return whoMovesHandlers[ix];
}

void regPushedHandler(const char* name, pushed_t handler) {
	int ix = pushedHandlers.num;
	addCard(name, ix);
	scheme_define(sc, sc->global_env, mk_symbol(sc, name), mk_integer(sc, ix));
	pushedHandlers.add(handler);
}

pushed_t getPushedHandler(int ix) {
	return pushedHandlers[ix];
}
