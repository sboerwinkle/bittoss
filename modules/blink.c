#include <stdlib.h>
#include <assert.h>

#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"

#include "blink.h"

enum {
	s_hand,
	s_cooldown,
	s_cooldown_max,
	s_range,
	s_num
};

static_assert(s_num == M_BLINK_NUM_SLIDERS);

char const * const * const M_BLINK_HELP = (char const * const[]) {
	"hand - internal use",
	"cooldown - internal countdown to next activation",
	"max cooldown - how long between activations",
	"range - distance to blink"
};

static void blink_tick_held(gamestate *gs, ent *me) {
	// Doesn't do anything unless held by a person (or similar)
	ent *h = me->holder;
	if (!(type(h) & T_INPUTS)) return;

	int32_t cooldown = getSlider(me, s_cooldown);
	if (cooldown > 0) {
		uSlider(me, s_cooldown, cooldown - 1);
		return;
	}
	int32_t hand = getSlider(me, s_hand);
	if (!getTrigger(h, hand)) return;

	uSlider(me, s_cooldown, getSlider(me, s_cooldown_max) - 1);

	int32_t dir[3];
	getLook(dir, h);
	int32_t range = getSlider(me, s_range);
	range(i, 3) dir[i] = range*dir[i]/axisMaxis;

	uCenter(me->holdRoot, dir);
}

void module_blink() {
	tickHandlers.reg(TICK_HELD_BLINK, blink_tick_held);
}
