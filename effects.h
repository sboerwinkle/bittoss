extern void doGravity(gamestate *gs);
extern void doLava(gamestate *gs);
extern void doCrushtainer(gamestate *gs);
extern void doBigCrushtainer(gamestate *gs);
extern void doBouncetainer(gamestate *gs);
extern void createDebris(gamestate *gs);

#define EFFECT_CRUSH (1 << 0)
#define EFFECT_CRUSH_BIG (1 << 1)
#define EFFECT_BLOCKS (1 << 2)
#define EFFECT_GRAV (1 << 3)
#define EFFECT_LAVA (1 << 4)
#define EFFECT_BOUNCE (1 << 5)
#define EFFECT_SPAWN (1 << 6)
#define EFFECT_NO_BLOCK (1 << 7)
#define EFFECT_NO_PLATFORM (1 << 8)
#define RULE_EDIT (1 << 10)

extern char const * const rulesHelp[13];
