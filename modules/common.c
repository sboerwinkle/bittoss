#include <math.h>

#include "../handlerRegistrar.h"
#include "../entUpdaters.h"

int32_t getApproachSpeed(int32_t d, int32_t a) {
	// Avoid floating point exceptions (divide-by-zero, or negative sqrt).
	// `d == 0` is actually okay, but we know the answer is 0 in that case anyway.
	if (a <= 0 || d <= 0) return 0;

	// If it decelerates at rate `a` on every subsequent turn,
	// a velocity of `n*a+r` will come to rest in `n*(n+1)/2*a + r*(n+1)` units.
	// The trick here is reversing `n*(n+1)/2`... Solve for x:
	// x*(x+1)/2 = q
	// x^2 + x = 2*q
	// x^2 + x - 2*q = 0
	// quadratic formula, a=1,b=1,c=-2q
	// (-1 +- sqrt(1+8q)) / 2
	// There will always be a negative and non-negative root here, we always want the non-negative one.
	// We also want it truncated to an integer. We also don't lose anything by immediately truncating
	// the result of `sqrt` to an integer - since we're scaling it down by 2 anyway, the fractional part
	// only gets smaller (and stays < 1)

	int32_t q = d / a;
	int32_t n = (((int32_t)sqrt(1 + 8*q)) - 1) / 2;
	// Now that we know the true value of `n`, we just solve for `r` in our original expression for stopping distance.
	int32_t r = (d - n*(n+1)/2*a)/(n+1);
	return n*a + r;
}

int32_t getStoppingDist(int32_t s, int32_t a) {
	// Avoid nonsense inputs, as well as one case where the answer is 0 anyways
	if (s <= 0 || a <= 0) return 0;
	int32_t n = s / a;
	int32_t r = s % a;
	return n*(n+1)/2*a + r*(n+1);
}

int32_t getMagnitude(int32_t *v, int n) {
	int32_t ret = 0;
	range(i, n) {
		int32_t x = abs(v[i]);
		if (x > ret) ret = x;
	}
	return ret;
}

static int move_me(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static void cursed_tick(gamestate *gs, ent *me) {
	uDead(gs, me);
}

static void decor_fumbled(gamestate *gs, ent *me, ent *him) {
	// When dropped, make us collidable.
	// Also clears this handler, since it isn't necessary anymore
	uMyTypeMask(me, me->typeMask | T_DEBRIS);
	uMyCollideMask(me, me->collideMask | T_TERRAIN | T_OBSTACLE | T_DEBRIS);
	me->onFumbled = entPairHandlers.get(ENTPAIR_NIL);
}

void module_common() {
	whoMovesHandlers.reg(WHOMOVES_ME, move_me);
	tickHandlers.reg(TICK_CURSED, cursed_tick);
	entPairHandlers.reg(FUMBLED_DECOR, decor_fumbled);
}
