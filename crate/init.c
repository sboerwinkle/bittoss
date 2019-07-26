
static void *crate_init() {
	loadFile("crate/init.scm");
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC crate_init
