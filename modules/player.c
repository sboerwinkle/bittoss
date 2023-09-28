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
	s_num
};

static_assert(s_editmode == PLAYER_EDIT_SLIDER);

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
void setAxis(ent *e, int32_t *x) {
	if (e->numSliders > s_axis1) {
		e->sliders[s_axis ].v = x[0];
		e->sliders[s_axis1].v = x[1];
	}
}

void setLook(ent *e, int32_t *x) {
	if (e->numSliders > s_look2) {
		e->sliders[s_look ].v = x[0];
		e->sliders[s_look1].v = x[1];
		e->sliders[s_look2].v = x[2];
	}
}

static int player_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_BOTH;
}

static char player_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	int32_t vel[3];
	getVel(vel, him, me);
	// Reset what "stationary" is, according to ground speed
	vel[0] = bound(vel[0], 128);
	vel[1] = bound(vel[1], 128);
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
	if (getButton(me, 1) && !getSlider(me, s_equip_processed) && (type(him) & T_EQUIP)) {
		// Skip if already holding any equipment
		holdeesAnyOrder(h, me) {
			if (h->typeMask & T_EQUIP) return;
		}

		// This is all a little dicey, since positions aren't finalized at the time we're handling a push.
		// We could use wires here, so the pickup actually is requested next frame. That would probably have
		// less impact, but requires us to dedicate player-wires to this purpose.
		uPickup(gs, me, him, HOLD_DROP | HOLD_FREEZE);
		int32_t offset[3];
		getPos(offset, him, me);
		uCenter(him, offset);
		// Once we get HOLD_SINGLE working, maybe this doesn't happen until the pickup is processed?
		uSlider(me, s_equip_processed, 1);
	}
}

void player_toggleBauble(gamestate *gs, ent *me, ent *target, int mode) {
	char found = 0;
	holdeesAnyOrder(h, me) {
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
				/* Logic here for hovering the item where you're looking.
				 * Might be useful later.
				int32_t pos[3], vel[3], r[3], a[3];
				holdeesAnyOrder(h, me) {
					if (h->typeMask & T_DECOR) continue;
					getPos(pos, me, h);
					getVel(vel, me, h);
					getSize(r, h);
					range(i, 3) {
						int32_t surface = 1024 + r[i];
						if (abs(pos[i]) > surface + 1024) {
							uDrop(gs, h);
							a[i] = 0;
						} else {
							int32_t offset = (int)(look[i] * surface / (double)axisMaxis) - pos[i];
							int32_t goalVel = bound(offset / 5, 64);
							a[i] = bound(goalVel - vel[i], 12);
						}
					}
					uVel(h, a);
				}
				*/
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
	ret->friction = 0;
	return ret;
}

void module_player() {
	whoMovesHandlers.reg(WHOMOVES_PLAYER, player_whoMoves);
	tickHandlers.reg(TICK_PLAYER, player_tick);
	pushedHandlers.reg(PUSHED_PLAYER, player_pushed);
	pushHandlers.reg(PUSH_PLAYER, player_push);
}
