#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../edit.h"

#include "player.h"

#include "gun.h"

enum {
	s_ammo,
	s_cooldown,
	s_reload,
	s_ammo_max, // 10
	s_cooldown_max, // 3
	s_reload_max, // 45
	s_speed, // 14000
	s_num
};

char const * const * const M_GUN_HELP = (char const * const[]) {
	"(internal) ammo",
	"(internal) cooldown",
	"(internal) reload progress",
	"max ammo",
	"cooldown (frames per shot)",
	"reload time (frames)",
	"bullet speed",
	NULL
};

static const int32_t bulletSize[3] = {100, 100, 100};

static void gun_tick_held(gamestate *gs, ent *me) {
	// Doesn't do anything unless held by a person (or similar)
	ent *h = me->holder;
	if (!(type(h) & T_INPUTS)) return;

	int32_t cooldown = getSlider(me, s_cooldown);
	int32_t ammo = getSlider(me, s_ammo);
	if (cooldown > 1) {
		uSlider(me, s_cooldown, cooldown - 1);
	} else if (getTrigger(h, 0) && ammo) {
		uSlider(me, s_cooldown, getSlider(me, s_cooldown_max));
		uSlider(me, s_ammo, ammo-1);
		uSlider(me, s_reload, 0);

		int32_t vel[3];
		getLook(vel, h);
		int32_t pos[3];
		int32_t spd = getSlider(me, s_speed);
		range(i, 3) {
			pos[i] = me->center[i] + (32*vel[i]/axisMaxis);
			vel[i] = me->vel[i] + (spd*vel[i]/axisMaxis);
		}
		ent* bullet = initEnt(
			gs, me,
			pos, vel, bulletSize,
			1,
			T_WEIGHTLESS, T_TERRAIN + T_OBSTACLE + T_DEBRIS
		);
		uSlider(bullet, 0, 30);
		bullet->whoMoves = whoMovesHandlers.get(WHOMOVES_ME);
		bullet->color = 0xC0C0C0;
		bullet->pushed = pushedHandlers.get(PUSHED_BULLET);
		bullet->tick = tickHandlers.get(TICK_BULLET);
		bullet->tickHeld = tickHandlers.get(TICK_HELD_BULLET);
		return;
	}

	int32_t ammoMax = getSlider(me, s_ammo_max);
	if (ammo != ammoMax) {
		int32_t reload = getSlider(me, s_reload) + 1;
		if (reload >= getSlider(me, s_reload_max)) {
			uSlider(me, s_reload, 0);
			uSlider(me, s_ammo, ammoMax);
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

	// Existing notes said this scale starts to have issues at
	// a relative velocity of 280000, not sure if that's right offhand.
	// Changing it down gives larger errors, but changing it up
	// means we'll hit int limits more easily.
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
	if (ttl <= -15 || ttl > 0) { // >0 shouldn't happen...
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
		me->color = 0xC0C0C0 - 0x10101 * (0xC0*ttl/-15);
		uSlider(me, 0, ttl);
	}
}

static void cmdGun(gamestate *gs, ent *e) {
	basicTypeCommand(gs, e, T_EQUIP, s_num);
	e->tickHeld = tickHandlers.get(TICK_HELD_GUN);
}

void module_gun() {
	tickHandlers.reg(TICK_HELD_GUN, gun_tick_held);
	tickHandlers.reg(TICK_HELD_BULLET, bullet_tick_held);
	tickHandlers.reg(TICK_BULLET, bullet_tick);
	pushedHandlers.reg(PUSHED_BULLET, bullet_pushed);
	addEditHelp(&ent::tickHeld, gun_tick_held, "gun", M_GUN_HELP);
	addEntCommand("gun", cmdGun);
}
