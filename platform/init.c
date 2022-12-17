
static void *platform_init() {
	loadFile("platform/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC platform_init
