
static int stackem_whoMoves(ent *a, ent *b, int axis, int dir) {
	int typ = type(b);
	if (typ & T_TERRAIN) {
		return MOVE_ME;
	}

	if (axis == 2) {
		if (dir == -1 && (typ & T_HEAVY)) return MOVE_ME;
		return MOVE_HIM;
	}

	if (TEAM_MASK & typ & type(a)) return MOVE_ME;
	return MOVE_HIM;
}

static int stackem_pushed(ent *me, ent *him, int axis, int dir, int dx, int dv) {
	if (axis == 2 && dir == -1) {
		int vel[3];
		getVel(vel, me, him);
		uStateSlider(&me->state, 0, bound(vel[0], 4));
		uStateSlider(&me->state, 1, bound(vel[1], 4));
	}
	// I'm using this more in the flag, which can actually be picked up.
	// Maybe I should change this?
	return r_move;
}

static void *stackem_init() {
	regWhoMovesHandler("stackem-whomoves", stackem_whoMoves);
	regPushedHandler("stackem-pushed", stackem_pushed);
	loadFile("stackem/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC stackem_init
