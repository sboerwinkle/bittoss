#include <stdlib.h>
#include <assert.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../colors.h"
#include "../effects.h"

#include "stackem.h"
#include "platform.h"
#include "edittool.h"

#include "player.h"

enum {
	s_axis,
	s_axis1,
	s_look,
	s_look1,
	s_look2,
	s_vel,
	s_vel1,
	s_grounded,
	s_charge,
	s_cooldown,
	s_equip_processed,
	s_editmode,
	s_buttons,
	s_num
};

static_assert(s_editmode == PLAYER_EDIT_SLIDER);
static_assert(s_charge == PLAYER_CHARGE_SLIDER);
static_assert(s_cooldown == PLAYER_COOLDOWN_SLIDER);

void getAxis(int32_t *dest, ent *e) {
        dest[0] = getSlider(e, s_axis);
        dest[1] = getSlider(e, s_axis1);
}

void getLook(int32_t *dest, ent *e) {
        dest[0] = getSlider(e, s_look);
        dest[1] = getSlider(e, s_look1);
        dest[2] = getSlider(e, s_look2);
}

// We write these guys directly into the sliders,
// because we want these inputs to be visible to the tick function on this frame.
void player_setAxis(ent *e, int32_t *x) {
	if (e->numSliders > s_axis1) {
		e->sliders[s_axis ].v = x[0];
		e->sliders[s_axis1].v = x[1];
	}
}

void player_setLook(ent *e, int32_t *x) {
	if (e->numSliders > s_look2) {
		e->sliders[s_look ].v = x[0];
		e->sliders[s_look1].v = x[1];
		e->sliders[s_look2].v = x[2];
	}
}

// Some buttons are properties on every ent (used e.g. for logic),
// but I need more buttons. Adding them to every ent seems silly
// (and maybe they never should have gone there in the first place),
// so further buttons will be communicated to the player entity as a slider/bitfield.
void player_setButtons(ent *e, int32_t x) {
	e->sliders[s_buttons].v = x;
}

static int player_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_BOTH;
}

static char player_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	int32_t vel[3];
	getVel(vel, him, me);
	// Reset what "stationary" is, according to ground speed
	vel[0] = bound(vel[0], PLAYER_SPD);
	vel[1] = bound(vel[1], PLAYER_SPD);
	if (axis == 2) {
		if (dir < 0) {
			// Indicate that we can jump
			uSlider(me, s_grounded, 1);
		}
	} else {
		// Argh.
		int32_t currentSlide = getSlider(me, s_vel + axis);
		vel[axis] = currentSlide + dir*dv;
		if (dir*vel[axis] > 0) vel[axis] = 0;
	}
	uSlider(me, s_vel, vel[0]);
	uSlider(me, s_vel1, vel[1]);
	return 0;
}

static void player_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (
		getButton(me, 1)
		&& !(1 & getSlider(me, s_equip_processed))
		&& (type(him) & EQUIP_MASK)
		&& !getSlider(me, s_editmode)
		&& !him->holder
	) {
		int32_t tEquip = him->typeMask & T_EQUIP;
		int32_t tEquipSm = him->typeMask & T_EQUIP_SM;
		char primaryEquipment = 0;
		ent *offhandEquipment = NULL;

		holdeesAnyOrder(h, me) {
			int32_t heldEquip = h->typeMask & T_EQUIP;
			int32_t heldEquipSm = h->typeMask & T_EQUIP_SM;

			if (heldEquip) {
				// If held equip is 2-handed,
				// or both require the primary hand,
				// or we're also holding an offhand item,
				// we can't pickup.
				if (heldEquipSm || tEquip || offhandEquipment) return;
				// Otherwise, we're 1-handed and they're offhand, so no issue yet
				primaryEquipment = 1;
			} else if (heldEquipSm) {
				// If this is our second piece of equipment,
				// we can't pickup.
				if (primaryEquipment || offhandEquipment) return;
				// If the new equip is 2-handed,
				// we can't pickup.
				if (tEquip && tEquipSm) return;
				// Otherwise, no issue yet
				offhandEquipment = h;
			}
			// else, some non-equipment holdee, we don't care.
		}

		// By this point, we know we can pick it up, the only question is
		// which hand, and do we have to swap an existing offhand item...
		if (tEquip) {
			if (offhandEquipment) uSlider(offhandEquipment, 0, 1);
		} else {
			// Else we know incoming item is offhand, need to know
			// which hand it goes in. Prefer off-hand if possible.
			char hand = !(offhandEquipment && getSlider(offhandEquipment, 0));
			uSlider(him, 0, hand);
		}

		uPickup(gs, me, him, HOLD_DROP | HOLD_FREEZE);
		// Direct slider update. We absolutely don't want to examine another equipment before
		// this pickup processes, and we don't really care which equipment gets handled if two
		// are touched in the same frame.
		// (it's not a desync risk, just "arbitrary" from the player's view)
		me->sliders[s_equip_processed].v |= 1;

	}
}

static void player_pickup(gamestate *gs, ent *me, ent *him) {
	if (type(him) & EQUIP_MASK) {
		int32_t offset[3];
		getPos(offset, him, me);
		uCenter(him, offset);
	}
}

void player_toggleBauble(gamestate *gs, ent *me, ent *target, int mode) {
	char found = 0;
	holdeesAnyOrder(h, me) {
		// TODO a more sane way to identify baubles
		if ((h->typeMask & T_DECOR) && h->wires.num == 1 && mode == getSlider(h, 0)) {
			range(j, h->wires.num) {
				if (h->wires[j] == target) {
					uDrop(gs, h);
					found = 1;
					// We're just exiting the wire looping here,
					// still need to finish looking through the holdees
					break;
				}
			}
		}
	}
	if (!found) mkBauble(gs, me, target, mode);
}

void player_clearBaubles(gamestate *gs, ent *me, int mode) {
	holdeesAnyOrder(h, me) {
		if ((h->typeMask & T_DECOR) && h->wires.num == 1 && mode == getSlider(h, 0)) {
			uDrop(gs, h);
		}
	}
}

void player_flipBaubles(ent *me) {
	holdeesAnyOrder(h, me) {
		if ((h->typeMask & T_DECOR) && h->wires.num == 1) {
			// Right now I've got my buttons and triggers all mixed up;
			// trigger 1 is just button 3 by another name.
			// (TODO)
			pushBtn(h, 2);
		}
	}
}

// Which hand(s) the given equipment uses (1, 2, or 1+2)
// 2-handed item =>           T_EQUIP + T_EQUIP_SM
// 1-handed (primary) item => T_EQUIP
// ambidextrous item =>       T_EQUIP_SM, slider 0 indicates hand
static char getEquipHands(ent *e) {
	// EQUIP_MASK == T_EQUIP + T_EQUIP_SM
	if (!(e->typeMask & EQUIP_MASK)) return 0;
	char hands = 0;

	int32_t tEquip = e->typeMask & T_EQUIP;
	if (tEquip || !getSlider(e, 0)) {
		hands = 1;
	}
	if ((e->typeMask & T_EQUIP_SM) && (tEquip || getSlider(e, 0))) {
		hands |= 2;
	}
	return hands;
}

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
	vel[2] = getButton(me, 0) && grounded ? -384 : 0;
	range(i, 2) {
		vel[i] = (PLAYER_SPD/axisMaxis)*axis[i] - sliders[i];
	}
	// Can't push as hard in the air
	boundVec(vel, grounded ? 56 : 16, 2);
	range(i, 2) {
		uSlider(me, s_vel + i, sliders[i] + vel[i]);
	}
	uVel(me, vel);

	// "shooting" and grabbing

	int charge = getSlider(me, s_charge);
	int cooldown = getSlider(me, s_cooldown);
	char fire = getTrigger(me, 0);
	char fire2 = getTrigger(me, 1);
	char utility = getButton(me, 1);
	int32_t otherBtns = getSlider(me, s_buttons);
	char altfire = !!(otherBtns & PLAYER_BTN_ALTFIRE1);
	char altfire2 = !!(otherBtns & PLAYER_BTN_ALTFIRE2);

	// Controls edittool on/off, only manipulated externally by game commands
	if (getSlider(me, s_editmode)) {
		if (altfire) {
			player_clearBaubles(gs, me, 0);
		}
		if (altfire2) {
			player_clearBaubles(gs, me, 1);
		}
		if (cooldown >= 3) {
			if (utility && getButton(me, 0)) { // Shift+Space
				player_flipBaubles(me);
				cooldown = 0;
			} else if (fire) {
				edittool_select(gs, me, 0);
				cooldown = 0;
			} else if (fire2) {
				edittool_select(gs, me, 1);
				cooldown = 0;
			}
		}
	} else {
		// This is fun block making stuff, i.e. not edittool

		// Count held equipment. If somehow we have a violation of the hand rules
		// (e.g. some other thing has attached an equipment???), for now we force a
		// drop on the relevant hand.
		char hands = 0;
		char drops = 0;
		holdeesAnyOrder(h, me) {
			int32_t eHands = getEquipHands(h);
			// Any already taken hands get put in the drop list
			drops |= (hands & eHands);
			hands |= eHands;
		}

		// Update s_equip_processed
		int32_t equipProcessed = getSlider(me, s_equip_processed);
		// equipProcessed tracks equipment actions, namely Shift, Shift+LMB, Shift+RMB.
		if (altfire) {
			if (!(equipProcessed & 2)) {
				equipProcessed |= 3;
				drops |= 1;
			}
		} else {
			equipProcessed &= ~2;
		}
		if (altfire2) {
			if (!(equipProcessed & 4)) {
				equipProcessed |= 5;
				drops |= 2;
			}
		} else {
			equipProcessed &= ~4;
		}
		// altfire/altfire2 also mark utility (Shift) as processed, since the purpose of
		// pressing Shift might have been to get at altfire/altfire2.
		// It's also possible Shift is released by the time the frame processes, though,
		// so we do this check last:
		if (!utility) equipProcessed &= ~1;
		// (if `utility` is true, we don't act on that here, it's for pickups in the "push" handler)
		uSlider(me, s_equip_processed, equipProcessed);

		// We don't always need this but sometimes we do
		int32_t look[3];
		getLook(look, me);

		if (drops) {
			if (drops & hands) {
				int32_t vel[3];
				range(i, 3) vel[i] = 200 * look[i] / axisMaxis;
				holdeesAnyOrder(h, me) {
					if (drops & getEquipHands(h)) {
						uVel(h, vel);
						uCenter(h, vel);
						uDrop(gs, h);
					}
				}
			} else {
				// Else we have a drop request, but nothing in that hand.
				// This is how we swap offhand item hands, let's try that
				holdeesAnyOrder(h, me) {
					// We know it can't be 2-handed, so T_EQUIP_SM means offhand here
					if (h->typeMask & T_EQUIP_SM) {
						// `drops` is a bitfield, but from context we know it must be either
						// 1 or 2 right now, so we can be a little sneaky and subtract 1 to
						// get the index of the hand the "drop" was requested on
						// (which should be the destination hand)
						uSlider(h, 0, drops-1);
					}
				}
			}
		}

		// Not just a shortcut, also disables recharge of "innate" ammo if both hands are full
		if (hands == 3) return;

		if (cooldown >= 5 && charge >= 60) {
			if (fire && !(hands & 1) && !(gs->gamerules & EFFECT_NO_BLOCK)) {
				charge -= 60;
				cooldown = 0;
				getLook(look, me);
				int32_t offset[3];
				range(i, 3) {
					// 1000 - 900 = 100; 100/2 = 50, so 50 units we can use for the offset.
					// `look` maxes out as 64, so we /2.
					offset[i] = look[i]/2;
					look[i] *= (double)(14*32)/axisMaxis;
				}
				ent* stackem = mkStackem(gs, me, offset);
				uVel(stackem, look);
			} else if (charge >= 180 && fire2 && !(hands & 2) && !(gs->gamerules & EFFECT_NO_PLATFORM)) {
				charge -= 180;
				cooldown = 0;
				getLook(look, me);
				int32_t color = (random(gs) % 2) ? CLR_WHITE : CLR_BLUE;
				range(i, 3) {
					look[i] *= 1.25/axisMaxis * (500 + platformSize[i]);
				}
				mkPlatform(gs, me, look, color);
			}
		}
	}

	if (charge < 180) uSlider(me, s_charge, charge + 2);
	if (cooldown < 5) uSlider(me, s_cooldown, cooldown + 1);
}

int32_t playerSize[3] = {500, 500, 500};

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
