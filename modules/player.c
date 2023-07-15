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

static int player_pushed(gamestate *gs, ent *me, ent *him, int axis, int dir, int dx, int dv) {
	entState *state = &me->state;
	if (axis == 2) {
		if (dir < 0) {
			int vel[3];
			getVel(vel, him, me);
			// Reset what "stationary" is, according to ground speed
			uStateSlider(state, 0, bound(vel[0], 128));
			uStateSlider(state, 1, bound(vel[1], 128));
			// Indicate that we can jump
			uStateSlider(state, 2, 1);
		}
	} else {
		// Tried to be more clever about this in the past, but it got weird.
		// I think being able to suction towards surfaces in midair is not the
		// weirdest ability, and it has no issues with multiple collisions in a frame.
		uStateSlider(state, axis, 0);
	}
	return r_pass;
}

static void player_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (getButton(me, 1) && (type(him) & T_FLAG)) {
		// Skip if already any holdees
		holdeesAnyOrder(h, me) { return; }
		uPickup(gs, me, him);
	}
}

static void player_tick(gamestate *gs, ent *me) {
	entState *s = &me->state;

	// Jumping and movement

	char grounded = getSlider(s, 2) > 0;
	uStateSlider(s, 2, 0);

	int axis[2];
	getAxis(axis, me);

	int sliders[2];
	sliders[0] = getSlider(s, 0);
	sliders[1] = getSlider(s, 1);

	int vel[3];
	vel[2] = (grounded && getButton(me, 0)) ? -192 : 0;
	range(i, 2) {
		vel[i] = 4*axis[i] - sliders[i];
	}
	boundVec(vel, 10, 2);
	int divisor = grounded ? 1 : 2;
	range(i, 2) {
		uStateSlider(s, i, sliders[i] + vel[i]);
		vel[i] /= divisor;
	}
	uVel(me, vel);

	// "shooting" and grabbing

	int charge = getSlider(s, 3);
	int cooldown = getSlider(s, 4);
	char fire = getTrigger(me, 0);

	/*
	// edittool stuff (WIP)
	if (cooldown >= 10 && fire) {
		cooldown = 0;
		mkThumbtack(gs, me);
	}
	list<ent*> &wires = me->wires;
	range(i, wires.num) {
		ent *e = wires[i];
		uUnwire(me, e);
		char found = 0;
		holdeesAnyOrder(h, me) {
			// TODO I think the idea was not to identify what "type" things are, except for by their type flags.
			//      I'm beginning to think this is an impractical idea, and maybe should be tossed out.
			//      So I'm doing this this way, and we'll see how it feels.
			if (h->typeMask & T_DECOR) {
				range(j, h->wires.num) {
					if (h->wires[j] == e) {
						uDrop(gs, h);
						found = 1;
						// We're just exiting the wire looping here,
						// still need to finish looking through the holdees
						break;
					}
				}
			}
		}
		if (!found) mkBauble(gs, me, e);
	}
	// */


	//* This is fun block making stuff that we're turning off for now while I play with getting edittool functional
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
			int colorToggle = getSlider(s, 5);
			uStateSlider(s, 5, !colorToggle);
			int32_t color = colorToggle ? CLR_WHITE : CLR_BLUE;
			range(i, 3) {
				// 0.040635 == 1.3 / axisMaxis
				look[i] *= 0.040625 * (512 + platformSize[i]);
			}
			mkPlatform(gs, me, look, color);
		}
	}
	// */
	if (charge < 180) uStateSlider(s, 3, charge + 1);
	if (cooldown < 10) uStateSlider(s, 4, cooldown + 1);
}

int32_t playerSize[3] = {512, 512, 512};

ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team) {
	int32_t vel[3] = {0, 0, 0};
	ent *ret = initEnt(
		gs, NULL,
		pos, vel, playerSize,
		6,
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
