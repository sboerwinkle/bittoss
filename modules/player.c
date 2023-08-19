#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"
#include "../colors.h"

#include "stackem.h"
#include "platform.h"
#include "edittool.h"

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
			uStateSlider(me, 2, 1);
		}
	} else {
		// It won't report as 0 yet, because the collision isn't finished,
		// but we know it will be 0.
		vel[axis] = 0;
		// A little boost, to make up for friction.
		// Most of the actual moving we do in the tick handler (so it's centralized),
		// but friction is applied per collision (e.g. 2x in corners) so we counter it
		// per collision as well.
		int32_t up[3];
		up[0] = up[1] = 0;
		up[2] = -3;
		uVel(me, up);
	}
	uStateSlider(me, 0, vel[0]);
	uStateSlider(me, 1, vel[1]);
	return 0;
}

static void player_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (getButton(me, 1) && (type(him) & T_FLAG)) {
		// Skip if already any (non-decor) holdees
		holdeesAnyOrder(h, me) {
			if (!(h->typeMask & T_DECOR)) return;
		}
		uPickup(gs, me, him, HOLD_MOVE);
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

	char grounded = getSlider(me, 2) > 0;
	uStateSlider(me, 2, 0);

	int axis[2];
	getAxis(axis, me);

	int sliders[2];
	sliders[0] = getSlider(me, 0);
	sliders[1] = getSlider(me, 1);

	int vel[3];
	vel[2] = getButton(me, 0) && grounded ? -192 : 0;
	range(i, 2) {
		vel[i] = 4*axis[i] - sliders[i];
	}
	// In the air, our efforts only yield half the impulse
	int divisor = grounded ? 1 : 2;
	boundVec(vel, 28 / divisor, 2);
	vel[0] /= 2;
	vel[1] /= 2;
	range(i, 2) {
		uStateSlider(me, i, sliders[i] + vel[i]*divisor);
	}
	uVel(me, vel);

	// "shooting" and grabbing

	int charge = getSlider(me, 3);
	int cooldown = getSlider(me, 4);
	char fire = getTrigger(me, 0);
	char altFire = getTrigger(me, 1);

	// Controls edittool on/off, only manipulated externally by game commands
	if (getSlider(me, 6)) {
		if (getButton(me, 1)) { // Shift pressed - clear or flip baubles
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
		// We don't always need this but sometimes we do
		int32_t look[3];
		if (numHoldees) {
			if (numHoldees > 1 || (cooldown >= 10 && fire)) {
				holdeesAnyOrder(h, me) {
					if (h->typeMask & T_DECOR) continue;
					uDrop(gs, h);
				}
				cooldown = 0;
			} else {
				getLook(look, me);
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
							// 0.03125 == 1 / axisMaxis
							int32_t offset = (int)(look[i] * surface * 0.03125) - pos[i];
							int32_t goalVel = bound(offset / 5, 64);
							a[i] = bound(goalVel - vel[i], 12);
						}
					}
					uVel(h, a);
				}
			}
		} else if (cooldown >= 10 && charge >= 60) {
			if (fire) {
				charge -= 60;
				cooldown = 0;
				getLook(look, me);
				ent* stackem = mkStackem(gs, me, look);
				range(i, 3) { look[i] *= 7; }
				uVel(stackem, look);
			} else if (charge >= 180 && getTrigger(me, 1)) {
				charge -= 180;
				cooldown = 0;
				getLook(look, me);
				int colorToggle = getSlider(me, 5);
				uStateSlider(me, 5, !colorToggle);
				int32_t color = colorToggle ? CLR_WHITE : CLR_BLUE;
				range(i, 3) {
					// 0.040635 == 1.3 / axisMaxis
					look[i] *= 0.040625 * (512 + platformSize[i]);
				}
				mkPlatform(gs, me, look, color);
			}
		}
	}

	if (charge < 180) uStateSlider(me, 3, charge + 1);
	if (cooldown < 10) uStateSlider(me, 4, cooldown + 1);
}

int32_t playerSize[3] = {512, 512, 512};

ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs, NULL,
		pos, vel, playerSize,
		7,
		T_OBSTACLE + (team*TEAM_BIT), T_OBSTACLE + T_TERRAIN
	);
	ret->whoMoves = whoMovesHandlers.getByName("player-whomoves");
	ret->color = 0xFFFFFF;
	ret->pushed = pushedHandlers.getByName("player-pushed");
	ret->push = pushHandlers.getByName("player-push");
	ret->tick = tickHandlers.getByName("player-tick");
	return ret;
}

void module_player() {
	whoMovesHandlers.reg("player-whomoves", player_whoMoves);
	tickHandlers.reg("player-tick", player_tick);
	pushedHandlers.reg("player-pushed", player_pushed);
	pushHandlers.reg("player-push", player_push);
}
