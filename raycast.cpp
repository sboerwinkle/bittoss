#include <math.h>

#include "ent.h"

struct rat { int32_t n, d; };
// Only strictly rational when `d > 0`. `d` should never be negative.
// When `d == 0`, you get values which behave something like +/-Inf and NaN
//   (depending on how `n` compares to 0).
// Based on how we do comparisons, there are some surprises:
// +/-Inf == +/-Inf (regardless of sign)
// NaN == Anything
// However, we still have +/-Inf comparing correctly to rationals,
// so if we play our cards right we can use this fairly simple comparison operator with success.
#define lt(a, b) ((int64_t)a.n*b.d < (int64_t)b.n*a.d)

ent* selectCast(gamestate *gs, ent *parent, int32_t * look) {
	int32_t *pos = parent->center;
	ent *holdRoot = parent->holdRoot;

	int32_t flip[3];
	range(i, 3) {
		if (look[i] >= 0) {
			flip[i] = 1;
		} else {
			flip[i] = -1;
			look[i] *= -1;
		}
	}

	// Check intersection of look ray with each ent.
	ent *winner = NULL;
	rat best = {.n = INT32_MAX, .d = 1}; // Highest possible rational
	for (ent *e = gs->ents; e; e = e->ll.n) {
		if (e->holdRoot == holdRoot) continue;
		rat lower = {.n = 0, .d = 1}; // 0/1 == 0
		rat upper = best;
		range(d, 3) {
			int32_t x = flip[d]*(e->center[d] - pos[d]);

			rat f1, f2;
			f1.n = x - e->radius[d];
			f2.n = x + e->radius[d];
			f1.d = f2.d = look[d];
			// f1 and f2 might not be strictly rational, so be careful with phrasing here...
			if (lt(lower, f1)) lower = f1;
			if (lt(f2, upper)) upper = f2;
			if (!lt(lower, upper)) goto next_ent;
		}
		if (lower.n) {
			// We only take things a positive distance in front of us, not things we're already inside
			best = lower;
			winner = e;
		}
		next_ent:;
	}
	return winner;
}

// This was copied very closely from selectCast.
// Since this isn't crucial for synchronization we can use floating point types.
double cameraCast(gamestate *gs, int32_t *pos, double *look, ent *ignoreRoot) {
	int32_t flip[3];
	range(i, 3) {
		// We're going to be potentially handling division by zero below,
		// so we really don't want any "negative zeroes" to come through,
		// as that messes with the sign of the resulting infinity.
		// (This is all to do with how the `double` type works,
		//  if you're confused by the above sentence from a "math" perspective.)
		// Therefore, we use `signbit`, rather than a simple `< 0` comparison.
		if (signbit(look[i])) {
			flip[i] = -1;
			look[i] *= -1;
		} else {
			flip[i] = 1;
		}
	}

	double best = INFINITY;
	for (ent *e = gs->ents; e; e = e->ll.n) {
		// We skip invisible things, since we're raycasting for a camera
		if (e->holdRoot == ignoreRoot || e->color == -1) continue;
		double lower = 0;
		double upper = best;
		range(d, 3) {
			int32_t x = flip[d]*(e->center[d] - pos[d]);

			// Padding of 300 units makes it harder for walls to wind up
			// inside the GL near clipping plane.
			// Can always experiment with this value.
			double f1 = (x - e->radius[d] - 300) / look[d];
			double f2 = (x + e->radius[d] + 300) / look[d];
			// f1 and f2 might not be strictly Real, so be careful with phrasing here...
			if (lower < f1) lower = f1;
			if (f2 < upper) upper = f2;
			if (upper <= lower) goto next_ent;
		}
		if (lower > 0) {
			// We only take things a positive distance in front of us, not things we're already inside
			best = lower;
		}
		next_ent:;
	}
	return best;
}
