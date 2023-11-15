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
		timer = 30;
	}

	if (timer) {
		timer--;
		uSlider(me, 1, timer);
		if (!timer && (alive == 2 || alive == 4 || alive == 0)) {
			char msg[13];
			const char* winner;
			if (alive == 2) winner = "green";
			else if (alive == 4) winner = "blue";
			else winner = "tie";
			sprintf(msg, "winner %s", winner);
			showMessage(gs, msg);
			uMyCollideMask(me, 0);
			for (ent *e = gs->ents; e; e = e->ll.n) {
				if (e != me) uErase(gs, e);
			}
		}
	}
}

void module_scoreboard() {
	tickHandlers.reg(TICK_SCOREBOARD, scoreboard_tick);
}
