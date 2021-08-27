
static void *player_init() {
	loadFile("player/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC player_init
