extern ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team);
extern void player_toggleBauble(gamestate *gs, ent *me, ent *target, int mode);
extern void player_clearBaubles(gamestate *gs, ent *me, int mode);
extern void player_flipBaubles(ent *me);
extern void module_player();
