#include <stdarg.h>

#include "util.h"

#include "builderContext.h"
#include "entFuncs.h"

void builderContext::init(gamestate *_gs) {
	gs = _gs;
	range(i, 3) {
		space.pos[i] = space.vel[i] = 0;
	}
	initEntBlueprint(&bp);
	createdEnts.init();
	wires.init();
	holds.init();
	// Initialize transform to the identity
	range(i, 3) {
		space.indices[i] = i;
		space.signs[i] = 1;
	}
}

void builderContext::destroy() {
	destroyEntBlueprint(&bp);
	createdEnts.destroy();
	wires.destroy();
	holds.destroy();
}

void builderContext::withOffset(int32_t *dest, int32_t *offset, int32_t const x, int32_t const y, int32_t const z) {
	int32_t const input[3] = {x, y, z};
	range(i, 3) {
		int j = space.indices[i];
		int32_t flip = space.signs[i]; // `i` or `j`? IDK, figure this out later
		dest[j] = flip*input[i] + offset[j];
	}
}

void builderContext::adjOffset(int32_t *dest, int32_t *offset, int32_t const * input) {
	range(i, 3) {
		int j = space.indices[i];
		int32_t flip = space.signs[i];
		dest[j] += flip*input[i];
		offset[j] += flip*input[i];
	}
}

void builderContext::center(int32_t x, int32_t y, int32_t z) {
	withOffset(bp.center, space.pos, x, y, z);
}

void builderContext::vel(int32_t x, int32_t y, int32_t z) {
	withOffset(bp.vel, space.vel, x, y, z);
}

void builderContext::offsetCenter(int32_t const * input) {
	adjOffset(bp.center, space.pos, input);
}

void builderContext::offsetVel(int32_t const * input) {
	adjOffset(bp.vel, space.vel, input);
}

void builderContext::radius(int32_t x, int32_t y, int32_t z) {
	bp.radius[space.indices[0]] = x;
	bp.radius[space.indices[1]] = y;
	bp.radius[space.indices[2]] = z;
}

void builderContext::btn(int ix) {
	bp.ctrl.btns[ix].v2 = 1;
}

void builderContext::resetHandlers() {
	clearBlueprintHandlers(&bp);
}

void builderContext::sl(int num, ...) {
	free(bp.sliders);
	bp.numSliders = num;
	bp.sliders = (slider*)calloc(num, sizeof(slider));
	va_list args;
	va_start(args, num);
	range(i, num) {
		bp.sliders[i].v = va_arg(args, int);
	}
	va_end(args);
}

void builderContext::heldBy(int holder, int32_t holdFlags) {
	bctx_hold &h = holds.add();
	h.holder = holder;
	h.holdee = createdEnts.num;
	h.holdFlags = holdFlags;
}

void builderContext::wireTo(int dest) {
	bctx_wire &w = wires.add();
	w.src = createdEnts.num;
	w.dest = dest;
}

void builderContext::build() {
	ent *e = (ent*)malloc(sizeof(ent));
	copyEntBlueprint(e, &bp);

	// Relative is previously created ent (if any). A good-enough heuristic.
	ent *relative;
	if (createdEnts.num) relative = createdEnts[createdEnts.num-1];
	else relative = NULL;

	initBlueprintedEnt(e, gs, relative);

	createdEnts.add(e);
	// Buttons are reset after each new ent.
	// No deep reason for this, just a guess
	// that that's the more common case. The
	// code in "cdump.cpp" assumes this too.
	range(i, NUM_CTRL_BTN) {
		bp.ctrl.btns[i].v2 = 0;
	}
}

void builderContext::finish() {
	range(i, wires.num) {
		bctx_wire &w = wires[i];
		createdEnts[w.src]->wires.add(createdEnts[w.dest]);
	}
	range(i, holds.num) {
		bctx_hold &h = holds[i];
		pickupNoHandlers(gs, createdEnts[h.holder], createdEnts[h.holdee], h.holdFlags);
	}
}

// The problem this method solves might eventually be a solved by some kind of "copy" method, IDK
void builderContext::restart() {
	createdEnts.num = wires.num = holds.num = 0;
	destroyEntBlueprint(&bp);
	initEntBlueprint(&bp);
	memcpy(bp.center, space.pos, sizeof(space.pos));
	memcpy(bp.vel, space.vel, sizeof(space.vel));
}
