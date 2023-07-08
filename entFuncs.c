#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ent.h"
#include "main.h"
#include "handlerRegistrar.h"

// TODO I think a lot of these defaults can go away

/*
void onTickHeldDefault(ent *me) {}
*/

int tickTypeDefault(ent *a, ent *b) {return 1;}

/*
void onCrushDefault(ent *me) {}
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

static push_t defaultPush;

void initEnt(
	ent *e,
	gamestate *gs,
	ent *relative,
	const int32_t *c,
	const int32_t *v,
	const int32_t *r,
	int numSliders,
	int32_t typeMask,
	int32_t collideMask
) {
	// TODO Kind of dumb to do 3 `malloc`s that will probably never be used over the lifetime of the ent.
	//      It looks like `realloc` and `free` both appropriately treat NULL as a valid pointer to a zero-length allocation,
	//      so maybe we could modify `list` to start with `max = 0` and see if anything is broken by that assumption?
	//      I know there's some stuff in the brains codebase that wouldn't like that...
	e->wires.init();
	e->wiresRm.init();
	e->wiresAdd.init();
	flushCtrls(e);
	flushMisc(e, zeroVec);
	memcpy(e->center, c, sizeof(e->center));
	memcpy(e->vel, v, sizeof(e->vel));
	memcpy(e->radius, r, sizeof(e->radius));
	e->typeMask = typeMask;
	e->collideMask = collideMask;

	setupEntState(&e->state, numSliders);

	e->whoMoves = NULL;
	e->tick = NULL;
	e->tickHeld = NULL;
	e->tickType = tickTypeDefault;
	//e->onDraw = onDrawDefault;
	e->draw = NULL;
	//e->onCrush = onCrushDefault;
	e->push = defaultPush;
	e->pushed = NULL;
	e->onFumble = onFumbleDefault;
	e->onFumbled = onFumbledDefault;
	e->onPickUp = onPickUpDefault;
	e->onPickedUp = onPickedUpDefault;
	e->okayFumble = okayFumbleDefault;
	e->okayFumbleHim = okayFumbleHimDefault;
	addEnt(gs, e, relative);
}

ent *initEnt(
	gamestate *gs,
	ent *relative,
	const int32_t *c,
	const int32_t *v,
	const int32_t *r,
	int numSliders,
	int32_t typeMask,
	int32_t collideMask
) {
	ent *ret = (ent*) calloc(1, sizeof(ent));
	initEnt(ret, gs, relative, c, v, r, numSliders, typeMask, collideMask);
	return ret;
}

void init_entFuncs() {
	defaultPush = pushHandlers.getByName("nil");
}
