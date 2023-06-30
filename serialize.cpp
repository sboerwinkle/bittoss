#include <arpa/inet.h>
#include "util.h"
#include "list.h"
#include "ent.h"
#include "entFuncs.h"
#include "serialize.h"
#include "handlerRegistrar.h"

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
	range(i, e->state.numSliders) { \
		i32(e->state.sliders[i].v); \
	} \
	range(i, e->wires.num) { \
		wire(e->wires[i]); \
	} \
	/* Don't remember right now if controls roll over between frames -*/\
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

static int enumerateSiblings(ent *e, int ix) {
	for (; e; e = e->LL.n) {
		e->clone.ix = ix++;
		ix = enumerateSiblings(e->holdee, ix);
	}
	return ix;
}

static void serializeEnt(ent *e, list<char> *data) {
#define i32(x) write32(data, x)
#define handler(x, y) write32(data, y.reverseLookup(x))
#define wire(w) write32(data, w->clone.ix)
	range(i, 3) i32(e->center[i]);
	range(i, 3) i32(e->vel[i]);
	range(i, 3) i32(e->radius[i]);
	i32(e->state.numSliders);
	i32(e->typeMask);
	i32(e->collideMask);
	i32(e->wires.num);
	processEnt(e);
#undef wire
#undef handler
#undef i32
	writeEntRef(data, e->holder);
}

static void serializeSiblings(ent *e, list<char> *data) {
	for (; e; e = e->LL.n) {
		serializeEnt(e, data);
		serializeSiblings(e->holdee, data);
	}
}

void serialize(gamestate *gs, list<char> *data) {
	int countIx = data->num;
	// This space intentionally left blank; we populate the first 4 bytes (which is
	// the number of entities) after we've iterated through them.
	data->num += 4;
	int num = enumerateSiblings(gs->rootEnts, 0);
	serializeSiblings(gs->rootEnts, data);
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

static void deserializeEnt(gamestate *gs, ent** ents, ent *e, const list<char> *data, int *ix) {
	int32_t c[3], v[3], r[3];

#define i32(x) x = read32(data, ix)
#define handler(x, y) x = y.get(read32(data, ix))
#define wire(x) x = ents[read32(data, ix)]
	range(i, 3) i32(c[i]);
	range(i, 3) i32(v[i]);
	range(i, 3) i32(r[i]);
	initEnt(e, gs, NULL, c, v, r, read32(data, ix), read32(data, ix), read32(data, ix));

	int32_t numWires;
	i32(numWires);
	e->wires.setMaxUp(numWires);
	e->wires.num = numWires;

	processEnt(e);
#undef wire
#undef i32
#undef handler
	int holder = read32(data, ix);
	if (holder != -1) {
		pickupNoHandlers(gs, ents[holder], e);
	}
}

void deserialize(gamestate *gs, const list<char> *data) {
	int _ix = 0; // Used to step through the inbound data
	int *ix = &_ix;
	int32_t numEnts = read32(data, ix);
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