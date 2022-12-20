
static int platform_whoMoves(ent *a, ent *b, int axis, int dir) {
	return (type(b) & T_TERRAIN) ? MOVE_ME : MOVE_HIM;
}

static void *platform_init() {
	regWhoMovesHandler("platform-whomoves", platform_whoMoves);
	loadFile("platform/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC platform_init
