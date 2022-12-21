
static int flag_whoMoves(ent *a, ent *b, int axis, int dir) {
	return MOVE_ME;
}

static void *flag_init() {
	regWhoMovesHandler("flag-whomoves", flag_whoMoves);
	loadFile("flag/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC flag_init
