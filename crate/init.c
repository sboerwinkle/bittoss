
static pointer crate_create_tmp(scheme *sc, pointer args) {
	return createHelper(sc, args, 16 * PTS_PER_PX, 16 * PTS_PER_PX, T_OBSTACLE | T_HEAVY, T_OBSTACLE | T_TERRAIN);
}

static void *crate_init() {
	loadFile("crate/init.scm");
	scheme_define(sc, sc->global_env, mk_symbol(sc, "crate-create-tmp"), mk_foreign_func(sc, crate_create_tmp));
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC crate_init
