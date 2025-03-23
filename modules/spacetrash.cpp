#include <math.h>

#include "../util.h"
#include "../list.h"
#include "../file.h"
#include "../ent.h"
#include "../builderContext.h"
#include "../main.h"
#include "../edit.h"
#include "../random.h"
#include "../entGetters.h"
#include "../handlerRegistrar.h"
#include "../modules.h"

#include "../compounds/refuelIslands.h"

char const * const * const M_SPACETRASH_HELP = (char const * const[]) {
	"threshold - out of approx 2,000,000,000",
	"size - radius along all dimensions for the cube",
	"speed",
	NULL
};

static void st_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) ^ getButton(me, 1)) return;

	// This is broadly the same as `createDebris()` in effects.cpp
	int32_t threshold = getSlider(me, 0);
	if (random(gs) >= threshold) return; // Nothing else to do, hooray!
	// We will restore this at the end
	const rand_t origSeed = random(gs);

	// Right now the item spawning might be a little intensive, so don't mess with that
	// until we actually need it (for the "real" step). It's okay if the items "pop in"
	// a little late, since they're popping in anyways! The important thing is that the
	// random seed in `gs` is treated the same regardless of if this is the root state,
	// so other, independent random outcomes won't jump when the item spawning happens.
	if (gs != rootState) return;

	int32_t size = getSlider(me, 1);
	int32_t speed = getSlider(me, 2);
	if (size < 0 || speed < 0) return;

	{
		uint32_t input = origSeed; // this is totally fine I think. I'm tired. We know seed is in (0, randomMax].
		uint32_t otherRandomNumber = splitmix32(&input);
		gs->rand = otherRandomNumber % (uint32_t)randomMax;
	}

	int32_t flavor = random(gs);

	int dimension = flavor % 3;
	int sign = (flavor % 2) * 2 - 1;
	flavor /= 6;

	int32_t pos[3], vel[3];

	vel[(dimension+1)%3] = (random(gs) % (speed*2+1)) - speed;
	vel[(dimension+2)%3] = (random(gs) % (speed*2+1)) - speed;
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
	int32_t value = sqrt(random(gs));
	int32_t sqrtMax = sqrt(randomMax);
	vel[dimension] = sign * (1 + speed*value/sqrtMax);

	pos[dimension] = -size * sign;
	// We assume just using a modulus is fine here. It will get a little
	// weird if `size` approaches the same scale as `randomMax`, though.
	pos[(dimension+1)%3] = (random(gs) % (size*2+1)) - size;
	pos[(dimension+2)%3] = (random(gs) % (size*2+1)) - size;

	range(i, 3) {
		vel[i] += me->vel[i];
		pos[i] += me->center[i];
	}

	builderContext ctx;
	ctx.init(gs);
	ctx.offsetCenter(pos);
	ctx.offsetVel(vel);
	mkRefuelIsland(&ctx);
	ctx.destroy();
	gs->rand = origSeed;
}

static void cmdSpacetrash(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 3);
	e->tick = tickHandlers.get(TICK_SPACETRASH);
	e->tickHeld = tickHandlers.get(TICK_SPACETRASH);
}

void module_spacetrash() {
	tickHandlers.reg(TICK_SPACETRASH, st_tick);
	addEditHelp(&ent::tickHeld, st_tick, "spacetrash", M_SPACETRASH_HELP);
	addEntCommand("spacetrash", cmdSpacetrash);
}
