extern void doGravity(gamestate *gs);
extern void doLava(gamestate *gs);
extern void doCrushtainer(gamestate *gs);
extern void doBigCrushtainer(gamestate *gs);
extern void createDebris(gamestate *gs);

#define EFFECT_CRUSH (1 << 0)
#define EFFECT_CRUSH_BIG (1 << 1)
#define EFFECT_BLOCKS (1 << 2)
#define EFFECT_GRAV (1 << 3)
#define EFFECT_LAVA (1 << 4)

#define RULE_HELP_STR "Rules available are:\n"\
	"0 - enforce boundary\n"\
	"1 - enforce boundary at double distance\n"\
	"2 - spawn incoming blocks\n"\
	"3 - gravity\n"\
	"4 - enforce lower boundary\n"
