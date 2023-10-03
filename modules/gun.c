#include <stdlib.h>
#include <assert.h>

#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

#include "player.h"

#include "gun.h"

enum {
	s_ammo,
	s_cooldown,
	s_reload,
	s_num
};

static_assert(s_num == M_GUN_NUM_SLIDERS);

static const int32_t bulletSize[3] = {100, 100, 100};

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

		int32_t vel[3];
		getLook(vel, h);
		int32_t pos[3];
		range(i, 3) {
			pos[i] = me->center[i] + (32*vel[i]/axisMaxis);
			vel[i] = me->vel[i] + (512*vel[i]/axisMaxis);
		}
		ent* bullet = initEnt(
			gs, me,
			pos, vel, bulletSize,
			1,
			T_WEIGHTLESS, T_TERRAIN + T_OBSTACLE + T_DEBRIS
		);
		uSlider(bullet, 0, 60);
		bullet->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		bullet->color = 0x808080;
		bullet->pushed = pushedHandlers.get(PUSHED_BULLET);
		bullet->tick = tickHandlers.get(TICK_BULLET);
		bullet->tickHeld = tickHandlers.get(TICK_HELD_BULLET);
		bullet->onPickedUp = entPairHandlers.get(PICKED_UP_BULLET);
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

static char bullet_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	uPickup(gs, him, me, HOLD_DROP | HOLD_FREEZE);
	uSlider(me, 0, 0);
	return 0;
}

static void bullet_picked_up(gamestate *gs, ent *me, ent *him) {
	// We could probably do this when it's pushed?
	// But I don't really know what side effects could come from
	// changing the collide mask in the middle of collision processing
	uMyTypeMask(me, 0);
	uMyCollideMask(me, 0);
}

static void bullet_tick(gamestate *gs, ent *me) {
	int32_t ttl = getSlider(me, 0);
	if (ttl <= 0) {
		uDead(gs, me);
		return;
	}
	uSlider(me, 0, ttl-1);
}

static void bullet_tick_held(gamestate *gs, ent *me) {
	int32_t ttl = getSlider(me, 0);
	if (ttl <= -30 || ttl > 0) { // >0 shouldn't happen...
		// TODO: bullet killing stuff logic
		uDead(gs, me);
	} else {
		ttl--;
		me->color = 0x808080 - 0x10101 * (0x80*ttl/-30);
		uSlider(me, 0, ttl);
	}
}

void module_gun() {
	tickHandlers.reg(TICK_HELD_GUN, gun_tick_held);
	tickHandlers.reg(TICK_HELD_BULLET, bullet_tick_held);
	tickHandlers.reg(TICK_BULLET, bullet_tick);
	entPairHandlers.reg(PICKED_UP_BULLET, bullet_picked_up);
	pushedHandlers.reg(PUSHED_BULLET, bullet_pushed);
}
