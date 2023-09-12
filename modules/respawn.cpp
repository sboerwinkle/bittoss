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
	player *toRevive = NULL;
	range(i, ps->num) {
		player *p = &(*ps)[i];
		if (p->reviveCounter > delay && (p->data & mask) == check) {
			toRevive = p;
			delay = p->reviveCounter;
		}
	}
	if (!toRevive) return;

	toRevive->entity = mkPlayer(gs, me->center, (toRevive->data % 2) + 1);
	mkEye(gs, toRevive->entity);
	toRevive->reviveCounter = 0;
	toRevive->entity->color = toRevive->color;
	uSlider(me, 4, getSlider(me, 3));

	// Lets spawning tie into the logic wiring system,
	// though for simplicity it's always just signal 0
	wiresAnyOrder(w, me) {
		pushBtn(w, 0);
	}
}

void module_respawn() {
	tickHandlers.reg(TICK_RESPAWN, respawn_tick);
}
