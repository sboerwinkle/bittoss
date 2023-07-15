#include <stdio.h>
#include <stdlib.h>
#include "ent.h"
#include "main.h"

void pushBtn(ent *who, int ix) {who->ctrl.btns[ix].v2 = 1;}

void pushAxis1(ent *who, int *x) {
	int i;
	for (i = 0; i < 2; i++) {
		if (x[i] > who->ctrl.axis1.max[i]) {
			who->ctrl.axis1.max[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] > axisMaxis) {
				printf("Invalid axis component %d on dimension %c\n", x[i], "XY"[i]);
			}
#endif
		}
		if (x[i] < who->ctrl.axis1.min[i]) {
			who->ctrl.axis1.min[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] < -axisMaxis) {
				printf("Invalid axis component %d on dimension %c\n", x[i], "XY"[i]);
			}
#endif
		}
	}
}
void pushEyes(ent *who, int *x) {
	int i;
	for (i = 0; i < 3; i++) {
		if (x[i] > who->ctrl.look.max[i]) {
			who->ctrl.look.max[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] > axisMaxis) {
				printf("Invalid look component %d on dimension %d\n", x[i], i);
			}
#endif
		}
		if (x[i] < who->ctrl.look.min[i]) {
			who->ctrl.look.min[i] = x[i];
#ifndef NODEBUG_SCRIPT
			if (x[i] < -axisMaxis) {
				printf("Invalid axis component %d on dimension %d\n", x[i], i);
			}
#endif
		}
	}
}

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
	e->dead_max[gs->flipFlop_death] = 1;
}

void uDrop(gamestate *gs, ent *e) {
	e->drop_max[gs->flipFlop_drop] = 1;
}

void uPickup(gamestate *gs, ent *p, ent *e) {
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
	p->holdRoot->pickup_max[gs->flipFlop_pickup] = 1;
	if (e->holder_max[gs->flipFlop_pickup]) {
		if (e->holder_max[gs->flipFlop_pickup] != p) e->holder_max[gs->flipFlop_pickup] = e;
		return;
	}
	e->holder_max[gs->flipFlop_pickup] = p;
}

// TODO need functions for wiring / de-wiring ents

void uStateSlider(entState *s, int ix, int32_t value) {
	if (ix < 0 || ix >= s->numSliders) {
#ifndef NODEBUG_SCRIPT
		printf("Invalid slider #%d (limit is %d)\n", ix, s->numSliders);
#endif
		return;
	}
	if (value > s->sliders[ix].max) s->sliders[ix].max = value;
	if (value < s->sliders[ix].min) s->sliders[ix].min = value;
}

void uWire(ent *e, ent *w) {
	e->wiresAdd.add(w);
}

void uUnwire(ent *e, ent *w) {
	e->wiresRm.add(w);
}

void uTypeMask(ent *e, uint32_t mask, char turnOn) {
	if (turnOn) {
		e->d_typeMask_max |= mask;
	} else {
		e->d_typeMask_min |= mask;
	}
}

void uCollideMask(ent *e, uint32_t mask, char turnOn) {
	if (turnOn) {
		e->d_collideMask_max |= mask;
	} else {
		e->d_collideMask_min |= mask;
	}
}

//TODO: Requests to update orientation
