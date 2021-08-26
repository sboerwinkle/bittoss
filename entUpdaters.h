extern void pushBtn1(ent *who);
extern void pushBtn2(ent *who);
extern void pushAxis1(ent *who, int *x);
extern void pushEyes(ent *who, int *x);
extern void uCenter(ent *e, int32_t *p);
extern void uVel(ent *e, int32_t *a);
extern void uDead(ent *e);
extern void uDrop(ent *e);
extern void uPickup(ent *e, ent *p);
extern entRef* uStateRef(entState *s, int ix, ent *e, int numSliders, int numRefs);
extern void uStateSlider(entState *s, int ix, int32_t value);
extern void uTypeMask(ent *e, uint32_t mask, char turnOn);
extern void uCollideMask(ent *e, uint32_t mask, char turnOn);

extern void registerTsUpdaters();
