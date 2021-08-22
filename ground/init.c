
static pointer ground_create_tmp(scheme *sc, pointer args) {
	int32_t r[3] = {16 * PTS_PER_PX, 16 * PTS_PER_PX, 16 * PTS_PER_PX};
	return createHelper(sc, args, NULL, r, T_TERRAIN | T_HEAVY | T_WEIGHTLESS, 0);
}

static void *ground_init() {
	loadFile("ground/init.scm");
	scheme_define(sc, sc->global_env, mk_symbol(sc, "ground-create-tmp"), mk_foreign_func(sc, ground_create_tmp));
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC ground_init
