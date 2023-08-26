#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"
#include "eyes.h"

static void respawn_tick(gamestate *gs, ent *me) {
	int32_t cooldown = getSlider(me, 4);
	if (cooldown > 0) {
		uSlider(me, 4, cooldown - 1);
		return;
	}

	int32_t mask = getSlider(me, 0);
	int32_t check = getSlider(me, 1);
	int32_t delay = getSlider(me, 2);
	list<player> *ps = gs->players;
	range(i, ps->num) {
		player *p = &(*ps)[i];
		if (p->reviveCounter > delay && (p->data & mask) == check) {
			p->entity = mkPlayer(gs, me->center, (p->data % 2) + 1);
			mkEye(gs, p->entity);
			p->reviveCounter = 0;
			p->entity->color = p->color;
			uSlider(me, 4, getSlider(me, 3));
			return;
		}
	}
}

void module_respawn() {
	tickHandlers.reg(TICK_RESPAWN, respawn_tick);
}
