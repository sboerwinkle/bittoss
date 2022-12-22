#include <math.h>

#include "util.h"
#include "ent.h"
#include "entUpdaters.h"
#include "entFuncs.h"
#include "main.h"
#include "handlerRegistrar.h"

/*
This file is intended to be world-level effects that are applied every tick.
Probably at some point these will go into a registry (like the whoMoves handlers & co)
so that they can be serialized.
*/

void doGravity() {
        int32_t grav[3] = {0, 0, 8};
        ent *i;
        for (i = rootEnts; i; i = i->LL.n) {
                if (!(i->typeMask & T_WEIGHTLESS)) uVel(i, grav);
        }
}

void doLava() {
        ent *e;
        for (e = ents; e; e = e->ll.n) {
                if (e->center[2] > 64000) crushEnt(e);
        }
}

void doCrushtainer() {
	// This is small enough corner-to-corner (< 444K even with 128K radius per side) that the vision range (480K) encloses it safely
	for (ent *e = ents; e; e = e->ll.n) {
		if (abs(e->center[0]) > 128000) crushEnt(e);
		if (abs(e->center[1]) > 64000) crushEnt(e);
		if (abs(e->center[2]) > 64000) crushEnt(e);
	}
}

void createDebris() {
	// We're going to create incoming platforms at the edges of the crushtainer boundary.
	// Assumptions are that all of infinite space is populated with some density of platforms,
	// and each velocity has independent components uniformly distributed over some range (-max_v, max_v).

	// Really the number of boxes appearing per face pre frame should be modelled by the Poisson distribution -
	// https://en.wikipedia.org/wiki/Poisson_distribution#Random_variate_generation
	// For now we make do with flipping a "coin" once per face per frame, which is somewhat close
	// so long as the probability stays low.

	const rand_t limit = random_max / 55; // Odds of appearing each frame
	const int max_v = 96;
	// 1.5x wider than manmade platforms, same height
	const int32_t size[3] = {6400, 6400, 512};
	const int32_t bounds[3] = {128000, 64000, 64000};

	range(d, 3) {
		range(s, 2) {
			rand_t encounter = get_random();
			// Play area is longer along X axis, so fewer platforms at X endcaps
			if (encounter > (d == 0 ? limit/2 : limit)) continue;
			int32_t vel[3];
			// The orthogonal directions are uniformly distributed, no fuss here
			vel[(d+1)%3] = (get_random() % (max_v*2+1)) - max_v;
			vel[(d+2)%3] = (get_random() % (max_v*2+1)) - max_v;
			// The direction normal to the spawn plane is a bit odd at first glance.
			// We assume an even soup of platforms, so the rate of incidence doesn't
			// change over time. However, for a given incoming mystery platform,
			// it's more likely to be moving faster - just since faster ones will
			// cross the same volume of space faster, meaning they run into spawn
			// planes (assuming there are more out there in the void) more often.
			// Said another way, if there's to be a uniform distribution inside our
			// space, then the fast ones (which leave sooner) have to be created
			// proportionally more often.
			// This results in a triangular distribution of speeds.
			double v = sqrt(get_random());
			int sign = 2*s - 1; // 0/1 -> -1/1
			vel[d] = sign * (v / sqrt(random_max) * max_v + 1);

			int32_t pos[3];
			pos[d] = -bounds[d] * sign;
			// The starting positions are distributed over 256001 possilibities,
			// but compared to the approx 2 billion possible outcomes that'll still
			// look pretty smooth if we just check it through a modulous.
			int dim = (d+1)%3;
			pos[dim] = (get_random() % (2*bounds[dim] + 1)) - bounds[dim];
			dim = (d+2)%3;
			pos[dim] = (get_random() % (2*bounds[dim] + 1)) - bounds[dim];

			// Why re-use `encounter`? Because I want to and I can, that's why, now shaddup
			int color = encounter % 3;
			const char *colors[3] = {"clr-mag-1", "clr-mag-2", "clr-mag-3"};

			// Barren rectangle floating through space
			ent *e = initEnt(pos, vel, size, 0, 0);
			// Behaves like a platform now
			e->typeMask = T_TERRAIN + T_HEAVY + T_WEIGHTLESS;
			e->collideMask = T_TERRAIN;
			e->whoMoves = getWhoMovesHandler(handlerByName("platform-whomoves"));
			// Ask scheme to dig up the draw function by name,
			// since drawing is something the debris needs that's
			// still handled in scheme-land.
			save_from_C_call(sc);
			scheme_eval(sc, mk_symbol(sc, colors[color]));
			sc->value->references++;
			decrem(sc, e->draw);
			e->draw = sc->value;
		}
	}
}
