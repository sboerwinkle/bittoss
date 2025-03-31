#include "../ent.h"
#include "../builderContext.h"

void reorient(builderContext *ctx) {
	rand_t r = random(ctx->gs);
	// Randomizes the flat orientation between all 8 possibilities
/*
    OX XO
    O   O

 OO       OO
 X         X

 X         X
 OO       OO

    O   O
    OX XO
*/
	ctx->space.signs[0] = 1 - 2*(r&1);
	ctx->space.signs[1] = 1 -   (r&2);
	if (r & 4) {
		int *x = ctx->space.indices;
		int tmp = x[0];
		x[0] = x[1];
		x[1] = tmp;
		// Usually we'd also have to swap signs[0] <-> signs[1].
		// We just randomized those, though, so it's kinda moot.
	}
}
