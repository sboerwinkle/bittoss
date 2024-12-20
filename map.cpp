// Not sure if this should be in `modules/` since it isn't core engine stuff,
// but for now it'll just chill here I guess.

#include <cstdint>

#include "util.h"
#include "ent.h"
#include "handlerRegistrar.h"
#include "colors.h"
#include "effects.h"

#include "modules/player.h"
#include "modules/eyes.h"
#include "modules/ground.h"
#include "modules/flag.h"

ent* mkHero(gamestate *gs, int n, int total) {
	int32_t team = (n % 2) + 1; // 1 or 2 (teams are powers of 2)
	int32_t pos[3];
	// X axis is major axis of arena, team determines spawn side.
	pos[0] = 70000 * (3-team*2); // +/- 70K
	pos[1] = n/2 * 1024; // Multiples of 1K, based on index within team
	pos[2] = -8000;

	ent *p = mkPlayer(gs, pos, team, 1000, 1000);
	mkEye(gs, p);
	return p;
}

static void mkBase(gamestate *gs, int32_t xOffset, int32_t team) {
	int32_t width = groundSize[0] * 2;
	const int32_t white = CLR_WHITE;
	const int32_t blue = CLR_BLUE;
	int32_t pos[3];
	pos[2] = 0; // Ground is at "sea level"
	range(x, 3) {
		pos[0] = xOffset + width * (x-1);
		range(y, 3) {
			pos[1] = width * (y-1);
			mkGround(gs, pos, ((x+y)%2) ? blue : white);
		}
	}

	pos[0] = xOffset;
	pos[1] = 0;
	pos[2] = -1024;
	mkFlagSpawner(gs, pos, team);
}

void mkMap(gamestate *gs) {
	gs->gamerules = EFFECT_CRUSH + EFFECT_BLOCKS + EFFECT_GRAV + EFFECT_SPAWN;
	mkBase(gs, 64000, 1);
	mkBase(gs, -64000, 2);
}
