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
		if ((h->typeMask & T_DECOR) && h->wires.num == 1 && h->state.numSliders == 1) {
			list<ent*> &dest = getSlider(&h->state, 0) ? b : a;
			dest.add(h->wires[0]);
		}
	}

	if (!a.num) {
		a.add(e);
		return 1;
	} else {
		return 0;
	}
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

void edit_info(ent *e) {
	if (!e) return;
	getLists(e);
	e = a[0];
	printf(
		"p (%d, %d, %d)\ns (%d, %d, %d)\nc %6X\nSelected %d, %d\n",
		e->center[0], e->center[1], e->center[2],
		e->radius[0], e->radius[1], e->radius[2],
		e->color,
		a.num, b.num
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
	char shouldReturn = getLists(e);
	range(i, a.num) {
		a[i]->color = color;
	}

	return shouldReturn ? color : -2;
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
