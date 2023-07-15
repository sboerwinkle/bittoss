#include <arpa/inet.h>
#include "util.h"
#include "list.h"
#include "ent.h"
#include "entFuncs.h"
#include "serialize.h"
#include "handlerRegistrar.h"

static const char* version_string = "gam0";

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
	i32(e->color); \
	range(i, e->state.numSliders) { \
		i32(e->state.sliders[i].v); \
	} \
	check(131); \
	range(i, e->wires.num) { \
		wire(e->wires[i]); \
	} \
	check(132); \
	/* Don't remember right now if controls roll over between frames -*/\
	/* for now we're not going to bother.*/\
	handler(e->whoMoves, whoMovesHandlers); \
	handler(e->tick, tickHandlers); \
	handler(e->tickHeld, tickHandlers); \
	handler(e->crush, crushHandlers); \
	handler(e->push, pushHandlers); \
	handler(e->pushed, pushedHandlers); \
	check(133)

static void write32Raw(list<char> *data, int offset, int32_t v) {
	*(int32_t*)(data->items + offset) = htonl(v);
}

static void write32(list<char> *data, int32_t v) {
	int n = data->num;
	data->setMaxUp(n + 4);
	write32Raw(data, n, v);
	data->num = n + 4;
}

static void writeEntRef(list<char> *data, ent *e) {
	write32(data, e ? e->clone.ix : -1);
}

static int enumerateSiblings(ent *e, int ix) {
	for (; e; e = e->LL.n) {
		e->clone.ix = ix++;
		ix = enumerateSiblings(e->holdee, ix);
	}
	return ix;
}

static void serializeEnt(ent *e, list<char> *data) {
#define check(x) data->add(x)
#define i32(x) write32(data, x)
#define handler(x, y) write32(data, y.reverseLookup(x))
#define wire(w) write32(data, w->clone.ix)
	check(128);
	range(i, 3) i32(e->center[i]);
	range(i, 3) i32(e->vel[i]);
	range(i, 3) i32(e->radius[i]);
	check(129);
	i32(e->state.numSliders);
	i32(e->typeMask);
	i32(e->collideMask);
	i32(e->wires.num);
	check(130);
	processEnt(e);
#undef wire
#undef handler
#undef i32
#undef check
	writeEntRef(data, e->holder);
}

static void serializeSiblings(ent *e, list<char> *data) {
	for (; e; e = e->LL.n) {
		serializeEnt(e, data);
		serializeSiblings(e->holdee, data);
	}
}

void serialize(gamestate *gs, list<char> *data) {
	// First 8 bytes are 'gam0', followed by the count of entities (which we'll populate later)
	data->setMaxUp(data->num + 8);
	memcpy(&(*data)[data->num], version_string, 4);
	int countIx = data->num + 4;
	data->num += 8;

	int num = enumerateSiblings(gs->rootEnts, 0);
	write32Raw(data, countIx, num);
	serializeSiblings(gs->rootEnts, data);

	int numPlayers = gs->players->num;
	write32(data, numPlayers);
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		write32(data, p->color);
		write32(data, p->reviveCounter);
		writeEntRef(data, p->entity);
	}

	write32(data, gs->rand);
}

static int32_t read32(const list<char> *data, int *ix) {
	int i = *ix;
	if (i + 4 > data->num) return 0;
	*ix = i + 4;
	return ntohl(*(int32_t*)(data->items + i));
}

static void checksum(const list<char> *data, int *ix, int expected) {
	char x = (*data)[(*ix)++];
	if (x != (char) expected) {
		fprintf(stderr, "Expected %d, got %hhu\n", expected, x);
	}
}

static void deserializeEnt(gamestate *gs, ent** ents, ent *e, const list<char> *data, int *ix) {
	int32_t c[3], v[3], r[3];

#define check(x) checksum(data, ix, x)
#define i32(x) x = read32(data, ix)
#define handler(x, y) x = y.get(read32(data, ix))
#define wire(x) x = ents[read32(data, ix)]
	check(128);
	range(i, 3) i32(c[i]);
	range(i, 3) i32(v[i]);
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
	processEnt(e);
#undef wire
#undef i32
#undef handler
#undef check
	int holder = read32(data, ix);
	if (holder != -1) {
		pickupNoHandlers(gs, ents[holder], e);
	}
}

void deserialize(gamestate *gs, const list<char> *data) {
	if (data->num < 4) {
		fputs("Only got %d bytes of data, can't deserialize\n", stderr);
		return;
	}
	if (strncmp(data->items, version_string, 4)) {
		fprintf(
			stderr,
			"Beginning of serialized data should read \"%s\", actually reads \"%c%c%c%c\"\n",
			version_string,
			(*data)[0], (*data)[1], (*data)[2], (*data)[3]
		);
		return;
	}
	int _ix = 4; // Used to step through the inbound data
	int *ix = &_ix;
	int32_t numEnts = read32(data, ix);
	if (data->num < numEnts * 80) {
		// 80 isn't exact, and it doesn't have to be since we're careful when reading data out of that list.
		// We just want a sanity check so we don't allocate 0x80808080 entities...
		fputs("Not enough data in file to deserialize!\n", stderr);
		return;
	}
	ent** ents = new ent*[numEnts];
	range(i, numEnts) {
		// A bunch of ent-sized spaces in memory.
		// We fill them in later, but it's helpful to have a valid pointer
		// so we can set stuff up to point to the ent before it's initialized.
		ents[i] = (ent*) calloc(1, sizeof(ent));
	}
	range(i, numEnts) {
		deserializeEnt(gs, ents, ents[i], data, ix);
	}

	int serializedPlayers = read32(data, ix);
	player dummy;
	range(i, serializedPlayers) {
		player *p;
		if (i < gs->players->num) p = &(*gs->players)[i];
		else p = &dummy;

		p->color = read32(data, ix);
		p->reviveCounter = read32(data, ix);

		int e_ix = read32(data, ix);
		if (e_ix == -1) p->entity = NULL;
		else p->entity = ents[e_ix];
	}

	gs->rand = read32(data, ix);

	delete[] ents;
}
