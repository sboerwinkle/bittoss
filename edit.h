extern void edit_info(ent *e);
extern int32_t edit_color(ent *e, const char *colorStr, char priviledged);
extern void edit_selectNearby(gamestate *gs, ent *e);
extern void edit_selectInside(gamestate *gs, ent *me);
extern void edit_selectWires(gamestate *gs, ent *me);
extern void edit_selectHeld(gamestate *gs, ent *me);
extern void edit_selectHeldRecursive(gamestate *gs, ent *me);
extern void edit_rm(gamestate *gs, ent *e);
extern void edit_m_weight(gamestate *gs, ent *me);
extern void edit_m_paper(gamestate *gs, ent *me);
extern void edit_m_wood(gamestate *gs, ent *me);
extern void edit_m_stone(gamestate *gs, ent *me);
extern void edit_m_wall(gamestate *gs, ent *me);
extern void edit_m_ghost(gamestate *gs, ent *me);
extern void edit_t_dumb(gamestate *gs, ent *me);
extern void edit_t_cursed(gamestate *gs, ent *me);
extern void edit_t_logic(gamestate *gs, ent *me);
extern void edit_t_logic_debug(gamestate *gs, ent *me);
extern void edit_t_door(gamestate *gs, ent *me);
extern void edit_slider(gamestate *gs, ent *me, const char *argsStr, char verbose);
extern void edit_create(gamestate *gs, ent *me, const char *argsStr, char verbose);
extern void edit_push(gamestate *gs, ent *me, const char *argsStr);
extern void edit_stretch(gamestate *gs, ent *me, const char *argsStr, char verbose);
extern void edit_copy(gamestate *gs, ent *me);
extern void edit_rotate(gamestate *gs, ent *me, char verbose);
extern void edit_flip(gamestate *gs, ent *me);
extern void edit_pickup(gamestate *gs, ent *me, const char *argsStr);
extern void edit_drop(gamestate *gs, ent *me);
extern void edit_wire(gamestate *gs, ent *me);
extern void edit_unwire(gamestate *gs, ent *me);
extern void edit_highlight(gamestate *gs, ent *me);
extern void edit_measure(gamestate *gs, ent *me);
extern void edit_import(gamestate *gs, ent *me, int32_t dist, list<char> *data);
extern void edit_export(gamestate *gs, ent *me, const char *name);
extern void edit_init();
extern void edit_destroy();
