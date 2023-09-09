#include <math.h>

#include "util.h"
#include "ent.h"
#include "entUpdaters.h"
#include "entFuncs.h"
#include "main.h"
#include "handlerRegistrar.h"
#include "colors.h"

/*
This file is intended to be world-level effects that are applied every tick.
Probably at some point these will go into a registry (like the whoMoves handlers & co)
so that they can be serialized.
*/

void doGravity(gamestate *gs) {
	int32_t grav[3] = {0, 0, 8};
	ent *i;
	for (i = gs->rootEnts; i; i = i->LL.n) {
		if (!(i->typeMask & T_WEIGHTLESS)) uVel(i, grav);
	}
}

void doLava(gamestate *gs) {
	ent *e;
	for (e = gs->ents; e; e = e->ll.n) {
		if (e->center[2] > 64000) uDead(gs, e);
	}
}

static void crush(gamestate *gs, int32_t radius) {
	// This is small enough corner-to-corner (< 444K even with 128K radius per side) that the vision range (480K) encloses it safely
	for (ent *e = gs->ents; e; e = e->ll.n) {
		if (
			abs(e->center[0]) > radius * 2
			|| abs(e->center[1]) > radius
			|| abs(e->center[2]) > radius
		) {
			uDead(gs, e);
		}
	}
}

void doCrushtainer(gamestate *gs) {
	crush(gs, 64000);
}

void doBigCrushtainer(gamestate *gs) {
	crush(gs, 256000);
}

static void bounce(gamestate *gs, int32_t radius) {
	for (ent *e = gs->ents; e; e = e->ll.n) {
		if (
			abs(e->center[0]) > radius * 2
			|| abs(e->center[1]) > radius
			|| abs(e->center[2]) > radius
		) {
			if (e->typeMask & T_TERRAIN) {
				range(i, 3) {
					int32_t bound = i ? radius : radius*2;
					if (
						(e->center[i] > bound && e->vel[i] > 0) ||
						(e->center[i] < -bound && e->vel[i] < 0)
					) {
						e->vel[i] *= -1;
					}
				}
			} else {
				uDead(gs, e);
			}
		}
	}
}

void doBouncetainer(gamestate *gs) {
	bounce(gs, 64000);
}

void createDebris(gamestate *gs) {
	// We're going to create incoming platforms at the edges of the crushtainer boundary.
	// Assumptions are that all of infinite space is populated with some density of platforms,
	// and each velocity has independent components,
	// each either 0 or uniformly distributed over some range (-max_v, max_v).

	// Really the number of boxes appearing per face pre frame should be modelled by the Poisson distribution -
	// https://en.wikipedia.org/wiki/Poisson_distribution#Random_variate_generation
	// For now we make do with flipping a "coin" once per face per frame, which is somewhat close
	// so long as the probability stays low.

	const rand_t limit = randomMax / 200; // Odds of appearing each frame
	const int max_v = 96;
	// 1.5x wider than manmade platforms, same height
	const int32_t size[3] = {6400, 6400, 512};
	const int32_t bounds[3] = {128000, 64000, 64000};

	range(d, 3) {
		range(s, 2) {
			rand_t encounter = random(gs);
			// Play area is longer along X axis, so fewer platforms at X endcaps
			if (encounter > (d == 0 ? limit/2 : limit)) continue;
			int32_t vel[3];
			// The orthogonal directions are either 0 or uniformly distributed
			// We can re-use `encounter` here b/c why not
			vel[(d+1)%3] = (encounter &  3) ? 0 : (random(gs) % (max_v*2+1)) - max_v;
			vel[(d+2)%3] = (encounter & 12) ? 0 : (random(gs) % (max_v*2+1)) - max_v;
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
			double v = sqrt(random(gs));
			int sign = 2*s - 1; // 0/1 -> -1/1
			vel[d] = sign * (v / sqrt(randomMax) * max_v + 1);

			int32_t pos[3];
			pos[d] = -bounds[d] * sign;
			// The starting positions are distributed over 256001 possilibities,
			// but compared to the approx 2 billion possible outcomes that'll still
			// look pretty smooth if we just check it through a modulous.
			int dim = (d+1)%3;
			pos[dim] = (random(gs) % (2*bounds[dim] + 1)) - bounds[dim];
			dim = (d+2)%3;
			pos[dim] = (random(gs) % (2*bounds[dim] + 1)) - bounds[dim];

			// This actually doesn't correlate with any of the previous
			// checks we've done against `encounter` assuming RAND_MAX
			// is reasonably big. I am going to assume that.
			int32_t color = encounter % 3;
			switch (color) {
				case 0: color = CLR_MAG_1; break;
				case 1: color = CLR_MAG_2; break;
				case 2: color = CLR_MAG_3;
			}

			// Make the actual platform, starting with the basic entity physics stuff
			ent *e = initEnt(
				gs, NULL,
				pos, vel, size,
				0,
				T_TERRAIN + T_HEAVY + T_WEIGHTLESS, T_TERRAIN
			);
			// Make it more platform-y
			e->whoMoves = whoMovesHandlers.get(WHOMOVES_PLATFORM);
			e->color = color;
		}
	}
}
