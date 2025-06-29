#include <stdio.h>
#include <string.h>

#include "util.h"
#include "main.h"
#include "gamestring.h"

// This is "exported" as gamestrings, but in this file we just call it "strings".
char const *gamestrings[GAMESTR_NUM];
#define strings gamestrings

char const *gamestring_empty = "";
#define empty gamestring_empty

void gamestring_init() {
	range(i, GAMESTR_NUM) strings[i] = empty;
}

void gamestring_destroy() {
	gamestring_reset();
}

char const *gamestring_get(int ix) {
	if (ix < 0 || ix >= GAMESTR_NUM) {
		fprintf(stderr, "gamestring_get: bad index %d\n", ix);
		return empty;
	}
	return strings[ix];
}

void gamestring_set(gamestate *gs, int ix, char const *src) {
	if (gs != rootState) return;

	if (ix < 0 || ix >= GAMESTR_NUM) {
		fprintf(stderr, "gamestring_set: bad index %d\n", ix);
		return;
	}

	int l = strlen(src);
	if (l >= GAMESTR_LEN) {
		fprintf(stderr, "gamestring_set: provided str is too long (%d bytes) (starts with '%.10s')\n", l, src);
	}

	// At this point we know we're setting something, clean out any old value
	if (strings[ix] != empty) free((char*)strings[ix]);

	if (!l) {
		strings[ix] = empty;
	} else {
		char *dest = (char*)malloc(l+1);
		memcpy(dest, src, l);
		dest[l] = '\0';
		strings[ix] = dest;
	}
}

void gamestring_reset() {
	range(i, GAMESTR_NUM) {
		if (strings[i] != empty) {
			// Explicitly remove `const` if we know it's not our special `empty` string
			free((char*)strings[i]);
			strings[i] = empty;
		}
	}
}
