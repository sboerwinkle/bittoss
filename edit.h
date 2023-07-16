extern void edit_info(ent *e);
extern int32_t edit_color(ent *e, const char *colorStr);
extern void edit_wireNearby(gamestate *gs, ent *e);
extern void edit_rm(gamestate *gs, ent *e);
extern void edit_create(gamestate *gs, ent *me, const char *argsStr);
extern void edit_init();
extern void edit_destroy();
