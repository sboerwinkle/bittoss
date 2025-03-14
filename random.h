
typedef int32_t rand_t;

extern const rand_t randomMax;
extern void stepRand(rand_t* x);
extern uint32_t splitmix32(uint32_t *state);
