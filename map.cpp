// Not sure if this should be in `modules/` since it isn't core engine stuff,
// but for now it'll just chill here I guess.

#include <cstdint>

#include "util.h"
#include "ent.h"

#include "modules/player.h"
#include "modules/eyes.h"

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
