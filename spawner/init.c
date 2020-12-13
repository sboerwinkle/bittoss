
static void *spawner_init() {
	loadFile("spawner/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC spawner_init
