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

static char getLists(ent *e) {
	a.num = b.num = 0;
	holdeesAnyOrder(h, e) {
		if ((h->typeMask & T_DECOR) && h->wires.num == 1 && h->numSliders == 1) {
			list<ent*> &dest = getSlider(h, 0) ? b : a;
			dest.add(h->wires[0]);
		}
	}

	char ret = 0;
	if (!a.num) {
		ret += 1;
		a.add(e);
	}
	if (!b.num) {
		ret += 2;
		b.add(e);
	}
	return ret;
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

static void getExtents(list<ent*> *L, int axis, int32_t *min_out, int32_t *max_out) {
	list<ent*> &l = *L;
	ent *e = l[0];
	int32_t min = e->center[axis] - e->radius[axis];
	int32_t max = e->center[axis] + e->radius[axis];
	for (int i = 1; i < l.num; i++) {
		int32_t x = l[i]->center[axis];
		int32_t r = l[i]->radius[axis];
		if (x+r - max > 0) max = x+r;
		if (x-r - min < 0) min = x-r;
	}
	*min_out = min;
	*max_out = max;
}

// Finds the median of the items in `a`, but it may be off by 0.5
static void getMedian(int32_t *dest) {
	range(i, 3) {
		int32_t min, max;
		getExtents(&a, i, &min, &max);
		// Wacky average formula, works better when position is extreme
		dest[i] = min + (max - min)/2;
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
	char defaulted = getLists(e);

	// Counts of selected ents should include when we're the implicit selection
	int aNum = a.num - (defaulted & 1);
	int bNum = b.num - (defaulted & 2)/2;

	e = a[0];
	printf(
		"p (%d, %d, %d)\ns (%d, %d, %d)\nc %6X\nSelected %d, %d\n",
		e->center[0], e->center[1], e->center[2],
		e->radius[0], e->radius[1], e->radius[2],
		e->color,
		aNum, bNum
	);
}

int32_t edit_color(ent *e, const char *colorStr, char priviledged) {
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

	// if not priviledged, you can only change yourself
	if (!priviledged) {
		a.num = 0;
		a.add(e);
	}

	range(i, a.num) {
		a[i]->color = color;
	}

	return (a.num == 1 && a[0] == e) ? color : -2;
}

void edit_wireNearby(gamestate *gs, ent *me) {
	if (!me) return;
	for (ent *e = gs->ents; e; e = e->ll.n) {
		if (e->holdRoot == me) continue;
		range(i, 3) {
			int32_t r = e->radius[i] + me->radius[i];
			if (abs(e->center[i] - me->center[i]) > r) goto next;
		}
		uWire(me, e);
		next:;
	}
}

void edit_wireInside(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	int32_t min[3], max[3];
	range(i, 3) getExtents(&b, i, min+i, max+i);
	for (ent *e = gs->ents; e; e = e->ll.n) {
		range(i, 3) {
			if (
				e->center[i] + e->radius[i] - min[i] <= 0 ||
				e->center[i] - e->radius[i] - max[i] >= 0
			) {
				goto skip;
			}
		}
		uWire(me, e);
		skip:;
	}
}

void edit_rm(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		uDead(gs, a[i]);
	}
}

static int32_t adjustRadius(int32_t input, char verbose) {
	if (input <= 0 || input > 10000000) {
		int32_t n;
		if (input <= 0) n = 1;
		else n = 10000000;
		if (verbose) fprintf(stderr, "Input radius %d was replaced with %d\n", input, n);
		return n;
	}
	return input;
}

void edit_create(gamestate *gs, ent *me, const char *argsStr, char verbose) {
	if (!me) return;
	parseArgs(argsStr);
	if (args.num != 3) {
		if (verbose) fprintf(stderr, "Requires exactly 3 numerical args, but received: %s\n", argsStr);
		return;
	}

	int32_t pos[3], size[3];
	range(i, 3) {
		pos[i] = me->center[i];
		size[i] = adjustRadius(args[i], verbose);
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

void edit_stretch(gamestate *gs, ent *me, const char *argsStr, char verbose) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	if (args.num >= 3) {
		int32_t radius[3];
		range(i, 3) {
			radius[i] = adjustRadius(args[i], verbose);
		}
		range(i, a.num) {
			ent *e = a[i];
			box *b = e->myBox;
			memcpy(e->radius, radius, sizeof(int32_t)*3);
			assignVelbox(e, b);
			velbox_remove(b);
		}
	} else if (args.num >= 1) {
		int axis, dir;
		getAxis(me, &axis, &dir);
		int32_t amt = args[0];
		int32_t offset = amt * -dir;
		range(i, a.num) {
			ent *e = a[i];
			box *b = e->myBox;
			e->center[axis] += offset;
			e->radius[axis] = adjustRadius(e->radius[axis] + amt, verbose);
			assignVelbox(e, b);
			velbox_remove(b);
		}
	}
}

void edit_copy(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	int axis, dir;
	getAxis(me, &axis, &dir);
	int32_t min, max;
	getExtents(&b, axis, &min, &max);
	int32_t width = (max-min) * dir;
	range(i, b.num) {
		ent *e = b[i];
		// Push the existing ent around, just for a sec.
		// We undo this later, but it spares us having
		// to copy yet another vector
		e->center[axis] -= width;

		ent *created = initEnt(
			gs, e,
			e->center, zeroVec, e->radius,
			0,
			T_TERRAIN + T_HEAVY + T_WEIGHTLESS, 0
		);
		created->color = e->color;
		uWire(me, created);

		e->center[axis] += width;
	}
}

void edit_rotate(gamestate *gs, ent *me, char verbose) {
	if (!me) return;
	getLists(me);
	int axis, dir;
	getAxis(me, &axis, &dir);

	axis = (axis + dir + 3) % 3;
	int32_t min1, max1;
	getExtents(&a, axis, &min1, &max1);
	int32_t d1 = max1 - min1;
	int32_t c1 = min1 + d1/2;

	int axis2 = (axis + dir + 3) % 3;
	int32_t min2, max2;
	getExtents(&a, axis2, &min2, &max2);
	int32_t d2 = max2 - min2;
	int32_t c2 = min2 + d2/2;

	char shift = d1%2;
	if (shift != d2%2 && verbose) {
		puts("Warning! Rotation between axes with even/odd widths necessarily introduces drift, make sure your items are aligned how you want");
	}

	range(i, a.num) {
		ent *e = a[i];
		// No need to redo velbox data, since it only cares about what the box's greatest width is.
		int32_t tmp = e->radius[axis];
		e->radius[axis] = e->radius[axis2];
		e->radius[axis2] = tmp;

		tmp = e->center[axis2] - c2;
		e->center[axis2] = c2 + e->center[axis] - c1;
		e->center[axis] = c1 - tmp + shift;
	}
}

void edit_flip(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	int axis, dir;
	getAxis(me, &axis, &dir);
	int32_t min, max;
	getExtents(&a, axis, &min, &max);
	int32_t x = min + max; // Too tired to explain this eloquently, but yeah it works
	range(i, a.num) {
		ent *e = a[i];
		e->center[axis] = x - e->center[axis];
	}
}

void edit_highlight(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) a[i]->color ^= 0x808080;
}

void edit_measure(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	int axis, dir;
	getAxis(me, &axis, &dir);
	int32_t min, max;
	getExtents(&a, axis, &min, &max);
	int32_t d = max-min;

	printf("Outer: %d", d);
	if (d%2 == 0) {
		printf(" (2*%d)", d/2);
	}
	putchar('\n');

	if (a.num == 2) {
		d -= 2*(a[0]->radius[axis] + a[1]->radius[axis]);
		printf("Inner: %d", d);
		if (d%2 == 0) {
			printf(" (2*%d)", d/2);
		}
		putchar('\n');
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
