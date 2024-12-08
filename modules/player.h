#define PLAYER_CHARGE_SLIDER 8
#define PLAYER_COOLDOWN_SLIDER 9
#define PLAYER_EDIT_SLIDER 11

#define PLAYER_SPD 256

#define PLAYER_BTN_ALTFIRE1 1
#define PLAYER_BTN_ALTFIRE2 2

extern void getAxis(int32_t *dest, ent *e);
extern void getLook(int32_t *dest, ent *e);
extern void player_setAxis(ent *e, int32_t *x);
extern void player_setLook(ent *e, int32_t *x);
extern void player_setButtons(ent *e, int32_t x);
extern ent* mkPlayer(gamestate *gs, int32_t *pos, int32_t team, int32_t width, int32_t height);
extern void player_toggleBauble(gamestate *gs, ent *me, ent *target, int mode);
extern void player_clearBaubles(gamestate *gs, ent *me, int mode);
extern void player_flipBaubles(ent *me);
extern void module_player();
