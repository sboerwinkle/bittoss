extern void doGravity(gamestate *gs);
extern void doLava(gamestate *gs);
extern void doCrushtainer(gamestate *gs);
extern void doBigCrushtainer(gamestate *gs);
extern void createDebris(gamestate *gs);

#define EFFECT_CRUSH 1
#define EFFECT_CRUSH_BIG 2
#define EFFECT_BLOCKS 4
#define EFFECT_GRAV 8
#define EFFECT_LAVA 16
