#define PLAYER_CHARGE_SLIDER 8
#define PLAYER_COOLDOWN_SLIDER 9
#define PLAYER_EDIT_SLIDER 11
extern void getAxis(int32_t *dest, ent *e);
extern void getLook(int32_t *dest, ent *e);
extern void setAxis(ent *e, int32_t *x);
extern void setLook(ent *e, int32_t *x);
extern ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team);
extern void player_toggleBauble(gamestate *gs, ent *me, ent *target, int mode);
extern void player_clearBaubles(gamestate *gs, ent *me, int mode);
extern void player_flipBaubles(ent *me);
extern void module_player();
