#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ent.h"
#include "main.h"
#include "handlerRegistrar.h"

#include "entFuncs.h"

static push_t defaultPush;
static entPair_t defaultEntPair;

void prepNewSliders(ent *e) {
	range(i, e->numSliders) {
		e->sliders[i].max = INT32_MIN;
		e->sliders[i].min = INT32_MAX;
	}
}

// blueprint funcs

void clearBlueprintHandlers(ent_blueprint *e) {
	e->whoMoves = NULL;
	e->tick = NULL;
	e->tickHeld = NULL;
	e->crush = NULL;
	e->push = defaultPush;
	e->pushed = NULL;
	e->onFumble = defaultEntPair;
	e->onFumbled = defaultEntPair;
	e->onPickUp = defaultEntPair;
	e->onPickedUp = defaultEntPair;
}

void initEntBlueprint(ent_blueprint *e) {
	*e = {0};
	// "Initializing" a lot of this just means leaving it at 0,
	// and some other stuff (like wires) we don't intend to
	// use a blueprint for so there's no need to initialize it.
	// However, some stuff has a default value that's non-zero.
	e->color = -1;
	e->friction = DEFAULT_FRICTION;
	clearBlueprintHandlers(e);
	// numSliders / sliders can stay 0
}

void destroyEntBlueprint(ent_blueprint *e) {
	free(e->sliders);
	// TODO Maybe move `wires` from `ent_blueprint` into `ent`?
	//      Not sure yet if I want them to actually differ any.
}

void copyEntBlueprint(ent_blueprint *dest, ent_blueprint const *src) {
	*dest = *src;
	size_t sliderSize = dest->numSliders * sizeof(slider);
	dest->sliders = (slider*)malloc(sliderSize);
	memcpy(dest->sliders, src->sliders, sliderSize);
}

// ent init funcs

// Like `initEnt`, but you have to give it the ent, and expects that the blueprint stuff is already done
void initBlueprintedEnt (
	ent *e,
	gamestate *gs,
	ent *relative
) {
	e->newTypeMask = e->typeMask;
	e->newCollideMask = e->collideMask;

	// TODO Kind of dumb to do 3 `malloc`s that will probably never be used over the lifetime of the ent.
	//      It looks like `realloc` and `free` both appropriately treat NULL as a valid pointer to a zero-length allocation,
	//      so maybe we could modify `list` to start with `max = 0` and see if anything is broken by that assumption?
	//      I know there's some stuff in the brains codebase that wouldn't like that...
	e->wires.init();
	e->wiresRm.init();
	e->wiresAdd.init();
	prepNewSliders(e);

	// Having this well-defined in all cases is nice for the visual interpolation we do,
	// and actually there might have been some edge cases causing desync if ents got
	// added in the middle of collision processing??? If that's possible??
	range(i, 3) e->old[i] = e->center[i] - e->vel[i];

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
	ent *e = (ent*) malloc(sizeof(ent));
	initEntBlueprint(e);

	e->typeMask = typeMask;
	e->collideMask = collideMask;

	e->numSliders = numSliders;
	e->sliders = (slider*) calloc(numSliders, sizeof(slider));
	memcpy(e->center, c, sizeof(e->center));
	memcpy(e->vel, v, sizeof(e->vel));
	memcpy(e->radius, r, sizeof(e->radius));

	initBlueprintedEnt(e, gs, relative);
	return e;
}

void init_entFuncs() {
	defaultPush = pushHandlers.get(PUSH_NIL);
	defaultEntPair = entPairHandlers.get(ENTPAIR_NIL);
}
