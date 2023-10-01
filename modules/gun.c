#include <stdlib.h>
#include <assert.h>

#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "gun.h"

enum {
	s_ammo,
	s_cooldown,
	s_reload,
	s_num
};

static_assert(s_num == M_GUN_NUM_SLIDERS);

static void gun_tick_held(gamestate *gs, ent *me) {
	// Doesn't do anything unless held by a person (or similar)
	ent *h = me->holder;
	if (!(type(h) & T_INPUTS)) return;

	int32_t cooldown = getSlider(me, s_cooldown);
	int32_t ammo = getSlider(me, s_ammo);
	if (cooldown > 0) {
		uSlider(me, s_cooldown, cooldown - 1);
	} else if (getTrigger(h, 0) && ammo) {
		uSlider(me, s_cooldown, 4);
		uSlider(me, s_ammo, ammo-1);
		uSlider(me, s_reload, 0);
		// TODO: Actually shoot something lol
		return;
	}

	if (ammo != 10) {
		int32_t reload = getSlider(me, s_reload) + 1;
		if (reload >= 90) {
			uSlider(me, s_reload, 0);
			uSlider(me, s_ammo, 10);
		} else {
			uSlider(me, s_reload, reload);
		}
	}
}

void module_gun() {
	tickHandlers.reg(TICK_HELD_GUN, gun_tick_held);
}

/*
static void player_tick(gamestate *gs, ent *me) {
	// Jumping and movement

	char grounded = getSlider(me, s_grounded) > 0;
	uSlider(me, s_grounded, 0);

	int axis[2];
	getAxis(axis, me);

	int sliders[2];
	sliders[0] = getSlider(me, s_vel);
	sliders[1] = getSlider(me, s_vel1);

	int vel[3];
	vel[2] = getButton(me, 0) && grounded ? -192 : 0;
	range(i, 2) {
		vel[i] = (128/axisMaxis)*axis[i] - sliders[i];
	}
	// Can't push as hard in the air
	boundVec(vel, grounded ? 14 : 4, 2);
	range(i, 2) {
		uSlider(me, s_vel + i, sliders[i] + vel[i]);
	}
	uVel(me, vel);

	// "shooting" and grabbing

	int charge = getSlider(me, s_charge);
	int cooldown = getSlider(me, s_cooldown);
	char fire = getTrigger(me, 0);
	char altFire = getTrigger(me, 1);
	char utility = getButton(me, 1);

	// Controls edittool on/off, only manipulated externally by game commands
	if (getSlider(me, s_editmode)) {
		if (utility) { // Shift pressed - clear or flip baubles
			if (fire) player_clearBaubles(gs, me, 0);
			if (altFire) player_clearBaubles(gs, me, 1);
			if (cooldown >= 5 && getButton(me, 0)) {
				player_flipBaubles(me);
				cooldown = 0;
			}
		} else { // Shift not pressed - click to select
			if (cooldown >= 10 && fire) {
				cooldown = 0;
				mkThumbtack(gs, me, 0);
			} else if (cooldown >= 10 && altFire) {
				cooldown = 0;
				mkThumbtack(gs, me, 1);
			}
		}
	} else {
		// This is fun block making stuff, i.e. not edittool
		int numHoldees = 0;
		holdeesAnyOrder(h, me) {
			if (h->typeMask & T_DECOR) continue;
			if (++numHoldees > 1) break; // Don't care about counting any higher than 2
		}
		if (!utility) uSlider(me, s_equip_processed, 0);
		// We don't always need this but sometimes we do
		int32_t look[3];
		if (numHoldees) {
			getLook(look, me);
			if (numHoldees > 1 || (utility && !getSlider(me, s_equip_processed))) {
				int32_t vel[3];
				range(i, 3) vel[i] = 100 * look[i] / axisMaxis;
				holdeesAnyOrder(h, me) {
					if (h->typeMask & T_DECOR) continue;
					uVel(h, vel);
					uCenter(h, vel);
					uDrop(gs, h);
				}
				uSlider(me, s_equip_processed, 1);
				cooldown = 0;
			} else {
			}
		} else if (cooldown >= 10 && charge >= 60) {
			if (fire && !(gs->gamerules & EFFECT_NO_BLOCK)) {
				charge -= 60;
				cooldown = 0;
				getLook(look, me);
				// Hack - not scaling `look` here, but that's fine for a "small" offset
				ent* stackem = mkStackem(gs, me, look);
				range(i, 3) { look[i] *= (double)(7*32)/axisMaxis; }
				uVel(stackem, look);
			} else if (charge >= 180 && altFire && !(gs->gamerules & EFFECT_NO_PLATFORM)) {
				charge -= 180;
				cooldown = 0;
				getLook(look, me);
				int32_t color = (random(gs) % 2) ? CLR_WHITE : CLR_BLUE;
				range(i, 3) {
					look[i] *= 1.25/axisMaxis * (512 + platformSize[i]);
				}
				mkPlatform(gs, me, look, color);
			}
		}
	}

	if (charge < 180) uSlider(me, s_charge, charge + 1);
	if (cooldown < 10) uSlider(me, s_cooldown, cooldown + 1);
}

int32_t playerSize[3] = {512, 512, 512};

ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs, NULL,
		pos, vel, playerSize,
		s_num,
		T_OBSTACLE + (team*TEAM_BIT) + T_INPUTS + T_NO_DRAW_FP, T_OBSTACLE + T_TERRAIN
	);
	ret->whoMoves = whoMovesHandlers.get(WHOMOVES_PLAYER);
	ret->color = 0xFFFFFF;
	ret->pushed = pushedHandlers.get(PUSHED_PLAYER);
	ret->push = pushHandlers.get(PUSH_PLAYER);
	ret->tick = tickHandlers.get(TICK_PLAYER);
	ret->onPickUp = entPairHandlers.get(PICKUP_PLAYER);
	ret->friction = 0;
	return ret;
}

void module_player() {
	whoMovesHandlers.reg(WHOMOVES_PLAYER, player_whoMoves);
	tickHandlers.reg(TICK_PLAYER, player_tick);
	pushedHandlers.reg(PUSHED_PLAYER, player_pushed);
	pushHandlers.reg(PUSH_PLAYER, player_push);
	entPairHandlers.reg(PICKUP_PLAYER, player_pickup);
}
*/
