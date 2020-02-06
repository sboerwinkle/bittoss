
static void *move_to_init() {
	loadFile("move-to/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC move_to_init
