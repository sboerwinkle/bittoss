#define DEFAULT_FRICTION 4

extern int tickTypeDefault(ent *a, ent *b);

extern char okayFumbleDefault(ent *me);

extern char okayFumbleHimDefault(ent *me, ent *him);

extern void prepNewSliders(ent *e);

extern ent *initEnt(
	gamestate *gs,
	ent *relative,
	const int32_t *c,
	const int32_t *v,
	const int32_t *r,
	int numSliders,
	int32_t typeMask,
	int32_t collideMask
);

extern void initEnt(
	ent *e,
	gamestate *gs,
	ent *relative,
	const int32_t *c,
	const int32_t *v,
	const int32_t *r,
	int numSliders,
	int32_t typeMask,
	int32_t collideMask
);

extern void setWhoMoves(ent *e, const char* text);

extern void init_entFuncs();
