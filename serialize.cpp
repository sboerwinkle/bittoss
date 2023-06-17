#include <arpa/inet.h>
#include "util.h"
#include "list.h"
#include "ent.h"
#include "entFuncs.h"
#include "serialize.h"
#include "handlerRegistrar.h"

static const int32_t dummyArr[3] = {0, 0, 0};

// Thought about doing this as a function that accepts function pointers
// rather than as a big ol' whacky macro,
// but the problem is the generics system in C++ isn't flexible enough to let me
// define a pointer to a function that accepts an argument of any type T
// along with a catalog<T>. With macros I can do that as a transformation rather than
// a function pointer, and each instance will still be checked for type correctness.
#define processEnt(e) \
	range(i, e->state.numSliders) { \
		i32(e->state.sliders[i].v); \
	} \
	i32(e->typeMask); \
	i32(e->collideMask); \
	/* We're not handling any of the fields related to pending changes, */\
	/* so the serialization might not be totally faithful in the event */\
	/* that something doesn't get flushed all the way (is that possible?)*/\
	range(i, 3) i32(e->center[i]); \
	range(i, 3) i32(e->vel[i]); \
	range(i, 3) i32(e->radius[i]); \
	/* Don't remember right now if controls persist roll over between frames -*/\
	/* for now we're not going to bother.*/\
	handler(e->whoMoves, whoMovesHandlers); \
	handler(e->tick, tickHandlers); \
	handler(e->tickHeld, tickHandlers); \
	handler(e->draw, drawHandlers); \
	handler(e->push, pushHandlers); \
	handler(e->pushed, pushedHandlers)

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

static void serializeEnt(ent *e, int ix, list<char> *data) {
	e->clone.ix = ix;
	write32(data, e->state.numSliders);
#define i32(x) write32(data, x)
#define handler(x, y) write32(data, y.reverseLookup(x))
	processEnt(e);
#undef handler
#undef i32
	writeEntRef(data, e->holder);
}

static int serializeSiblings(ent *e, int ix, list<char> *data) {
	for (; e; e = e->LL.n) {
		serializeEnt(e, ix, data);
		ix++;
		ix = serializeSiblings(e->holdee, ix, data);
	}
	return ix;
}

void serialize(gamestate *gs, list<char> *data) {
	int countIx = data->num;
	// This space intentionally left blank; we populate the first 4 bytes (which is
	// the number of entities) after we've iterated through them.
	data->num += 4;
	int num = serializeSiblings(gs->rootEnts, 0, data);
	write32Raw(data, countIx, num);

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

static ent* deserializeEnt(gamestate *gs, ent** ents, const list<char> *data, int *ix) {
	int32_t numSliders = read32(data, ix);
	ent *e = initEnt(gs, dummyArr, dummyArr, dummyArr, numSliders, 0, 0, 0);
#define i32(x) x = read32(data, ix)
#define handler(x, y) x = y.get(read32(data, ix))
	processEnt(e);
#undef i32
#undef handler
	int holder = read32(data, ix);
	if (holder != -1) {
		pickupNoHandlers(gs, ents[holder], e);
	}
	return e;
}

void deserialize(gamestate *gs, const list<char> *data) {
	int _ix = 0; // Used to step through the inbound data
	int *ix = &_ix;
	int32_t numEnts = read32(data, ix);
	ent** ents = new ent*[numEnts];
	range(i, numEnts) {
		ents[i] = deserializeEnt(gs, ents, data, ix);
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