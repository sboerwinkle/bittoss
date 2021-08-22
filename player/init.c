
/*
struct player_data {
	ent *floor;
};
*/

//static void player_onTick(ent *me) {
	//struct player_data* data = (struct player_data*)me->data;
	
	/*
	int32_t dv[2];// = {data->keys[1] - data->keys[0], data->keys[3] - data->keys[2]};
	int i;
	for (i = 0; i < 2; i++) {
		dv[i] = 50*PTS_PER_PX*me->ctrl.axis1.v[i]/axisMaxis/FRAMERATE - me->vel[i];
	}
	uVel(me, dv);
	*/
	/*
	if (data->floor) {
		int32_t desiredVel = 50*PTS_PER_PX/FRAMERATE*(data->keys[1] - data->keys[0]);
		int32_t a[2] = {desiredVel-me->vel[0], data->keys[2] ? -200*PTS_PER_PX/FRAMERATE : 0};
		accelRecursive(me, a);
	}
	data->floor = NULL;
	onTickGravity(me);
	*/
//}

/*
static void player_onPushed(ent *cd, ent *me, ent *leaf, ent *him) {
	if (cd->collisionAxis == 1 && cd->collisionDir == -1) {
		((struct player_data*)me->data)->floor = him;
	}
	onPushedDefault(cd, me, leaf, him);
}
*/

static pointer player_create_tmp(scheme *sc, pointer args) {
	int32_t r[3] = {16 * PTS_PER_PX,16 * PTS_PER_PX,16 * PTS_PER_PX};
	pointer ret = createHelper(sc, args, NULL, r, T_OBSTACLE, T_HEAVY | T_OBSTACLE | T_TERRAIN);
	/*
	if (is_c_ptr(ret, 0)) {
		((ent*)c_ptr_value(ret))->onTick = player_onTick;
	}
	*/
	return ret;
}

///////////////////////////////////////

static void *player_init() {
	loadFile("player/init.scm");
	scheme_define(sc, sc->global_env, mk_symbol(sc, "player-create-tmp"), mk_foreign_func(sc, player_create_tmp));
	return (void*)PREV_FUNC;
}

#undef PREV_FUNC
#define PREV_FUNC player_init
