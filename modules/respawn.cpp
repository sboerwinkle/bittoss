#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "player.h"
#include "eyes.h"

#include "respawn.h"

char const * const * const M_RESPAWN_HELP = (char const * const[]){
	"mask - player data mask",
	"value - req'd player data after masking",
	"delay - how long they've been dead",
	"cooldown - how long between spawns",
	"counter - working memory for cooldown",
	"width - Width of spawned player",
	"height - Height of spawned player",
	NULL
};

static void respawn_tick(gamestate *gs, ent *me) {
	int32_t cooldown = getSlider(me, 4);
	if (cooldown > 0) {
		uSlider(me, 4, cooldown - 1);
		return;
	}
	// Respawner can be suppressed with logic signal
	if (getButton(me, 0)) return;

	int32_t mask = getSlider(me, 0);
	int32_t check = getSlider(me, 1);
	int32_t delay = getSlider(me, 2);
	list<player> *ps = &gs->players;
	player *toRevive = NULL;
	range(i, ps->num) {
		player *p = &(*ps)[i];
		if (p->reviveCounter > delay && (p->data & mask) == check) {
			toRevive = p;
			delay = p->reviveCounter;
		}
	}
	if (!toRevive) return;

	int32_t width = 0, height = 0;
	if (me->numSliders >= 7) {
		width = getSlider(me, 5);
		height = getSlider(me, 6);
	}
	if (width <= 0 || height <= 0) {
		width = 1000;
		height = 1000;
	}
	toRevive->entity = mkPlayer(gs, me->center, (toRevive->data % 2) + 1, width, height);
	mkEye(gs, toRevive->entity);
	toRevive->reviveCounter = 0;
	toRevive->entity->color = toRevive->color;
	uSlider(me, 4, getSlider(me, 3));

	// Emit logic pulse when it respawns someone
	wiresAnyOrder(w, me) {
		pushBtn(w, 0);
	}
}

static void cmdRespawn(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, 0, 7);
	e->tick = tickHandlers.get(TICK_RESPAWN);
	e->tickHeld = tickHandlers.get(TICK_RESPAWN);
}

void module_respawn() {
	tickHandlers.reg(TICK_RESPAWN, respawn_tick);
	addEditHelp(&ent::tickHeld, respawn_tick, "respawn", M_RESPAWN_HELP);
	addEntCommand("respawn", cmdRespawn);
}
