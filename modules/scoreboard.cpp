#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../main.h"

#include "scoreboard.h"

enum {
	s_foo_todo,
	s_foo_rename,
	s_team_blu,
	s_team_red,
	s_num
};
static_assert(s_num == M_SCOREBOARD_NUM_SLIDERS);

static void scoreboard_tick(gamestate *gs, ent *me) {
	list<player> *ps = gs->players;
	int alive = 0;
	range(i, ps->num) {
		player *p = &(*ps)[i];
		if (!p->entity) continue;
		alive |= 1 << (3 & p->data);
	}

	int32_t result = getSlider(me, 0);
	int32_t timer = getSlider(me, 1);

	if (result != alive) {
		uSlider(me, 0, alive);
		timer = 150;
	}

	if (timer) {
		timer--;
		uSlider(me, 1, timer);
		if (timer) return;
		if (alive == 2 || alive == 4 || alive == 0) {
			int blu = getSlider(me, s_team_blu);
			int red = getSlider(me, s_team_red);
			// winner green (99 vs 99)
			// 123456789-123456789-1234
			char msg[24];
			const char* winner;
			if (alive == 2) {
				winner = "blu";
				blu++;
				uSlider(me, s_team_blu, blu);
			} else if (alive == 4) {
				winner = "red";
				red++;
				uSlider(me, s_team_red, red);
			} else {
				winner = "tie";
			}
			snprintf(msg, 24, "winner %s (%d vs %d)", winner, blu, red);
			showMessage(gs, msg);
			for (ent *e = gs->ents; e; e = e->ll.n) {
				if (e != me) uErase(gs, e);
			}
			// We erased everything, so the "expected" state is now emptiness.
			uSlider(me, 0, 0);
			// Now the leap of faith
			requestReload(gs);
			uMyCollideMask(me, 0);
		} else {
			uMyCollideMask(me, T_TERRAIN);
		}
	}
}

void module_scoreboard() {
	tickHandlers.reg(TICK_SCOREBOARD, scoreboard_tick);
}
