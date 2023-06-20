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

//TODO: Orientation is an arg here.
ent *initEnt(
	gamestate *gs,
	const int32_t *c,
	const int32_t *v,
	const int32_t *r,
	int numSliders,
	int numRefs,
	int32_t typeMask,
	int32_t collideMask
) {
	ent *ret = (ent*) calloc(1, sizeof(ent));
	// TODO Kind of dumb to do 3 `malloc`s that will probably never be used over the lifetime of the ent.
	//      It looks like `realloc` and `free` both appropriately treat NULL as a valid pointer to a zero-length allocation,
	//      so maybe we could modify `list` to start with `max = 0` and see if anything is broken by that assumption?
	//      I know there's some stuff in the brains codebase that wouldn't like that...
	ret->wires.init();
	ret->wiresRm.init();
	ret->wiresAdd.init();
	flushCtrls(ret);
	flushMisc(ret);
	memcpy(ret->center, c, sizeof(ret->center));
	memcpy(ret->vel, v, sizeof(ret->vel));
	memcpy(ret->radius, r, sizeof(ret->radius));
	ret->typeMask = typeMask;
	ret->collideMask = collideMask;

	setupEntState(&ret->state, numSliders, numRefs);

	ret->whoMoves = NULL;
	ret->tick = NULL;
	ret->tickHeld = NULL;
	ret->tickType = tickTypeDefault;
	//ret->onDraw = onDrawDefault;
	ret->draw = NULL;
	//ret->onCrush = onCrushDefault;
	ret->push = defaultPush;
	ret->pushed = NULL;
	ret->onFumble = onFumbleDefault;
	ret->onFumbled = onFumbledDefault;
	ret->onPickUp = onPickUpDefault;
	ret->onPickedUp = onPickedUpDefault;
	ret->okayFumble = okayFumbleDefault;
	ret->okayFumbleHim = okayFumbleHimDefault;
	addEnt(gs, ret);
	return ret;
}

void init_entFuncs() {
	defaultPush = pushHandlers.getByName("nil");
}
