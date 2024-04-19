#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "player.h"

#include "teamselect.h"

char const * const * const M_TEAMSELECT_HELP = (char const * const[]){
	"mask - player data mask",
	"value - player data is set to this (subject to mask)",
	NULL
};

enum {
	s_mask,
	s_value,
	s_num
};

static_assert(s_num == M_TEAMSELECT_NUM_SLIDERS);

static void updatePlayer(ent *me, player *p) {
	int32_t mask = getSlider(me, s_mask);
	int32_t value = getSlider(me, s_value);

	p->color = me->color;
	p->entity->color = me->color;
	p->data = (value & mask) + (p->data & ~mask);
}

static void teamselect_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	list<player> &ps = gs->players;
	range(i, ps.num) {
		player *p = &ps[i];
		if (p->entity == him) {
			updatePlayer(me, p);
		}
	}
}

void module_teamselect() {
	pushHandlers.reg(PUSH_TEAMSELECT, teamselect_push);
	addEditHelp(&ent::push, teamselect_push, "teamselect", M_TEAMSELECT_HELP);
}
