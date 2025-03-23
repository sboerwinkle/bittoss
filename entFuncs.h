#define DEFAULT_FRICTION 16

extern int tickTypeDefault(ent *a, ent *b);

extern void prepNewSliders(ent *e);

extern void clearBlueprintHandlers(ent_blueprint *e);
extern void initEntBlueprint(ent_blueprint *e);
extern void destroyEntBlueprint(ent_blueprint *e);
extern void copyEntBlueprint(ent_blueprint *dest, ent_blueprint const *src);

extern void initBlueprintedEnt (
	ent *e,
	gamestate *gs,
	ent *relative
);

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

extern void init_entFuncs();
