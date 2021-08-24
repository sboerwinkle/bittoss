
static void *ground_init() {
	loadFile("ground/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC ground_init
