// The number of active gamestrings and length of each string are each serialized in a byte (for now), so shouldn't exceed 255
#define GAMESTR_NUM 64
#define GAMESTR_LEN 200

extern char const *gamestrings[GAMESTR_NUM];

extern char const *gamestring_empty;

extern void gamestring_init();
extern void gamestring_destroy();

extern char const *gamestring_get(int ix);
extern void gamestring_set(gamestate *gs, int ix, char const *src);
extern void gamestring_reset();
