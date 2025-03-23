#include <stdio.h>

#include "list.h"
#include "util.h"
#include "ent.h"
#include "handlerRegistrar.h"
#include "entFuncs.h"

struct handler_info {
	// Once again, type system in C isn't nuanced enough for
	// what I'm after here. I want to read from the catalog,
	// and the only thing I care about is that the result is
	// a function pointer, but I don't want to ever write to
	// it (and indeed don't know enough about the type to do
	// so, since adding the wrong type of entry is illegal).
	// Instead I'll include a manual cast that the language,
	// in my flattering estimation, should be able to infer.
	catalog<void(*)(void)> const * cat;
	// We'd also like to enforce that the type of this field
	// matches the type of the `catalog` in the same struct,
	// but we can't enforce this if we also want to permit a
	// `list<handler_info>` with different types of entries.
	void (*ent_blueprint::*field)(void);
	char const * catalogName;
	char const * fieldName;
};

static list<handler_info> handlerInfos;

static ent_blueprint _baselineBlueprint = {0};
// Me being clumsy! I want access to _baselineBlueprint that's guaranteed to be RO,
// but I also do need to modify it during `cdump_init`. This is a `const` reference
// (which most access will be through) that I hope the compiler will optimize away.
static ent_blueprint const & baselineBlueprint = _baselineBlueprint;

#define f(a,b,c,d) handlerInfos.add((handler_info){\
	(catalog<void(*)(void)> const *) &a,\
	(void (*ent_blueprint::*)(void)) &b,\
	c,\
	d\
})
void cdump_init() {
	initEntBlueprint(&_baselineBlueprint);
	handlerInfos.init();
	f(tickHandlers, ent::tick, "tickHandlers", "tick");
	f(tickHandlers, ent::tickHeld, "tickHandlers", "tickHeld");
	f(whoMovesHandlers, ent::whoMoves, "whoMovesHandlers", "whoMoves");
	f(crushHandlers, ent::crush, "crushHandlers", "crush");
	f(pushHandlers, ent::push, "pushHandlers", "push");
	f(pushedHandlers, ent::pushed, "pushedHandlers", "pushed");
	f(entPairHandlers, ent::onFumble, "entPairHandlers", "onFumble");
	f(entPairHandlers, ent::onFumbled, "entPairHandlers", "onFumbled");
	f(entPairHandlers, ent::onPickUp, "entPairHandlers", "onPickUp");
	f(entPairHandlers, ent::onPickedUp, "entPairHandlers", "onPickedUp");
}
#undef f

void cdump_destroy() {
	// TODO teardown _baselineBlueprint.
	// We shouldn't actually need to, no allocations or anything,
	// but it should be valid (and therefore valid to tear down).
	// Consistency is key with such stuff - plus it's cheap here.
	handlerInfos.destroy();
}

static char vec3differ(int32_t const *a, int32_t const *b) {
	range(i, 3) if (a[i] != b[i]) return 1;
	return 0;
}

static void vec3sub(int32_t *dest, int32_t const *a, int32_t const *b) {
	range(i, 3) dest[i] = a[i] - b[i];
}

static void emitVec3(char const *pfx, int32_t const *v) {
	printf("ctx.%s(%d, %d, %d);\n", pfx, v[0], v[1], v[2]);
}

static void simpleProp(char const *name, int32_t a, int32_t b) {
	if (a == b) return;
	printf("ctx.bp.%s = %d;\n", name, b);
}

static void emitComparison(ent_blueprint const * prev, ent const * curr, ent const * relativeTo) {
	int32_t v[3];
	if (vec3differ(prev->center, curr->center)) {
		vec3sub(v, curr->center, relativeTo->center);
		emitVec3("center", v);
	}
	if (vec3differ(prev->vel, curr->vel)) {
		vec3sub(v, curr->vel, relativeTo->vel);
		emitVec3("vel", v);
	}
	if (vec3differ(prev->radius, curr->radius)) {
		emitVec3("radius", curr->radius);
	}
	simpleProp("typeMask", prev->typeMask, curr->typeMask);
	simpleProp("collideMask", prev->collideMask, curr->collideMask);
	simpleProp("friction", prev->friction, curr->friction);
	if (prev->color != curr->color) {
		printf("ctx.bp.color = 0x%06X;\n", curr->color);
	}
	// buttons reset between ents by convention
	range(i, NUM_CTRL_BTN) {
		if (curr->ctrl.btns[i].v) {
			printf("ctx.btn(%d);\n", i);
		}
	}

	int prevEdits = 0, baselineEdits = 0;
	range(i, handlerInfos.num) {
		handler_info &h = handlerInfos[i];
		void (*handler)(void) = curr->*(h.field);
		if (handler != prev->*(h.field)) prevEdits++;
		if (handler != baselineBlueprint.*(h.field)) baselineEdits++;
	}

	// This logic might change later. Doesn't much matter,
	// these are both valid ways to describe the handlers.
	ent_blueprint const * handlerBase;
	if (prevEdits < baselineEdits || prevEdits == 0) {
		handlerBase = prev;
	} else {
		puts("ctx.resetHandlers();");
		handlerBase = &baselineBlueprint;
	}

	range(i, handlerInfos.num) {
		handler_info &h = handlerInfos[i];
		void (*handler)(void) = curr->*(h.field);
		if (handler == handlerBase->*(h.field)) continue;
		int ix = h.cat->reverseLookup(handler);
		// TODO Names here would be somewhere between "very nice" and "necessary"
		printf("ctx.bp.%s = %s.get(%d);\n", h.fieldName, h.catalogName, ix);
	}

	char needsSliders = 0;
	if (curr->numSliders != prev->numSliders) {
		needsSliders = 1;
	} else {
		range(i, curr->numSliders) {
			if (curr->sliders[i].v != prev->sliders[i].v) {
				needsSliders = 1;
				break;
			}
		}
	}
	if (needsSliders) {
		// Space after arg is to set it off visually, idk if I'll keep it
		printf("ctx.sl(%d ", curr->numSliders);
		range(i, curr->numSliders) {
			printf(", %d", curr->sliders[i].v);
		}
		puts(");");
	}
}

// Passing this `list` in by value is probably fine? Saves us some
// hassle dereferencing it, and it's `const` so it should be safe.
void cdump(list<ent*> const items, ent const * relativeTo) {
	ent_blueprint const *prev = &baselineBlueprint;

	// If stuff like "/inside" preserves iteration order, and if sequential "build()"s add things
	// to the beginning of the iteration order, then we should produce our code in reverse order.
	const int end = items.num - 1;
	for (int i = end; i >= 0; i--) {
		ent *e = items[i];

		emitComparison(prev, e, relativeTo);

		int ix;
		if (e->holder && (ix = items.find(e->holder)) != -1) {
			printf("ctx.heldBy(%d, %X);\n", end-ix, e->holdFlags);
		}
		range(j, e->wires.num) {
			if ((ix = items.find(e->wires[j])) != -1) {
				printf("ctx.wireTo(%d);\n", end-ix);
			}
		}

		puts("ctx.build();");
		prev = e;
	}
	puts("ctx.finish();");
}
