#pragma once

#include <string.h>
#include "util.h"
#include "list.h"
#include "ent.h"

extern void init_registrar();
extern void destroy_registrar();

// There used to be some more complexity here, but it can still stay as a parameterized class
template <typename T> struct catalog {
	T* items;

	void init(int num);
	void destroy();

	void reg(int ix, T handler);
	T get(int ix);
	int reverseLookup(T handler);

	private:
	int num;
};

template <typename T>
void catalog<T>::init(int num) {
	this->num = num;
	items = new T[num];
	range(i, num) items[i] = NULL;
}

template <typename T>
void catalog<T>::destroy() {
	delete[] items;
}

template <typename T>
void catalog<T>::reg(int ix, T handler) {
	if (items[ix] != NULL) {
		fprintf(stderr, "handlerRegistrar: index %d already in use\n", ix);
		return;
	}

	items[ix] = handler;
}

template <typename T>
T catalog<T>::get(int ix) {
	if (ix < 0 || ix >= num) {
		fprintf(stderr, "ERROR - Invalid catalog index %d, there are only %d\n", ix, num);
		if (num) {
			return items[0];
		}
		fputs("ERROR - Also no fallback available, so I gotta crash, sorry\n", stderr);
		exit(1);
	}
	return items[ix];
}

template <typename T>
int catalog<T>::reverseLookup(T handler) {
	range(i, num) {
		if (items[i] == handler) return i;
	}
	fputs("WARN - Was asked to lookup nonexistent handler\n", stderr);
	return 0;
}



extern catalog<whoMoves_t> whoMovesHandlers;
extern catalog<tick_t> tickHandlers;
extern catalog<crush_t> crushHandlers;
extern catalog<pushed_t> pushedHandlers;
extern catalog<push_t> pushHandlers;
extern catalog<entPair_t> entPairHandlers;

enum {
	WHOMOVES_NIL,
	WHOMOVES_ME,
	WHOMOVES_EXPLOSION,
	WHOMOVES_PLATFORM, // Now defunct, just an alias for WHOMOVES_ME
	WHOMOVES_PLAYER,
	WHOMOVES_STACKEM,
	WHOMOVES_WOOD,
	WHOMOVES_METAL,
	WHOMOVES_NUM
};
// Did you include your module in "modules.c"?

enum {
	TICK_NIL,
	TICK_EXPLOSION,
	TICK_FLAG_SPAWNER,
	TICK_PLATFORM,
	TICK_BAUBLE,
	TICK_HELD_BAUBLE,
	TICK_HELD_THUMBTACK,
	TICK_PLAYER,
	TICK_EYE,
	TICK_HELD_EYE,
	TICK_gap1, // Unused
	TICK_LOGIC,
	TICK_LOGIC_DEBUG,
	TICK_DOOR,
	TICK_CURSED,
	TICK_RESPAWN,
	TICK_LEGG,
	TICK_SEAT,
	TICK_HELD_VEH_EYE,
	TICK_THUMBTACK,
	TICK_NUM
};
// Did you include your module in "modules.c"?

enum {
	CRUSH_NIL,
	CRUSH_FLAG,
	CRUSH_PLATFORM,
	CRUSH_NUM
};
// Did you include your module in "modules.c"?

enum {
	PUSHED_NIL,
	PUSHED_gap1, // Unused
	PUSHED_EXPLOSION,
	PUSHED_FLAG,
	PUSHED_THUMBTACK,
	PUSHED_PLAYER,
	PUSHED_gap2, // Unused
	PUSHED_LEGG,
	PUSHED_NUM
};
// Did you include your module in "modules.c"?

enum {
	PUSH_NIL,
	PUSH_PLAYER,
	PUSH_LOGIC,
	PUSH_SEAT,
	PUSH_NUM
};
// Did you include your module in "modules.c"?

enum {
	ENTPAIR_NIL,
	PICKUP_SEAT,
	FUMBLE_SEAT,
	ENTPAIR_NUM
};
// Did you include your module in "modules.c"?
