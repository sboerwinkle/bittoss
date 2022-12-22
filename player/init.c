
static int player_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_BOTH;
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

static void *player_init() {
	regWhoMovesHandler("player-whomoves", player_whoMoves);
	regPushedHandler("player-pushed", player_pushed);
	regPushHandler("player-push", player_push);
	loadFile("player/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC player_init
