#include <stdlib.h>
#include <stdio.h>
#include <regex.h>

#include "util.h"
#include "list.h"
#include "ent.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "entFuncs.h"
#include "colors.h"

static list<ent*> a, b;
static list<int32_t> args;
static regex_t colorRegex;
static regmatch_t regexMatches[3];

static void getLists(ent *e) {
	a.num = b.num = 0;
	holdeesAnyOrder(h, e) {
		if ((h->typeMask & T_DECOR) && h->wires.num == 1 && h->state.numSliders == 1) {
			list<ent*> &dest = getSlider(&h->state, 0) ? b : a;
			dest.add(h->wires[0]);
		}
	}

	if (!a.num) a.add(e);
	if (!b.num) b.add(e);
}

static void parseArgs(const char *a) {
	args.num = 0;
	char *b;
	while (1) {
		int32_t arg = strtol(a, &b, 0);
		if (b == a) return;
		a = b;
		args.add(arg);
	}
}

// Finds the median of the centerpoints (ignoring radius!) of the items in `a`.
// Ignoring the radius is a little unintuitive, but this isn't commonly used anyway so I'm saying it's fine.
static void getMedian(int32_t *dest) {
	int32_t min[3], max[3];
	range(i, 3) {
		min[i] = INT32_MAX;
		max[i] = INT32_MIN;
	}
	range(i, a.num) {
		ent *e = a[i];
		range(j, 3) {
			int32_t x = e->center[j];
			if (x > max[j]) max[j] = x;
			if (x < min[j]) min[j] = x;
		}
	}
	range(i, 3) {
		// Wacky average formula, works better when position is extreme
		dest[i] = min[i] + (max[i] - min[i])/2;
	}
}

static void getAxis(ent *e, int *axis, int *dir) {
	range(i, 3) {
		// This is a little messy,
		// but we absolutely want to grab the player's controls
		// from *this* frame (or weird stuff will happen if they missed last frame).
		int32_t x = e->ctrl.look.min[i];
		if (!x) x = e->ctrl.look.max[i];
		if (abs(x) == axisMaxis) {
			*axis = i;
			*dir = x / axisMaxis;
			return;
		}
	}
	// Fallback, ideally unused
	*axis = 2;
	*dir = -1;
}

static int32_t getNextOffset(int axis, int dir) {
	int32_t ret = INT32_MAX;
	range(i, a.num) {
		ent *e = a[i];
		range(j, b.num) {
			ent *o = b[j];
			int32_t offset = (o->center[axis] - e->center[axis]) * dir;
			int32_t r1 = e->radius[axis];
			int32_t r2 = o->radius[axis];
#define f(x) if(x > 0 && x < ret) ret = x
			f(offset);
			f(offset-r1-r2);
			f(offset-r1+r2);
			f(offset+r1-r2);
			f(offset+r1+r2);
#undef f
		}
	}
	return ret == INT32_MAX ? 0 : ret;
}

void edit_info(ent *e) {
	if (!e) return;
	getLists(e);

	// Counts of selected ents should include when we're the implicit selection
	int aNum = a.num;
	if (aNum == 1 && a[0] == e) aNum = 0;
	int bNum = b.num;
	if (bNum == 1 && b[0] == e) bNum = 0;

	e = a[0];
	printf(
		"p (%d, %d, %d)\ns (%d, %d, %d)\nc %6X\nSelected %d, %d\n",
		e->center[0], e->center[1], e->center[2],
		e->radius[0], e->radius[1], e->radius[2],
		e->color,
		aNum, bNum
	);
}

int32_t edit_color(ent *e, const char *colorStr) {
	int32_t color;
	// First, check for 6-digit hex color representation
	if (!regexec(&colorRegex, colorStr, 3, regexMatches, 0)) {
		color = strtol(colorStr + regexMatches[2].rm_so, NULL, 16);
	} else {
		// Next, check named CSS colors.
		// Originally an easter egg, it _is_ awful convenient.
		// This also contains Crayola "timberwolf" and Pantone "cactus", along with special color "clear".
		color = findColor(colorStr);
		if (color == -2) {

			// Finally, fall back to the old, weird, 6-bit color representation
			color = (colorStr[0] - '0') + 4 * (colorStr[1] - '0') + 16 * (colorStr[2] - '0');
			// Convert from weird 6-bit repr (with all its quirks) to regular 32-bit color
			color = (color&3)*255/3*0x10000 + (color&12)/4*255/3*0x100 + (color&48)/16*255/3;
		}
	}

	if (!e) return color;

	// Return value is used by the caller to set the persistent color for the active character.
	// We only want that behavior if they don't have anything selected.
	getLists(e);
	range(i, a.num) {
		a[i]->color = color;
	}

	return (a.num == 1 && a[0] == e) ? color : -2;
}

void edit_wireNearby(gamestate *gs, ent *me) {
	if (!me) return;
	for (ent *e = gs->rootEnts; e; e = e->LL.n) {
		if (e->holdRoot == me) continue;
		range(i, 3) {
			int32_t r = e->radius[i] + me->radius[i];
			if (abs(e->center[i] - me->center[i]) > r) goto next;
		}
		uWire(me, e);
		next:;
	}
}

void edit_rm(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		uDead(gs, a[i]);
	}
}

void edit_create(gamestate *gs, ent *me, const char *argsStr) {
	if (!me) return;
	parseArgs(argsStr);
	if (args.num != 3) {
		fprintf(stderr, "Requires exactly 3 numerical args, but received: %s\n", argsStr);
		return;
	}

	int32_t pos[3], size[3];
	range(i, 3) {
		pos[i] = me->center[i];
		int32_t w = args[i];
		if (w <= 0 || w > 10000000) {
			int32_t n;
			if (w <= 0) n = 1;
			else n = 10000000;
			fprintf(stderr, "Input radius %d was replaced with %d\n", w, n);
			w = n;
		}
		size[i] = w;
	}
	// Place it directly over our head, for starters
	pos[2] -= me->radius[2] + size[2];

	ent *created = initEnt(
		gs, me,
		pos, zeroVec, size,
		0,
		T_TERRAIN + T_HEAVY + T_WEIGHTLESS, 0
	);
	created->color = CLR_WHITE;
	uWire(me, created);
}

void edit_push(gamestate *gs, ent *me, const char *argsStr) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	int32_t offset[3];
	if (args.num >= 3) {
		int32_t center[3];
		getMedian(center);
		range(i, 3) {
			offset[i] = args[i] - center[i];
		}
	} else {
		int axis, dir;
		int32_t amt;
		getAxis(me, &axis, &dir);
		if (args.num >= 1) {
			amt = args[0];
		} else {
			amt = getNextOffset(axis, dir);
		}
		range(i, 3) offset[i] = 0;
		offset[axis] = dir * amt;
	}
	range(i, a.num) {
		uCenter(a[i], offset);
	}
}

void edit_init() {
	a.init();
	b.init();
	args.init();
	regcomp(&colorRegex, "^ *(#|0x)?([0-9a-f]{6}) *$", REG_EXTENDED|REG_ICASE);
}

void edit_destroy() {
	a.destroy();
	b.destroy();
	args.destroy();
	regfree(&colorRegex);
}
