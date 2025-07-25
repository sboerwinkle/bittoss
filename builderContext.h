#pragma once

#include "ent.h"

struct bctx_wire {
	int src, dest;
};

struct bctx_hold {
	int holder, holdee;
	int32_t holdFlags;
};

struct spaceFacts {
	int32_t pos[3];
	int32_t vel[3];
	int indices[3];
	int32_t signs[3];
};

struct builderContext {
	gamestate *gs;
	ent_blueprint bp;
	spaceFacts space;
	list<ent*> createdEnts;
	list<bctx_wire> wires;
	list<bctx_hold> holds;

	void init(gamestate *gs);
	void destroy();

	void center(int32_t x, int32_t y, int32_t z);
	void vel(int32_t x, int32_t y, int32_t z);
	void offsetCenter(int32_t const * input);
	void offsetVel(int32_t const * input);
	void radius(int32_t x, int32_t y, int32_t z);
	void btn(int ix);
	void resetHandlers();
	void sl(int num, ...);
	void heldBy(int holder, int32_t holdFlags);
	void wireTo(int dest);
	void build();
	void finish();
	void restart();

	// Eventually some stuff about transforms here, as well as probably some form of `copy`
	private:
	void withOffset(int32_t *dest, int32_t *offset, int32_t x, int32_t y, int32_t z);
	void adjOffset(int32_t *dest, int32_t *offset, const int32_t * input);
};
