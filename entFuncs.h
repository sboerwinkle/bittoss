
extern void onTickDefault(ent *me);

extern void onTickHeldDefault(ent *me);

extern int tickTypeDefault(ent *a, ent *b);

extern void onDrawDefault(ent *me, int layer);

extern void onCrushDefault(ent *me);

//default for onFree
extern void doNothing(ent *me);

//extern void onPushDefault(ent *cd, ent *me, ent *leaf, ent *him);

extern void onPushedDefault(ent *cd, ent *me, ent *leaf, ent *him);

/*
extern void onPushedFriction(ent *cd, ent *me, ent *leaf, ent *him);
*/

extern void onFumbleDefault(ent *me, ent *him);

extern void onFumbledDefault(ent *me);

extern void onPickUpDefault(ent *me, ent *him);

extern void onPickedUpDefault(ent *me, ent *him);

extern char okayFumbleDefault(ent *me);

extern char okayFumbleHimDefault(ent *me, ent *him);

extern ent *initEnt(const int32_t *c, const int32_t *v, const int32_t *r, int numSliders, int numRefs);

extern pointer createHelper(scheme *sc, pointer args, ent *parent, int32_t *r, int32_t typeMask, int32_t collideMask);

extern void setWhoMoves(ent *e, const char* text);

extern void registerTsFuncSetters();
