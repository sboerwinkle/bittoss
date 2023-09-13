#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <strings.h>

#include "util.h"
#include "list.h"
#include "ent.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "entFuncs.h"
#include "colors.h"
#include "handlerRegistrar.h"
#include "file.h"
#include "serialize.h"

#include "modules/player.h"
#include "modules/factory.h"

static list<ent*> a, b;
static list<int32_t> args;
static regex_t colorRegex;
static regmatch_t regexMatches[3];

static void fixNewBaubles(gamestate *gs) {
	// Pretty sloppy, but whatever, edit functionality can afford to be an exception.
	// The problem is that we're creating the baubles outside of the `tick` loop,
	// which means they'll actually experience a tick before their pickup processes.
	// (things which are added during the `tick` loop go at the beginning of the list,
	//  and so miss their `tick` for that turn)
	// Baubles don't like to be unheld, so they'll self-destruct unless we pick them up
	// before they tick.
	flushPickups(gs);
}

// TODO We can get away with a lot less if we're just e.g. turning the ent,
//      since we know it won't change position in the heirarchy.
static void radiusFix(ent *e) {
	box *b = e->myBox;
	if (!b) return;
	assignVelbox(e, b);
	velbox_remove(b);
}

static void setNumSliders(gamestate *gs, ent *e, int sliders) {
	e->sliders = (slider*) realloc(e->sliders, sliders * sizeof(slider));
	int oldSliders = e->numSliders;
	e->numSliders = sliders;
	if (sliders > oldSliders) {
		// Reset all the sliders we added
		bzero(e->sliders + oldSliders, sizeof(slider) * (sliders - oldSliders));
		// This actually resets min/max on all sliders, may need to work on the name
		prepNewSliders(e);
	}
}

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
	int32_t x;
	while (getNum(&a, &x)) args.add(x);
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
	int32_t look[3];
	getLook(look, e);
	range(i, 3) {
		// This is a little messy,
		// but we absolutely want to grab the player's controls
		// from *this* frame (or weird stuff will happen if they missed last frame).
		int32_t x = look[i];
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
		"p (%d, %d, %d)\ns (%d, %d, %d)\nc %06X\n",
		e->center[0], e->center[1], e->center[2],
		e->radius[0], e->radius[1], e->radius[2],
		e->color
	);
	if (e->numSliders) {
		fputs("Sliders: ", stdout);
		range(i, e->numSliders) {
			printf("%d ", e->sliders[i].v);
		}
		putchar('\n');
	}
	printf("Selected %d, %d\n", aNum, bNum);
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

static void _selectInside(gamestate *gs, ent *me, char bidi, int32_t offset) {
	if (!me) return;
	getLists(me);
	int32_t min[3], max[3];
	range(i, 3) getExtents(&b, i, min+i, max+i);
	for (ent *e = gs->ents; e; e = e->ll.n) {
		range(i, 3) {
			if (
				e->center[i] + e->radius[i] - min[i] < -offset ||
				e->center[i] - e->radius[i] - max[i] > offset
			) {
				goto skip;
			}
			if (
				bidi && (
					e->center[i] - e->radius[i] - min[i] < -offset ||
					e->center[i] + e->radius[i] - max[i] > offset
				)
			) {
				goto skip;
			}
		}
		player_toggleBauble(gs, me, e, 0);
		skip:;
	}
	fixNewBaubles(gs);
}

void edit_selectInside(gamestate *gs, ent *me, const char *argsStr) {
	parseArgs(argsStr);
	int mode = 1;
	if (args.num && args[0] >= 0 && args[0] < 4) {
		mode = args[0];
	}
	/*
	 * 0 - touching -        bidi false, offset 0
	 * 1 - intersecting -    bidi false, offset -1
	 * 2 - covered -         bidi true,  offset 0
	 * 3 - surrounded -      bidi true,  offset -1
	 */
	_selectInside(gs, me, mode >= 2, -(mode%2));
}

void edit_selectWires(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	list<ent*> wires;
	wires.init();
	range(i, b.num) {
		ent *e = b[i];
		wiresAnyOrder(w, e) {
			// We can't do anything more clever than this,
			// because we can't risk causing a desync
			// (so no sorting by memory address, e.g.)
			if (!wires.has(w)) wires.add(w);
		}
	}
	range(i, wires.num) {
		player_toggleBauble(gs, me, wires[i], 0);
	}
	fixNewBaubles(gs);
	wires.destroy();
}

void edit_selectHeld(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, b.num) {
		for (ent *x = b[i]->holdee; x; x = x->LL.n) {
			player_toggleBauble(gs, me, x, 0);
		}
	}
	fixNewBaubles(gs);
}

static void selectRecursive(gamestate *gs, ent *me, ent *e) {
	player_toggleBauble(gs, me, e, 0);
	for (ent *x = e->holdee; x; x = x->LL.n) selectRecursive(gs, me, x);
}

void edit_selectHeldRecursive(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, b.num) {
		ent *e = b[i];
		// Since we're going recursive, need to skip items with a selected ancestor.
		// we probably don't want to explicitly also move this ent.
		for (ent *x = e->holder; x; x = x->holder) {
			if (b.has(x)) goto next;
		}
		selectRecursive(gs, me, e);
		next:;
	}
	fixNewBaubles(gs);
}

void edit_rm(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		uDead(gs, a[i]);
	}
}

static void typeFlag(gamestate *gs, ent *me, const char* argsStr, int32_t flag) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	int mode = 2;
	// Weird coincidence, both "weight" and "fpdraw" are expressed via the
	// absence of a flag.
	if (args.num) mode = !args[0];
	range(i, a.num) {
		ent *e = a[i];
		int32_t t = e->typeMask;
		if (mode == 0) t &= ~flag;
		else if (mode == 1) t |= flag;
		else t ^= flag;
		uMyTypeMask(e, t);
	}
}

void edit_m_weight(gamestate *gs, ent *me, const char* argsStr) {
	typeFlag(gs, me, argsStr, T_WEIGHTLESS);
}

void edit_m_fpdraw(gamestate *gs, ent *me, const char* argsStr) {
	typeFlag(gs, me, argsStr, T_NO_DRAW_FP);
}

void edit_m_friction(gamestate *gs, ent *me, const char* argsStr) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	int32_t f = args.num ? args[0] : DEFAULT_FRICTION;
	range(i, a.num) {
		a[i]->friction = f;
	}
}

void edit_m_paper(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		uMyTypeMask(e, T_OBSTACLE + (e->typeMask & T_WEIGHTLESS));
		uMyCollideMask(e, T_OBSTACLE + T_TERRAIN);
	}
}

void edit_m_wood(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_WOOD);
		uMyTypeMask(e, T_OBSTACLE + T_HEAVY + (e->typeMask & T_WEIGHTLESS));
		uMyCollideMask(e, T_TERRAIN + T_OBSTACLE);
	}
}

void edit_m_stone(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		uMyTypeMask(e, T_TERRAIN + (e->typeMask & T_WEIGHTLESS));
		uMyCollideMask(e, T_TERRAIN);
	}
}

void edit_m_metal(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_METAL);
		uMyTypeMask(e, T_TERRAIN + T_HEAVY + (e->typeMask & T_WEIGHTLESS));
		uMyCollideMask(e, T_TERRAIN);
	}
}

void edit_m_wall(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		// `whoMoves` actually shouldn't be called, since it has no collideMask.
		// "real" walls actually leave it NULL, but I think this is better.
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		uMyTypeMask(e, T_TERRAIN + T_HEAVY + T_WEIGHTLESS);
		uMyCollideMask(e, 0);
	}
}

void edit_m_ghost(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		uMyTypeMask(e, T_WEIGHTLESS);
		uMyCollideMask(e, 0);
	}
}

void edit_t_dumb(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_NIL);
		e->tickHeld = tickHandlers.get(TICK_NIL);
		e->push = pushHandlers.get(PUSH_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 0);
	}
}

void edit_t_cursed(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_CURSED);
	}
}

void edit_t_logic(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_LOGIC);
		e->tickHeld = tickHandlers.get(TICK_LOGIC);
		e->push = pushHandlers.get(PUSH_LOGIC);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 2);
	}
}

void edit_t_logic_debug(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_LOGIC_DEBUG);
		e->tickHeld = tickHandlers.get(TICK_LOGIC_DEBUG);
		e->push = pushHandlers.get(PUSH_LOGIC);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 2);
	}
}

void edit_t_door(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_DOOR);
		e->tickHeld = tickHandlers.get(TICK_DOOR);
		e->push = pushHandlers.get(PUSH_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 2);
	}
}

void edit_t_legg(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_NIL); //Leggs don't do anything if not held
		e->tickHeld = tickHandlers.get(TICK_LEGG);
		e->push = pushHandlers.get(PUSH_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 8);
	}
}

void edit_t_respawn(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_RESPAWN);
		e->tickHeld = tickHandlers.get(TICK_RESPAWN);
		e->push = pushHandlers.get(PUSH_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 5);
	}
}

void edit_t_seat(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		e->tick = tickHandlers.get(TICK_SEAT);
		e->tickHeld = tickHandlers.get(TICK_SEAT);
		e->push = pushHandlers.get(PUSH_SEAT);
		e->onPickUp = entPairHandlers.get(PICKUP_SEAT);
		e->onFumble = entPairHandlers.get(FUMBLE_SEAT);
		setNumSliders(gs, e, 2);
	}
}

void edit_m_t_veh_eye(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	range(i, a.num) {
		ent *e = a[i];
		// Eyes are sort of a weird situation,
		// we set both their material (_m_) and their behavior (_t_)
		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		uMyTypeMask(e, T_DECOR);
		uMyCollideMask(e, 0);

		e->tick = tickHandlers.get(TICK_EYE);
		e->tickHeld = tickHandlers.get(TICK_HELD_VEH_EYE);
		e->push = pushHandlers.get(PUSH_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		setNumSliders(gs, e, 0);
	}
}

void edit_slider(gamestate *gs, ent *me, const char *argsStr, char verbose) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	if (args.num != 2) {
		if (verbose) puts("Exactly 2 args are expected to /slider");
		return;
	}
	range(i, a.num) {
		uSlider(a[i], args[0], args[1]);
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
		T_WEIGHTLESS, 0
	);
	created->color = CLR_WHITE;
	player_toggleBauble(gs, me, created, 0);
	fixNewBaubles(gs);
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
		// Since offsets are passed onto children,
		// if any of our ancestors is also selected
		// we probably don't want to explicitly also move this ent.
		for (ent *x = a[i]->holder; x; x = x->holder) {
			if (a.has(x)) goto next;
		}

		uCenter(a[i], offset);
		next:;
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
			memcpy(e->radius, radius, sizeof(int32_t)*3);

			radiusFix(e);
		}
	} else if (args.num >= 1) {
		int axis, dir;
		getAxis(me, &axis, &dir);
		int32_t amt = args[0];
		int32_t offset = amt * -dir;
		range(i, a.num) {
			ent *e = a[i];
			e->center[axis] += offset;
			e->radius[axis] = adjustRadius(e->radius[axis] + amt, verbose);

			radiusFix(e);
		}
	}
}

static void _scale(gamestate *gs, ent *me, const char *argsStr, char verbose, char force) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	if (args.num != 2) {
		if (verbose) puts("/scale requires exactly 2 args");
		return;
	}
	int32_t numer = args[0];
	int32_t denom = args[1];

	int32_t oldCenter[3];
	memcpy(oldCenter, a[0]->center, sizeof(oldCenter));
	int32_t newCenter[3];
	getMedian(newCenter);
	range(d, 3) newCenter[d] = newCenter[d] + (oldCenter[d] - newCenter[d]) * numer / denom;

	if (!force) {
		range(i, a.num) {
			ent *e = a[i];
			range(d, 3) {
				if (e->radius[d] % denom || (e->center[d] - oldCenter[d]) % denom) {
					if (verbose) puts("Refusing to /scale, divisor doesn't fit evently. You may /scale! if desired.");
					return;
				}
			}
		}
	}

	range(i, a.num) {
		ent *e = a[i];
		range(d, 3) {
			e->radius[d] = e->radius[d] * numer / denom;
			e->center[d] = newCenter[d] + (e->center[d] - oldCenter[d]) * numer / denom;
		}
		radiusFix(e);
	}
}

void edit_scale(gamestate *gs, ent *me, const char *argsStr, char verbose) {
	_scale(gs, me, argsStr, verbose, 0);
}

void edit_scale_force(gamestate *gs, ent *me, const char *argsStr, char verbose) {
	_scale(gs, me, argsStr, verbose, 1);
}

void edit_copy(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);
	int axis, dir;
	getAxis(me, &axis, &dir);
	int32_t min, max;
	getExtents(&a, axis, &min, &max);
	int32_t center[3] = {0, 0, 0};
	center[axis] = (max-min) * dir;
	player_clearBaubles(gs, me, 1);
	player_flipBaubles(me);

	for (ent *e = gs->ents; e; e = e->ll.n) {
		e->clone.ix = -1;
	}
	range(i, a.num) a[i]->clone.ix = 0; // This just marks it for serialization
	ent *start = gs->ents;

	list<char> data;
	data.init();
	serializeSelected(gs, &data, center, zeroVec);
	deserializeSelected(gs, &data, zeroVec, zeroVec);
	data.destroy();

	for (ent *e = gs->ents; e != start; e = e->ll.n) {
		player_toggleBauble(gs, me, e, 0);
	}
	fixNewBaubles(gs);
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
		int32_t tmp = e->radius[axis];
		e->radius[axis] = e->radius[axis2];
		e->radius[axis2] = tmp;

		tmp = e->center[axis2] - c2;
		e->center[axis2] = c2 + e->center[axis] - c1;
		e->center[axis] = c1 - tmp + shift;

		radiusFix(e);
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

void edit_pickup(gamestate *gs, ent *me, const char* argsStr) {
	if (!me) return;
	getLists(me);
	parseArgs(argsStr);
	int32_t holdFlags = args.num ? args[0] : 0;

	// TODO This should log a warning if (b.num != 1), and if a provided "verbose" flag is on (isMe && isReal)
	ent *holder = b[0];
	range(i, a.num) {
		ent *e = a[i];
		if (e->holder == holder) e->holdFlags = holdFlags;
		else uPickup(gs, holder, e, holdFlags);
	}
}

void edit_drop(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);

	range(i, a.num) {
		uDrop(gs, a[i]);
	}
}

void edit_wire(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);

	range(j, b.num) {
		ent *e2 = b[j];
		range(i, a.num) {
			ent *e = a[i];
			uWire(e2, e);
		}
	}
}

void edit_unwire(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);

	range(j, b.num) {
		ent *e2 = b[j];
		range(i, a.num) {
			ent *e = a[i];
			uUnwire(e2, e);
		}
	}
}

void edit_factory(gamestate *gs, ent *me) {
	if (!me) return;
	getLists(me);

	range(i, a.num) {
		ent *e = a[i];
		// Move all extant sliders to the end
		int oldSliders = e->numSliders;
		setNumSliders(gs, e, oldSliders + FACT_SLIDERS);
		for (int j = oldSliders - 1; j >= 0; j--) {
			uSlider(e, j + FACT_SLIDERS, getSlider(e, j));
		}

		uSlider(e, FACT_VERSION, 0);

		int32_t vel[3];
		ent *h = e->holder;
		if (h) {
			getVel(vel, h, e);
			range(j, 3) e->vel[j] = h->vel[j];
		} else {
			range(j, 3) vel[j] = 0;
		}
		range(j, 3) uSlider(e, FACT_VEL + j, vel[j]);

		uSlider(e, FACT_TYPE, e->typeMask);
		uSlider(e, FACT_COLLIDE, e->collideMask);
		uMyTypeMask(e, T_WEIGHTLESS);
		uMyCollideMask(e, 0);

		uSlider(e, FACT_COLOR, e->color);
		e->color = 0x808080;

		int32_t misc = 0;
		if (a.has(h)) misc |= FACT_MISC_HOLD;
		range(j, 4) {
			// Since buttons are flushed between "now" and the tick handlers,
			// we have to look at the future value of these guys.
			// Relatedly, we don't want any previous button presses going to the
			// new factory logic.
			if (e->ctrl.btns[j].v2) {
				e->ctrl.btns[j].v2 = 0;
				misc |= (1 << j);
			}
		}
		uSlider(e, FACT_MISC, misc);

		// Now all the handlers... oof.
		// Tempting to leave e.g. push/pushed intact on the factory,
		// but there's no guarantee it'll stay as a ghost so we might need those.
		uSlider(e, FACT_WHOMOVES, whoMovesHandlers.reverseLookup(e->whoMoves));
		uSlider(e, FACT_TICK, tickHandlers.reverseLookup(e->tick));
		uSlider(e, FACT_TICKHELD, tickHandlers.reverseLookup(e->tickHeld));
		uSlider(e, FACT_CRUSH, crushHandlers.reverseLookup(e->crush));
		uSlider(e, FACT_PUSH, pushHandlers.reverseLookup(e->push));
		uSlider(e, FACT_PUSHED, pushedHandlers.reverseLookup(e->pushed));
		uSlider(e, FACT_FUMBLE, entPairHandlers.reverseLookup(e->onFumble));
		uSlider(e, FACT_FUMBLED, entPairHandlers.reverseLookup(e->onFumbled));
		uSlider(e, FACT_PICKUP, entPairHandlers.reverseLookup(e->onPickUp));
		uSlider(e, FACT_PICKEDUP, entPairHandlers.reverseLookup(e->onPickedUp));

		e->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		e->tick = tickHandlers.get(TICK_FACTORY);
		e->tickHeld = tickHandlers.get(TICK_FACTORY);
		e->crush = crushHandlers.get(CRUSH_NIL);
		e->push = pushHandlers.get(PUSH_NIL);
		e->pushed = pushedHandlers.get(PUSHED_NIL);
		e->onFumble = entPairHandlers.get(ENTPAIR_NIL);
		e->onFumbled = entPairHandlers.get(ENTPAIR_NIL);
		e->onPickUp = entPairHandlers.get(ENTPAIR_NIL);
		e->onPickedUp = entPairHandlers.get(ENTPAIR_NIL);

		// We have to go through all this rigamarole for wires
		// because the new factory uses wires to keep track of
		// what it's creating, so old wires have to be handled
		// actually by a different ent
		ent *wireFactory = NULL;
		wiresAnyOrder(w, e) {
			if (!a.has(w)) continue;
			if (!wireFactory) {
				wireFactory = mkFactoryWires(gs, e);
				player_toggleBauble(gs, me, wireFactory, 0);
			}
			uWire(wireFactory, w);
		}
		e->wires.num = 0;

		// d_vel, d_center, and others could potentially have pending updates,
		// which would then be wrongly applied to the factory.
		// That is probably pretty unlikely based on where we are in the cycle, however.
		// More concerning is potential wires pointed to the factory'd ents.
		//
		// I could make new ents for the factory, rather than converting old ones,
		// which would resolve these issues but require effort to reconstruct the holds + wire destinations.
	}
	fixNewBaubles(gs);
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
		d = abs(a[0]->center[axis] - a[1]->center[axis]);
		printf("Centers: %d", d);
		if (d%2 == 0) {
			printf(" (2*%d)", d/2);
		}
		putchar('\n');

		d -= a[0]->radius[axis] + a[1]->radius[axis];
		printf("Inner: %d", d);
		if (d%2 == 0) {
			printf(" (2*%d)", d/2);
		}
		putchar('\n');
	}
}

void edit_import(gamestate *gs, ent *me, list<char> *data) {
	if (!me) return;
	int axis, dir;
	getAxis(me, &axis, &dir);

	ent *start = gs->ents;

	deserializeSelected(gs, data, me->center, me->vel);

	for (ent *e = gs->ents; e != start; e = e->ll.n) {
		player_toggleBauble(gs, me, e, 0);
	}
	fixNewBaubles(gs);
}

void edit_export(gamestate *gs, ent *me, const char *name) {
	if (!me) return;
	getLists(me);

	for (ent *e = gs->ents; e; e = e->ll.n) {
		e->clone.ix = -1;
	}
	range(i, a.num) a[i]->clone.ix = 0; // This just marks it for serialization

	list<char> data;
	data.init();
	serializeSelected(gs, &data, me->center, me->vel);
	writeFile(name, &data);
	data.destroy();
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
