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
			vel[i] = me->vel[i] + (10000*vel[i]/axisMaxis);
		}
		ent* bullet = initEnt(
			gs, me,
			pos, vel, bulletSize,
			1,
			T_WEIGHTLESS, T_TERRAIN + T_OBSTACLE + T_DEBRIS
		);
		uSlider(bullet, 0, 60);
		bullet->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		bullet->color = 0xC0C0C0;
		bullet->pushed = pushedHandlers.get(PUSHED_BULLET);
		bullet->tick = tickHandlers.get(TICK_BULLET);
		bullet->tickHeld = tickHandlers.get(TICK_HELD_BULLET);
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

static char bullet_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int32_t dx, int32_t dv) {
	if (getSlider(me, 0) == 0) return 0;
	uPickup(gs, him, me, HOLD_DROP | HOLD_FREEZE);
	// Immediate slider update, hacky but who cares
	me->sliders[0].v = 0;

	// I'll probably have to use 64-bit math here soon.
	// Bullet velocity is 100000, so say collision speed might be
	// 200000 for fun. We can't exceed around 280000 without
	// hitting int32_t limits, but if we make the scale any
	// smaller we start to get more noticeable errors
	if (dv) {
		int32_t scale = 7000;
		int32_t ratio = dx*scale / dv;
		range(i, 2) {
			int dim = (axis+1+i)%3;
			me->center[dim] -= me->vel[dim]*ratio/scale;
		}
	}

	// I have no idea if doing this here is fine. Probably it is??
	// Worst case we can change back to uMyTypeMask / uMyCollideMask
	me->typeMask = 0;
	me->collideMask = 0;
	return 0;
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
		ent *h = me->holder;
		int32_t t = type(h);
		// I think with the current rate of fire and bullet TTL,
		// standing perfectly still firing continuously maxes out at 6 concurrent hits.
		// That 6th one might easily be one frame shy of registering, idk.
		int hitsReqd;
		if (t & T_DEBRIS) hitsReqd = 1;
		else if (t & T_TERRAIN) hitsReqd = (t & T_HEAVY) ? 15 : 5;
		else hitsReqd = 3;

		holdeesAnyOrder(c, h) {
			if (c->tickHeld == bullet_tick_held) {
				hitsReqd--;
				if (!hitsReqd) {
					uDead(gs, h);
					break;
				}
			}
		}

		uDead(gs, me);
	} else {
		ttl--;
		me->color = 0xC0C0C0 - 0x10101 * (0xC0*ttl/-30);
		uSlider(me, 0, ttl);
	}
}

void module_gun() {
	tickHandlers.reg(TICK_HELD_GUN, gun_tick_held);
	tickHandlers.reg(TICK_HELD_BULLET, bullet_tick_held);
	tickHandlers.reg(TICK_BULLET, bullet_tick);
	pushedHandlers.reg(PUSHED_BULLET, bullet_pushed);
}
