#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

static int player_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_BOTH;
}

static void player_draw(ent *e) {
	int typ = type(e);
	drawEnt(
		e,
		(typ & TEAM_BIT) ? 1.0 : 0.5,
		(typ & (2*TEAM_BIT)) ? 1.0 : 0.5,
		(typ & (4*TEAM_BIT)) ? 1.0 : 0.5
	);
}

static int player_pushed(ent *me, ent *him, int axis, int dir, int dx, int dv) {
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

static void player_push(ent *me, ent *him, byte axis, int dir, int displacement, int dv) {
	if (
		getButton(me, 1)
		&& (type(him) & T_FLAG)
		// In general we want this to be order-independent, hence the weirness with "count-holdees" in scheme-land.
		// However, this is a quick way to check if there are any at all, and we're respecting that order doesn't matter.
		&& !me->holdee
	) {
		uPickup(me, him);
	}
}

static void player_tick(ent *me) {
	entState *s = me->state;

	// Jumping and movement

	char grounded = getSlider(s, 2) > 0;
	uStateSlider(s, 2, 0);

	int axis[2];
	getAxis(me, axis); // TODO impl (`s` or `me`?)

	int sliders[2];
	sliders[0] = getSlider(s, 0);
	sliders[1] = getSlider(s, 1);

	int divisor = grounded ? 1 : 2;
	int vel[3];
	vel[2] = (grounded && getButton(me, 0)) ? -192 : 0;
	range(i, 2) {
		int dx = bound(4*axis[i] - sliders[i], 10);
		uStateSlider(s, i, dx + sliders[i]);
		vel[i] = dx / divisor;
	}
	uVel(me, vel);

	// "shooting" and grabbing

	// (TODO)
}

void player_init() {
	regWhoMovesHandler("player-whomoves", player_whoMoves);
	regDrawHandler("player-draw", player_draw);
	regPushedHandler("player-pushed", player_pushed);
	regPushHandler("player-push", player_push);
	loadFile("modules/player.scm");
}
