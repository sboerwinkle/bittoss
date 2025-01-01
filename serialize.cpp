#include <arpa/inet.h>
#include "util.h"
#include "list.h"
#include "ent.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "entFuncs.h"
#include "gamestring.h"
#include "serialize.h"
#include "handlerRegistrar.h"

static const char* version_string = "gam6";
#define VERSION 6

static int version; // TODO global state is bad, use a context object during deserialization

static const int32_t dummyPos[3] = {0, 0, 0};
static const int32_t dummyR[3] = {1<<27, 1<<27, 1<<27};
// I didn't make that number up I swear. It's a real number with real reasons.
// Basically it's so when we're doing everything totally wrong at first
// (before we fix it with hacks, because that's better than *totally* ignoring standard ent creation flow)
// we don't waste too much energy on setting up a real deep box heirarchy.

// Thought about doing this as a function that accepts function pointers
// rather than as a big ol' whacky macro,
// but the problem is the generics system in C++ isn't flexible enough to let me
// define a pointer to a function that accepts an argument of any type T
// along with a catalog<T>. With macros I can do that as a transformation rather than
// a function pointer, and each instance will still be checked for type correctness.
#define processEnt(e) \
	/* We're not handling any of the fields related to pending changes, */\
	/* so the serialization might not be totally faithful in the event */\
	/* that something doesn't get flushed all the way (is that possible?)*/\
	handler(e->whoMoves, whoMovesHandlers, 0); \
	handler(e->tick, tickHandlers, 1); \
	handler(e->tickHeld, tickHandlers, 2); \
	handler(e->crush, crushHandlers, 3); \
	handler(e->push, pushHandlers, 4); \
	handler(e->pushed, pushedHandlers, 5); \
	handler(e->onPickUp, entPairHandlers, 6); \
	handler(e->onPickedUp, entPairHandlers, 7); \
	handler(e->onFumble, entPairHandlers, 8); \
	handler(e->onFumbled, entPairHandlers, 9); \
	check(133)

static void write32Raw(list<char> *data, int offset, int32_t v) {
	*(int32_t*)(data->items + offset) = htonl(v);
}

void write32(list<char> *data, int32_t v) {
	int n = data->num;
	data->setMaxUp(n + 4);
	write32Raw(data, n, v);
	data->num = n + 4;
}

int reserve32(list<char> *data) {
	int n = data->num;
	data->setMaxUp(n + 4);
	data->num = n + 4;
	return n;
}

static void writeEntRef(list<char> *data, ent *e) {
	write32(data, e ? e->clone.ix : -1);
}

static void writeStr(list<char> *data, const char *str) {
	int len = strlen(str);
	if (len > 255) len = 255;
	data->add((unsigned char) len);
	int end = data->num + len;
	data->setMaxUp(end);
	memcpy(data->items + data->num, str, len);
	data->num = end;
}

static int enumerateAll(ent *e) {
	int ix = 0;
	for (; e; e = e->ll.n) {
		e->clone.ix = ix++;
	}
	return ix;
}

static int enumerateSelected(ent *e) {
	int ix = 0;
	for (; e; e = e->ll.n) {
		if (e->clone.ix != -1) e->clone.ix = ix++;
	}
	return ix;
}

static char buttonByte(ent *e) {
	char ret = 0;
	range(i, 4) {
		if (getButton(e, i)) ret |= 1 << i;
	}
	return ret;
}

template <typename T>
static int32_t writeHandler(list<char> *data, T x, catalog<T> *y, int bit) {
	if (x == y->get(0)) return 0;
	write32(data, y->reverseLookup(x));
	return (int32_t)1 << bit;
}

static void serializeEnt(ent *e, list<char> *data, const int32_t *c_offset, const int32_t *v_offset) {
#define check(x) data->add(x)
#define i32(x) write32(data, x)
#define handler(x, y, i) handlersBitfield |= writeHandler(data, x, &y, i)
#define wire(w) write32(data, w->clone.ix)
	check(128);
	range(i, 3) i32(e->center[i] - c_offset[i]);
	range(i, 3) i32(e->vel[i] - v_offset[i]);
	range(i, 3) i32(e->radius[i]);
	check(129);
	i32(e->numSliders);
	i32(e->typeMask);
	i32(e->collideMask);

	int numWires = 0;
	range(i, e->wires.num) if (e->wires[i]->clone.ix != -1) numWires++;
	i32(numWires);

	check(130);
	i32(e->color);
	data->add(buttonByte(e));
	range(i, e->numSliders) i32(e->sliders[i].v);
	check(131);
	range(i, e->wires.num) {
		int ix = e->wires[i]->clone.ix;
		if (ix != -1) i32(ix);
	}

	check(132);

	int handlersIx = reserve32(data);
	int32_t handlersBitfield = 0;
	processEnt(e);
	if (e->friction != DEFAULT_FRICTION) {
		handlersBitfield |= (1<<31);
		i32(e->friction);
	}
	write32Raw(data, handlersIx, handlersBitfield);

	writeEntRef(data, e->holder);
	i32(e->holdFlags);
#undef wire
#undef handler
#undef i32
#undef check
}

static void serializeEnts(ent *e, list<char> *data, const int32_t *c_offset, const int32_t *v_offset) {
	for (; e; e = e->ll.n) {
		if (e->clone.ix != -1) serializeEnt(e, data, c_offset, v_offset);
	}
}

void writeHeader(list<char> *data) {
	data->setMaxUp(data->num + 4);
	memcpy(&(*data)[data->num], version_string, 4);
	data->num += 4;
}

static void serializeGamestrings(list<char> *data) {
	int count = 0;
	range(i, GAMESTR_NUM) {
		if (*gamestrings[i]) count = i+1;
	}
	data->add(count);
	range(i, count) {
		writeStr(data, gamestrings[i]);
	}
}

void serialize(gamestate *gs, list<char> *data) {
	writeHeader(data);

	// Count of entities
	write32(data, enumerateAll(gs->ents));

	serializeEnts(gs->ents, data, zeroVec, zeroVec);

	int numPlayers = gs->players.num;
	write32(data, numPlayers);
	range(i, numPlayers) {
		player *p = &gs->players[i];
		write32(data, p->color);
		write32(data, p->reviveCounter);
		write32(data, p->data);
		writeStr(data, p->name);
		writeEntRef(data, p->entity);
	}

	write32(data, gs->rand);
	write32(data, gs->gamerules);
	serializeGamestrings(data);
}

// Same format as a proper savegame, but we only include some ents, and never any players or global state
void serializeSelected(gamestate *gs, list<char> *data, const int32_t *c_offset, const int32_t *v_offset) {
	writeHeader(data);

	// Count of entities
	write32(data, enumerateSelected(gs->ents));

	serializeEnts(gs->ents, data, c_offset, v_offset);

	write32(data, 0); // Number of players (0)
	// This is ignored in normal use. However, if we load this as a level, we don't want a 0 here.
	write32(data, gs->rand);
	write32(data, 0); // Game rules (none)
	data->add(0); // Strings (none)
}

static char read8(const list<const char> *data, int *ix) {
	int i = *ix;
	if (i >= data->num) return 0;
	*ix = i+1;
	return (*data)[i];
}

static int32_t read32(const list<const char> *data, int *ix) {
	int i = *ix;
	if (i + 4 > data->num) return 0;
	*ix = i + 4;
	return ntohl(*(int32_t*)(data->items + i));
}

static void readStr(const list<const char> *data, int *ix, char *dest, int limit) {
	int reportedLen = (unsigned char) read8(data, ix);
	int i = *ix;
	int avail = data->num - i;
	if (avail < reportedLen || limit - 1 < reportedLen) {
		// Don't mess around even trying to read this.
		// `avail` could be negative, e.g.
		*ix = data->num;
		*dest = '\0';
		return;
	}

	memcpy(dest, data->items + i, reportedLen);
	dest[reportedLen] = '\0';
	*ix = i + reportedLen;
}

static void checksum(const list<const char> *data, int *ix, int expected) {
	char x = (*data)[(*ix)++];
	if (x != (char) expected) {
		fprintf(stderr, "Expected %d, got %hhu\n", expected, x);
	}
}

static void handleButtons(const list<const char> *data, int *ix, ent *e) {
	if (version == 0) return; // We didn't encode active buttons in version 0, since logic wasn't a thing then!
	char c = read8(data, ix);
	range(i, 4) {
		if (c & (1 << i)) pushBtn(e, i);
	}
}

static void deserializeEnt(gamestate *gs, ent** ents, ent *e, const list<const char> *data, int *ix, const int32_t *c_offset, const int32_t *v_offset) {
	int32_t c[3], v[3], r[3];

#define check(x) checksum(data, ix, x)
#define i32(x) x = read32(data, ix)
#define handler(x, y, i) if (handlersBitfield & (1 << i)) x = y.get(read32(data, ix))
#define wire(x) x = ents[read32(data, ix)]
	check(128);
	range(i, 3) c[i] = read32(data, ix) + c_offset[i];
	range(i, 3) v[i] = read32(data, ix) + v_offset[i];
	range(i, 3) i32(r[i]);
	check(129);
	// Order in which arguments are evaluated isn't defined, so this has to be explicitly ordered
	// I bet you can guess that I learned that the hard way
	int32_t numSliders = read32(data, ix);
	int32_t typeMask = read32(data, ix);
	int32_t collideMask = read32(data, ix);
	initEnt(e, gs, NULL, c, v, r, numSliders, typeMask, collideMask);

	int32_t numWires;
	i32(numWires);
	e->wires.setMaxUp(numWires);
	e->wires.num = numWires;

	check(130);
	i32(e->color);
	handleButtons(data, ix, e);
	range(i, e->numSliders) i32(e->sliders[i].v);
	check(131);
	// TODO unchecked access here, provided ent index could be bogus
	range(i, numWires) wire(e->wires[i]);

	check(132);
	int32_t handlersBitfield = (version >= 4) ? read32(data, ix) : 63;
	processEnt(e);
	if (handlersBitfield & (1<<31)) {
		i32(e->friction);
	}
#undef wire
#undef i32
#undef handler
#undef check
	int holder = read32(data, ix);
	int holdFlags = version >= 2 ? read32(data, ix) : 0;
	if (holder != -1) {
		pickupNoHandlers(gs, ents[holder], e, holdFlags);
	}
}

int verifyHeader(const list<const char> *data, int *ix) {
	if (data->num < 4) {
		fprintf(stderr, "Only got %d bytes of data, can't deserialize\n", data->num);
		return -1;
	}
	// Only compare 3 bytes, not 4, since the last one we use as a version
	if (strncmp(data->items, version_string, 3)) {
		fprintf(
			stderr,
			"Beginning of serialized data should read \"%s\", actually reads \"%c%c%c%c\"\n",
			version_string,
			(*data)[0], (*data)[1], (*data)[2], (*data)[3]
		);
		return -1;
	}
	// `version` is a global, we should probably not be doing that but oh well
	version = data->items[3] - '0';
	if (version < 0 || version > VERSION) {
		fprintf(
			stderr,
			"Version number should be between 0 and %d, but found %d\n",
			VERSION, version
		);
		return -1;
	}
	*ix = 4;
	int32_t numEnts = read32(data, ix);
	if (data->num < numEnts * 60) {
		// size multipler here isn't exact,
		// and it doesn't have to be since we're careful when reading data out of that list.
		// We just want a sanity check so we don't allocate 0x80808080 entities...
		fputs("Not enough data in file to deserialize!\n", stderr);
		return -1;
	}
	return numEnts;
}

static void deserializeGamestrings(gamestate *gs, const list<const char> *data, int *ix) {
	if (version < 6) return;

	unsigned char count = read8(data, ix);
	char buffer[GAMESTR_LEN];
	range(i, count) {
		readStr(data, ix, buffer, GAMESTR_LEN);
		gamestring_set(gs, i, buffer);
	}
}

void deserialize(gamestate *gs, const list<const char> *data, char fullState) {
	int _ix;
	int *ix = &_ix;
	int numEnts = verifyHeader(data, ix);
	if (numEnts == -1) return;

	ent** ents = new ent*[numEnts];
	range(i, numEnts) {
		// A bunch of ent-sized spaces in memory.
		// We fill them in later, but it's helpful to have a valid pointer
		// so we can set stuff up to point to the ent before it's initialized.
		ents[i] = (ent*) calloc(1, sizeof(ent));
	}
	range(i, numEnts) {
		deserializeEnt(gs, ents, ents[i], data, ix, zeroVec, zeroVec);
	}

	int serializedPlayers = read32(data, ix);
	player dummy;
	range(i, serializedPlayers) {
		player *p;
		if (i < gs->players.num) p = &gs->players[i];
		else p = &dummy;

		int32_t color = read32(data, ix);
		p->reviveCounter = read32(data, ix);
		int32_t playerData = version >= 3 ? read32(data, ix) : 0;
		char name[NAME_BUF_LEN];
		if (version >= 5) {
			readStr(data, ix, name, NAME_BUF_LEN);
		} else {
			name[0] = '\0';
		}

		if (fullState) {
			p->color = color;
			p->data = playerData;
			strcpy(p->name, name);
		}

		int e_ix = read32(data, ix);
		if (e_ix == -1 || e_ix >= numEnts) p->entity = NULL;
		else p->entity = ents[e_ix];
	}

	int32_t rand = read32(data, ix);
	if (fullState) {
		gs->rand = rand;
	}
	gs->gamerules = read32(data, ix);
	deserializeGamestrings(gs, data, ix);

	delete[] ents;
}

void deserializeSelected(gamestate *gs, const list<const char> *data, const int32_t *c_offset, const int32_t *v_offset) {
	int _ix;
	int *ix = &_ix;
	int numEnts = verifyHeader(data, ix);
	if (numEnts == -1) return;

	ent** ents = new ent*[numEnts];
	range(i, numEnts) {
		ents[i] = (ent*) calloc(1, sizeof(ent));
	}
	range(i, numEnts) {
		deserializeEnt(gs, ents, ents[i], data, ix, c_offset, v_offset);
	}

	delete[] ents;
}
