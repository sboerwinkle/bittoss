#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "player.h"

char const * const * const M_TEAMSELECT_HELP = (char const * const[]){
	"mask - player data mask",
	"value - player data is set to this (subject to mask)",
	"color - if non-zero, set player color from this block",
	NULL
};

enum {
	s_mask,
	s_value,
	s_color,
	s_num
};

static void updatePlayer(ent *me, player *p) {
	int32_t mask = getSlider(me, s_mask);
	int32_t value = getSlider(me, s_value);

	// `s_color` was a later addition, assume `1` if not present on this block
	if (me->numSliders <= s_color || getSlider(me, s_color)) {
		p->color = me->color;
		p->entity->color = me->color;
	}
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

static void cmdTeamselect(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, s_num);
	e->push = pushHandlers.get(PUSH_TEAMSELECT);
}

void module_teamselect() {
	pushHandlers.reg(PUSH_TEAMSELECT, teamselect_push);
	addEditHelp(&ent::push, teamselect_push, "teamselect", M_TEAMSELECT_HELP);
	addEntCommand("teamselect", cmdTeamselect);
}
