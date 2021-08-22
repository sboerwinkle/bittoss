
static void *growth_init() {
	loadFile("growth/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC growth_init
