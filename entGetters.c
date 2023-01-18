#include <stdio.h>

#include "ent.h"
#include "main.h"
#include "util.h"

int bound(int x, int b) {
	if (x > b) return b;
	if (x < -b) return -b;
	return x;
}

int type(ent *e) {
	return e->typeMask;
}

void getSize(int32_t *dest, ent *e) {
	range(i, 3) {
		dest[i] = e->radius[i];
	}
}

char getButton(ent *e, int i) {
	return e->ctrl.btns[i].v;
}

char getTrigger(ent *e, int i) {
	return e->ctrl.btns[i+2].v;
}

void getAxis(int32_t *dest, ent *e) {
	dest[0] = e->ctrl.axis1.v[0];
	dest[1] = e->ctrl.axis1.v[1];
}

void getLook(int32_t *dest, ent *e) {
	range(i, 3) {
		dest[i] = e->ctrl.look.v[i];
	}
}

void getPos(int *dest, ent *a, ent *b) {
	range(i, 3) {
		dest[i] = b->center[i] - a->center[i];
	}
}

void getVel(int *dest, ent *a, ent *b) {
	range(i, 3) {
		dest[i] = b->vel[i] - a->vel[i];
	}
}

int getSlider(entState *s, int ix) {
	if (ix < 0 || ix >= s->numSliders) {
		fprintf(stderr, "Can't access slider %d (%d total)\n", ix, s->numSliders);
		return 0;
	}
	return s->sliders[ix].v;
}
