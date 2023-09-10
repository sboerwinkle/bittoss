
extern ent* mkFactoryWires(gamestate *gs, ent *h);
extern void module_factory();

enum {
	FACT_VERSION, // Unused for now, may be handy if we don't want to invalidate all saved factories next time we make a change
	FACT_VEL,
	FACT_VEL1,
	FACT_VEL2,
	FACT_TYPE,
	FACT_COLLIDE,
	FACT_COLOR,
	FACT_MISC, // Buttons, plus "is held"
	FACT_WHOMOVES, // all the handlers...
	FACT_TICK,
	FACT_TICKHELD,
	FACT_CRUSH,
	FACT_PUSH,
	FACT_PUSHED,
	FACT_FUMBLE,
	FACT_FUMBLED,
	FACT_PICKUP,
	FACT_PICKEDUP,
	FACT_SLIDERS // and then any sliders go at the end
};

#define FACT_MISC_HOLD (1<<31)
