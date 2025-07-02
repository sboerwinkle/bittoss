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

#include "../compounds/refuelIslands.h"

char const * const * const M_SPACETRASH_HELP = (char const * const[]) {
	"threshold - out of approx 2,000,000,000",
	"spawn_dist - how far away along -X to spawn",
	"cube_size - radius",
	"base_speed - +X",
	"vel_size - radius",
	"multiplier",
	NULL
};

static void spawn(gamestate *gs, ent *me, int m) {
	int32_t spawn_dist = getSlider(me, 1);
	int32_t cube_size = getSlider(me, 2);
	int32_t base_speed = getSlider(me, 3);
	int32_t vel_size = getSlider(me, 4);
	int32_t multiplier = getSlider(me, 5);

	if (cube_size < 0 || vel_size < 0 || vel_size >= base_speed) return;

	{
		uint32_t input = gs->rand;
		uint32_t otherRandomNumber = splitmix32(&input);
		gs->rand = otherRandomNumber % (uint32_t)randomMax;
	}

	int32_t pos[3], vel[3];

	// Platforms coming towards us faster will appear proportionally more often.
	// (this helps us maintain an even distribution in play, since faster platforms die quicker)
	// Our distribution looks something like this:
	// (speed increasing vertically, frequency horizontally)
	//
	// X
	// X
	// X
	// ---X
	// ----X    <-- `base_speed`
	// -----X   <-- `base_speed + vel_size`
	// X
	// X
	//
	// The area "under the curve" (indicated by "-" above) can be split into a rectangular section
	// and a triangular section. The ratio of how much area belongs to each is going to be:
	// tri/(rect+tri) = vel_size/base_speed
	// Once we know which section it's in, we need to apply the inverse of the CDF (cumulative distribution function)
	// for that distribution. The rectangular section is uniformly distributed (very nice), but for the triangular section
	// the inverse of the CDF is basically a square root (with some scaling etc etc).
	int32_t tri_cutoff = (int64_t)randomMax * vel_size / base_speed;
	int32_t approach = random(gs);
	if (approach < tri_cutoff) {
		// We do another random call here just because `sqrt` is going to lose us precision anyway,
		// there's no need to aggravate that by applying it to something we know is less than `tri_cutoff`.
		int32_t value = sqrt(random(gs));
		// For scaling. Due to integer truncation, several different random values result in `value==sqrtMax`.
		int32_t sqrtMax = sqrt(randomMax);
		// `value` fits in about 16 bits (b/c of sqrt), so multiplication here doesn't need int64_t.
		vel[0] = (vel_size*2) * value / sqrtMax + base_speed - vel_size;
	} else {
		// We've got plenty of random bits left in `approach` (assuming tri_cutoff is reasonably small),
		// so we can just use that as input for our modulus.
		vel[0] = approach % (vel_size*2+1) + base_speed - vel_size;
	}
	pos[0] = -spawn_dist;

	vel[1] = (random(gs) % (vel_size*2+1)) - vel_size;
	vel[2] = (random(gs) % (vel_size*2+1)) - vel_size;
	pos[1] = (random(gs) % (cube_size*2+1)) - cube_size;
	pos[2] = (random(gs) % (cube_size*2+1)) - cube_size;
	// The idea is that we're "aiming at" some random point in a cube with radius `cube_size`,
	// but since we're spawning some distance away (`spawn_dist`) we have to adjust starting
	// position based on our velocity so we're aimed appropriately.
	int32_t dist = spawn_dist + (random(gs) % (cube_size*2+1)) - cube_size;
	int32_t time = dist / vel[0];
	// `time` will be truncated, so we add another +0.5 to avoid bias
	pos[1] -= time*vel[1] + vel[1]/2;
	pos[2] -= time*vel[2] + vel[2]/2;

	range(i, 3) {
		// `multiplier` and `m` are really just there to help visualize things faster during level
		// design. It basically lets us speed time up by a multiple, so we can see what it'll look
		// like once settled. In "real life", `multiplier==1 && m==0`.
		pos[i] += vel[i] * m;
		vel[i] *= multiplier;

		// Make it relative to this ent. Probably `me->vel` is 0, but it might not be!
		vel[i] += me->vel[i];
		pos[i] += me->center[i];
	}

	builderContext ctx;
	ctx.init(gs);
	ctx.offsetCenter(pos);
	ctx.offsetVel(vel);
	mkRefuelIsland(&ctx);
	ctx.destroy();
}

static void st_tick(gamestate *gs, ent *me) {
	if (getButton(me, 0) ^ getButton(me, 1)) return;
	int32_t mult = getSlider(me, 5);

	// This is broadly the same as `createDebris()` in effects.cpp
	int32_t threshold = getSlider(me, 0);

	range(m, mult) {
		if (random(gs) < threshold) {
			const rand_t origSeed = gs->rand;
			// Right now the item spawning might be a little intensive, so don't mess with that
			// until we actually need it (for the "real" step). It's okay if the items "pop in"
			// a little late, since they're popping in anyways! The important thing is that the
			// random seed in `gs` is treated the same regardless of if this is the root state,
			// so other, independent random outcomes won't jump when the item spawning happens.
			if (gs == rootState) spawn(gs, me, m);
			gs->rand = origSeed;
		}
	}
}

static void cmdSpacetrash(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 6);
	e->tick = tickHandlers.get(TICK_SPACETRASH);
	e->tickHeld = tickHandlers.get(TICK_SPACETRASH);
}

void module_spacetrash() {
	tickHandlers.reg(TICK_SPACETRASH, st_tick);
	addEditHelp(&ent::tickHeld, st_tick, "spacetrash", M_SPACETRASH_HELP);
	addEntCommand("spacetrash", cmdSpacetrash);
}
