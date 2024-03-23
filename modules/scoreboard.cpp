#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../main.h"

#include "scoreboard.h"

enum {
	s_result,
	s_timer,
	s_team_blu,
	s_team_red,
	s_num
};
static_assert(s_num == M_SCOREBOARD_NUM_SLIDERS);

static void scoreboard_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	// We're only interested in collisions with other scoreboards
	if (me->tick != him->tick) return;

	// This makes it so that it won't "wake up" until at least one person is alive,
	// so any level initialization mechanics can run.
	// Probably not necessary unless you're doing something goofy like procedurally
	// generating levels using in-game logic blocks...
	uSlider(me, s_result, 0);
	uSlider(me, s_timer, 0);
	// Carry over score from last round
	uSlider(me, s_team_blu, getSlider(him, s_team_blu));
	uSlider(me, s_team_red, getSlider(him, s_team_red));
	// Signal start of a new round (if we're connected to anything);
	wiresAnyOrder(w, me) { pushBtn(w, 0); }
}

static void scoreboard_tick(gamestate *gs, ent *me) {
	list<player> &ps = gs->players;
	int alive = 0;
	range(i, ps.num) {
		player *p = &ps[i];
		if (!p->entity) continue;
		alive |= 1 << (3 & p->data);
	}

	int32_t result = getSlider(me, s_result);
	int32_t timer = getSlider(me, s_timer);

	if (result != alive) {
		uSlider(me, s_result, alive);
		timer = 150;
	}

	if (timer) {
		timer--;
		uSlider(me, s_timer, timer);
		if (timer) return;
		if (alive == 2 || alive == 4 || alive == 0) {
			int blu = getSlider(me, s_team_blu);
			int red = getSlider(me, s_team_red);
			// winner green (99 vs 99)
			// 123456789-123456789-1234
			char msg[24];
			const char* winner;
			if (alive == 2) {
				winner = "blue";
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
			uSlider(me, s_result, 0);
			// Now the leap of faith
			requestReload(gs);
			uMyCollideMask(me, T_TERRAIN);
		}
	}
}

void module_scoreboard() {
	tickHandlers.reg(TICK_SCOREBOARD, scoreboard_tick);
	pushHandlers.reg(PUSH_SCOREBOARD, scoreboard_push);
}
