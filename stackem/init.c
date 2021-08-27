
static void *stackem_init() {
	loadFile("stackem/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC stackem_init
