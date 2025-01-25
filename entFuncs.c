#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ent.h"
#include "main.h"
#include "handlerRegistrar.h"

#include "entFuncs.h"

char okayFumbleDefault(ent *me, ent *him) {
	return 0;
}

char okayFumbleHimDefault(ent *me, ent *him) {
	return 0;
}

static push_t defaultPush;
static entPair_t defaultEntPair;

void prepNewSliders(ent *e) {
	range(i, e->numSliders) {
		e->sliders[i].max = INT32_MIN;
		e->sliders[i].min = INT32_MAX;
	}
}

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
	e->newTypeMask = e->typeMask = typeMask;
	e->newCollideMask = e->collideMask = collideMask;

	e->numSliders = numSliders;
	e->sliders = (slider*) calloc(numSliders, sizeof(slider));
	prepNewSliders(e);

	memcpy(e->center, c, sizeof(e->center));
	memcpy(e->vel, v, sizeof(e->vel));
	memcpy(e->radius, r, sizeof(e->radius));
	// Having this well-defined in all cases is nice for the visual interpolation we do,
	// and actually there might have been some edge cases causing desync if ents got
	// added in the middle of collision processing??? If that's possible??
	range(i, 3) e->old[i] = c[i] - v[i];

	e->whoMoves = NULL;
	e->tick = NULL;
	e->tickHeld = NULL;
	e->color = -1;
	e->friction = DEFAULT_FRICTION;
	e->crush = NULL;
	e->push = defaultPush;
	e->pushed = NULL;
	e->onFumble = defaultEntPair;
	e->onFumbled = defaultEntPair;
	e->onPickUp = defaultEntPair;
	e->onPickedUp = defaultEntPair;
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
	defaultPush = pushHandlers.get(PUSH_NIL);
	defaultEntPair = entPairHandlers.get(ENTPAIR_NIL);
}
