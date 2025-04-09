#include <stdio.h>
#include <stdlib.h>
#include "ent.h"
#include "main.h"

void pushBtn(ent *who, int ix) {who->ctrl.btns[ix].v2 = 1;}


void uCenter(ent *e, int32_t *d) {
	range(i, 3) {
		e->d_center[i] += d[i];
	}
}

void uVel(ent *e, int32_t *a) {
	for (int i = 0; i < 3; i++) {
		e->d_vel[i] += a[i];
	}
}

void uDead(gamestate *gs, ent *e) {
	e->dead_max[gs->flipFlop_death] |= 1;
}

void uErase(gamestate *gs, ent *e) {
	e->dead_max[gs->flipFlop_death] = 3;
}

void uDrop(gamestate *gs, ent *e) {
	e->newDrop = 1;
}

void uPickup(gamestate *gs, ent *p, ent *e, int32_t holdFlags) {
	if (e->holder) {
#ifndef NODEBUG_SCRIPT
		// Previously this would log something, but it's probably not a script bug to try to pick something up
		// without knowing for sure if it's root
		//puts("Can't pickup a non-root ent!");
#endif
		//And we're not going to write a ref to values that aren't checked for a while, that's just a bad plan.
		return;
	}
	//Got to be careful
	if (e->dead || p->dead) {
#ifndef NODEBUG_SCRIPT
		puts("No pickups invovling dead things! But you might not have known.");
#endif
		return;
	}
	if (e->newHolder) {
		if (e->newHolder != p || e->newHoldFlags != holdFlags) e->newHolder = e;
		return;
	}
	e->newHolder = p;
	e->newHoldFlags = holdFlags;
}

void uSlider(ent *e, int ix, int32_t value) {
	if (ix < 0 || ix >= e->numSliders) {
#ifndef NODEBUG_SCRIPT
		printf("Invalid slider #%d (limit is %d)\n", ix, e->numSliders);
#endif
		return;
	}
	e->fullFlush = 1;
	if (value > e->sliders[ix].max) e->sliders[ix].max = value;
	if (value < e->sliders[ix].min) e->sliders[ix].min = value;
}

void uWire(ent *e, ent *w) {
	e->fullFlush = 1;
	e->wiresAdd.add(w);
}

void uUnwire(ent *e, ent *w) {
	e->wiresRm.add(w);
}

void uMyTypeMask(ent *e, uint32_t mask) {
	e->fullFlush = 1;
	e->newTypeMask = mask;
}

void uMyCollideMask(ent *e, uint32_t mask) {
	if ((mask | T_COLLIDABLE) != T_COLLIDABLE) {
		fprintf(stderr, "collideMask %d surpasses T_COLLIDABLE, this needs to be fixed\n", mask);
	}
	e->fullFlush = 1;
	e->newCollideMask = mask;
}

//TODO: Requests to update orientation


// Hoo boy.
// This little number is meant to update ents to match after a pickup,
// and is supposed to work even in the middle of a round of collision
// resolution. If I also want it to work outside of collision resolution
// I'd probably have to include dx and dv (or whatever they're really called).
// As-is it'll be susceptible to other pending updates not getting cleared,
// assuming you consider that to be a bad thing.
void updateTrajectory(ent *who, int32_t *old, int32_t *center, int32_t *vel) {
	who->needsPhysUpdate = 1;
	int i;
	for (i = 0; i < 3; i++) {
		who->old[i] += old[i];
		who->center[i] += center[i];
		who->vel[i] += vel[i];
	}
	ent *e;
	for (e = who->holdee; e; e = e->LL.n) {
		updateTrajectory(e, old, center, vel);
	}
}
