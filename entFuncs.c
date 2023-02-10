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

//default for onFree
void doNothing(ent *me) {}

void onPushDefault(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {}

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
	ret->onFree = doNothing;
	ret->push = onPushDefault;
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
