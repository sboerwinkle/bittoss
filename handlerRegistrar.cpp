#include <string.h>

#include "list.h"
#include "main.h"
#include "ent.h"
#include "util.h"

static list<whoMoves_t> whoMovesHandlers;
static list<tick_t> tickHandlers;
static list<draw_t> drawHandlers;
static list<pushed_t> pushedHandlers;
static list<push_t> pushHandlers;

struct card {
	const char *name;
	int ix;
};

static list<card> catalog;

void init_registrar() {
	whoMovesHandlers.init();
	tickHandlers.init();
	drawHandlers.init();
	pushedHandlers.init();
	pushHandlers.init();
	catalog.init();
}

void destroy_registrar() {
	whoMovesHandlers.destroy();
	tickHandlers.destroy();
	drawHandlers.destroy();
	pushedHandlers.destroy();
	pushHandlers.destroy();
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
	// TODO This is all too common, needs to be refactored
	int ix = whoMovesHandlers.num;
	addCard(name, ix);
	whoMovesHandlers.add(handler);
}

whoMoves_t getWhoMovesHandler(int ix) {
	return whoMovesHandlers[ix];
}

void regTickHandler(const char* name, tick_t handler) {
	int ix = tickHandlers.num;
	addCard(name, ix);
	tickHandlers.add(handler);
}

tick_t getTickHandler(int ix) {
	return tickHandlers[ix];
}

void regDrawHandler(const char* name, draw_t handler) {
	int ix = drawHandlers.num;
	addCard(name, ix);
	drawHandlers.add(handler);
}

draw_t getDrawHandler(int ix) {
	return drawHandlers[ix];
}

void regPushedHandler(const char* name, pushed_t handler) {
	int ix = pushedHandlers.num;
	addCard(name, ix);
	pushedHandlers.add(handler);
}

pushed_t getPushedHandler(int ix) {
	return pushedHandlers[ix];
}

void regPushHandler(const char* name, push_t handler) {
	int ix = pushHandlers.num;
	addCard(name, ix);
	pushHandlers.add(handler);
}

push_t getPushHandler(int ix) {
	return pushHandlers[ix];
}
