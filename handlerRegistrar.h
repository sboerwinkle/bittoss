#include <string.h>
#include "util.h"
#include "list.h"

extern void init_registrar();
extern void destroy_registrar();

struct card {
	const char *name;
	int ix;
	// Todo `list` supports some nice functionality here if we were to implement `le`...
};

template <typename T> struct catalog {
	list<T> items;
	list<card> cards;

	void init();
	void destroy();

	void reg(const char* name, T handler);
	T get(int ix);
	T getByName(const char* name);
	int reverseLookup(T handler);

private:
	int findCard(const char* name);
};

template <typename T>
void catalog<T>::init() {
	items.init();
	cards.init();
}

template <typename T>
void catalog<T>::destroy() {
	items.destroy();
	cards.destroy();
}

template <typename T>
void catalog<T>::reg(const char* name, T handler) {
	if (findCard(name) != -1) {
		fprintf(stderr, "handlerRegistrar: catalog name \"%s\" already in use\n", name);
		return;
	}

	card &newCard = cards.add();
	newCard.name = name;
	newCard.ix = items.num;
	items.add(handler);
}

template <typename T>
T catalog<T>::get(int ix) {
	if (ix < 0 || ix >= items.num) {
		fprintf(stderr, "ERROR - Invalid catalog index %d, there are only %d\n", ix, items.num);
		if (items.num) {
			return items[0];
		}
		fputs("ERROR - Also no fallback available, so I gotta crash, sorry\n", stderr);
		exit(1);
	}
	return items[ix];
}

template <typename T>
T catalog<T>::getByName(const char* name) {
	int cardIx = findCard(name);
	if (cardIx == -1) {
		fprintf(stderr, "Couldn't get catalog entry for name \"%s\"!\n", name);
		// This still might fail if there are no entries in the catalog,
		// but oh well I guess.
		// Alternatively we could just `exit` the whole thing and make them fix it...
		cardIx = 0;
	}
	return items[cards[cardIx].ix];
}

template <typename T>
int catalog<T>::reverseLookup(T handler) {
	range(i, items.num) {
		if (items[i] == handler) return i;
	}
	fputs("WARN - Was asked to lookup nonexistent handler\n", stderr);
	return 0;
}

template <typename T>
int catalog<T>::findCard(const char* name) {
	range(i, cards.num) {
		if (!strcmp(cards[i].name, name)) {
			return i;
		}
	}
	return -1;
}



extern catalog<whoMoves_t> whoMovesHandlers;
extern catalog<tick_t> tickHandlers;
extern catalog<draw_t> drawHandlers;
extern catalog<pushed_t> pushedHandlers;
extern catalog<push_t> pushHandlers;

