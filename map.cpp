// Not sure if this should be in `modules/` since it isn't core engine stuff,
// but for now it'll just chill here I guess.

#include "handlerRegistrar.h"

#include "modules/player.h"
#include "modules/ground.h"

ent* mkHero(int n, int total) {
	int32_t team = (n % 2) + 1; // 1 or 2 (teams are powers of 2)
	int32_t pos[3];
	// X axis is major axis of arena, team determines spawn side.
	pos[0] = 70000 * (3-team*2); // +/- 70K
	pos[1] = n/2 * 1000; // Multiples of 1K, based on index within team
	pos[2] = -8000;

	return mkPlayer(pos, team);
}

static void mkBase(int32_t xOffset, int32_t team) {
	int32_t width = groundWidth[0] * 2;
	draw_t white = getDrawHandler(handlerByName("clr-white"));
	draw_t blue = getDrawHandler(handlerByName("clr-blue"));
	int32_t pos[3];
	pos[2] = 0; // Ground is at "sea level"
	range(x, 3) {
		pos[0] = xOffset + width * (x-1);
		range(y, 3) {
			pos[1] = width * (y-1);
			mkGround(pos, ((x+y)%2) ? blue : white);
		}
	}

	pos[0] = xOffset;
	pos[1] = 0;
	pos[2] = -1024;
	mkFlagSpawner(pos, team);
}

void mkMap() {
	mkBase(64000, 1);
	mkBase(-64000, 2);
}
