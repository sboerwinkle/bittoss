#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "player.h"
#include "dust.h"

enum {
	s_hand,
	s_ammo,
	s_reload,
	s_cooldown,
	s_ammo_max,
	s_reload_max,
	s_cooldown_max,
	s_speed,
	s_num
};

static char const * const * const HELP = (char const * const[]) {
	"hand - internal use",
	"ammo - how many charges are currently available",
	"reload - progress towards next charge",
	"cooldown - time until next charge can be fired",
	"ammo_max - how many charges can be stored",
	"reload_max - how long it takes to reload a charge",
	"cooldown_max - minimum time between charges firing",
	"speed - jump power",
	NULL
};

static int32_t const dustArea[3] = {500, 500, 0};

static void jumper_tick_held(gamestate *gs, ent *me) {
	// Doesn't do anything unless held by a person (or similar)
	ent *h = me->holder;
	if (!(type(h) & T_INPUTS)) return;

	int32_t ammo = getSlider(me, s_ammo);
	if (ammo < getSlider(me, s_ammo_max)) {
		int32_t reload = 1 + getSlider(me, s_reload);
		if (reload >= getSlider(me, s_reload_max)) {
			reload = 0;
			ammo++;
		}
		uSlider(me, s_reload, reload);
	}

	int32_t cooldown = getSlider(me, s_cooldown);
	if (cooldown > 0) {
		uSlider(me, s_cooldown, cooldown - 1);
	} else if (ammo) {
		int32_t hand = getSlider(me, s_hand);
		if (getTrigger(h, hand)) {
			ammo--;
			int32_t vec[3] = {0, 0, -getSlider(me, s_speed)};
			ent *holdRoot = me->holdRoot;
			uVel(holdRoot, vec);

			memcpy(vec, holdRoot->center, sizeof(vec));
			vec[2] += holdRoot->radius[2];
			mkDustCloud(gs, holdRoot, vec, holdRoot->vel, dustArea, 15, 200, 0x808080, 6);

			uSlider(me, s_cooldown, getSlider(me, s_cooldown_max) - 1);
		}
	}
	uSlider(me, s_ammo, ammo);
}

static void cmdJumper(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP_SM, s_num);
	e->tickHeld = tickHandlers.get(TICK_HELD_JUMPER);
}

void module_jumper() {
	tickHandlers.reg(TICK_HELD_JUMPER, jumper_tick_held);
	addEditHelp(&ent::tickHeld, jumper_tick_held, "jumper", HELP);
	addEntCommand("jumper", cmdJumper);
}
