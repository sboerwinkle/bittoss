#include "ent.h"
#include "ts_macros.h"
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

char getButton(ent *e, int i) {
	return e->ctrl.btns[i].v;
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
