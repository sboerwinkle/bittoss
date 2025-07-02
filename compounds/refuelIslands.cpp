#include "../builderContext.h"
#include "../handlerRegistrar.h"
#include "compoundUtils.h"

static void topping_none(builderContext *ctx) {
}

static void topping_bigChungus(builderContext *ctx) {
	ctx->center(0, 0, -200);
	ctx->radius(400, 400, 400);
	ctx->bp.typeMask = 8196;
	ctx->bp.collideMask = 6;
	ctx->bp.color = 0x40FFFF;
	ctx->resetHandlers();
	ctx->bp.tick = tickHandlers.get(37);
	ctx->bp.whoMoves = whoMovesHandlers.get(8);
	ctx->sl(3 , 0, 0, 3600);
	ctx->build();
	ctx->finish();
}

static void topping_fuelChunk(builderContext *ctx) {
	ctx->center(0, 0, -100);
	ctx->radius(300, 300, 300);
	ctx->bp.typeMask = 8196;
	ctx->bp.collideMask = 6;
	ctx->bp.color = 0x80FF80;
	ctx->resetHandlers();
	ctx->bp.tick = tickHandlers.get(37);
	ctx->bp.whoMoves = whoMovesHandlers.get(8);
	ctx->sl(3 , 0, 1, 1000);
	ctx->build();
	ctx->finish();
}

static void topping_kibbles(builderContext *ctx) {
	ctx->center(0, 1700, 0);
	ctx->radius(200, 200, 200);
	ctx->bp.typeMask = 8196;
	ctx->bp.collideMask = 6;
	ctx->bp.color = 0x40FFFF;
	ctx->resetHandlers();
	ctx->bp.tick = tickHandlers.get(37);
	ctx->bp.whoMoves = whoMovesHandlers.get(8);
	ctx->sl(3 , 0, 0, 225);
	ctx->build();
	ctx->center(2200, -100, 0);
	ctx->build();
	ctx->center(1000, -1700, 0);
	ctx->build();
	ctx->center(-2600, 900, 0);
	ctx->build();
	ctx->finish();
}

#define numToppings 4
typedef void (*topping)(builderContext*);
static topping const toppings[numToppings] = {
	topping_none,
	topping_bigChungus,
	topping_fuelChunk,
	topping_kibbles,
};

static int pullTopping(builderContext *ctx, int weights[numToppings+1]) {
	int pull = random(ctx->gs) % weights[0];
	range(i, numToppings) {
		pull -= weights[i+1];
		if (pull < 0) return i;
	}
	fprintf(stderr, "Bad weights [%d, %d, %d, %d, %d]\n", weights[0], weights[1], weights[2], weights[3], weights[4]);
	return 0;
}

static void addTopping(builderContext *ctx, int weights[numToppings+1], int32_t const pos[3]) {
	int32_t offset[3];
	offset[0] = pos[0] - 800 + (random(ctx->gs) % 1601);
	offset[1] = pos[1] - 800 + (random(ctx->gs) % 1601);
	offset[2] = pos[2];

	spaceFacts origSpace = ctx->space;
	ctx->offsetCenter(offset);
	ctx->restart();
	reorient(ctx);

	topping t = toppings[pullTopping(ctx, weights)];
	(*t)(ctx);

	ctx->space = origSpace;
}

static int32_t const enorme_toppings[][3] = {
	{  8300, -12100, -700},
	{  2700, -10300, -700},
	{ -9500, -10300, -700},
	{ -7900,  -8500, -700},
	{-10900,   -300, -700},
	{ -7900,  11900, -700},
	{  2300,   9500, -700},
	{  2300,   2500, -700},
	{  9700,   1600, -700},
	{  9900,   8000, -700},
};
static int const num_enorme_toppings = sizeof(enorme_toppings) / sizeof(enorme_toppings[0]);

static void plat_enorme(builderContext *ctx, int weights[numToppings+1]) {
	ctx->radius(15000, 15000, 500);
	ctx->bp.typeMask = 19;
	ctx->bp.collideMask = 2;
	ctx->bp.color = 0xFFFFFF;
	ctx->bp.whoMoves = whoMovesHandlers.get(7);
	ctx->build();
	ctx->finish();

	range(i, num_enorme_toppings) {
		addTopping(ctx, weights, enorme_toppings[i]);
	}
}

static int32_t const slender_toppings[][3] = {
	{-1600, -11500, -700},
	{ 2400,  -5700, -700},
	{-1600,   2100, -700},
	{-1600,  10300, -700},
	{ 1800,  11900, -700},
};
static int const num_slender_toppings = sizeof(slender_toppings) / sizeof(slender_toppings[0]);

static void plat_slender(builderContext *ctx, int weights[numToppings+1]) {
	ctx->radius(5000, 15000, 500);
	ctx->bp.typeMask = 19;
	ctx->bp.collideMask = 2;
	ctx->bp.color = 0xFFFFFF;
	ctx->bp.whoMoves = whoMovesHandlers.get(7);
	ctx->build();
	ctx->finish();

	range(i, num_slender_toppings) {
		addTopping(ctx, weights, slender_toppings[i]);
	}
}

static int32_t const coinage_toppings[][3] = {
	{-1400, -14000, -1700},
	{-9400, -13600, -1700},
	{-1400,  -7600, -1700},
	{ 6200,  -5200, -1700},
	{-6400,  -3400, -1700},
	{-2200,   4200,  -700},
	{ 6400,   6400,  -700},
};
static int const num_coinage_toppings = sizeof(coinage_toppings) / sizeof(coinage_toppings[0]);

static void plat_coinage(builderContext *ctx, int weights[numToppings+1]) {
	ctx->radius(10000, 10000, 500);
	ctx->bp.typeMask = 19;
	ctx->bp.collideMask = 2;
	ctx->bp.color = 0xFFFFFF;
	ctx->bp.whoMoves = whoMovesHandlers.get(7);
	ctx->build();
	ctx->center(-5000, -10000, -1000);
	ctx->bp.color = 0xD0FFD0;
	ctx->heldBy(0, 0);
	ctx->build();
	ctx->finish();

	range(i, num_coinage_toppings) {
		addTopping(ctx, weights, coinage_toppings[i]);
	}
}

// Eventually the def'n of `plat` may differ from `topping`... or maybe I'll just combine them?
typedef void (*plat)(builderContext*, int weights[numToppings+1]);
static plat const plats[] = {
	plat_enorme,
	plat_slender,
	plat_coinage,
};
static int const numPlats = sizeof(plats) / sizeof(plats[0]);

static void mkPlat(builderContext *ctx, int weights[numToppings+1], int32_t x, int32_t y, int32_t z) {
	int32_t offset[3] = {x, y, z};

	spaceFacts origSpace = ctx->space;
	ctx->offsetCenter(offset);
	ctx->restart();
	reorient(ctx);

	plat p = plats[random(ctx->gs) % numPlats];
	(*p)(ctx, weights);

	ctx->space = origSpace;
}

static void shape_triangle(builderContext *ctx, int weights[numToppings+1]) {
#define elev (random(ctx->gs) % 2001 - 1000)
	mkPlat(
		ctx, weights,
		-18500, 0, elev
	);
	mkPlat(
		ctx, weights,
		12500 + random(ctx->gs) % 10000, -15500 - random(ctx->gs) % 5000, elev
	);
	mkPlat(
		ctx, weights,
		12500 + random(ctx->gs) % 10000,  15500 + random(ctx->gs) % 5000, elev
	);
#undef elev
}

static void shape_pancakes(builderContext *ctx, int weights[numToppings+1]) {
#define x (random(ctx->gs) % 30001 - 15000)
	mkPlat(ctx, weights, x, x,  4000 + (random(ctx->gs) % 10000));
	mkPlat(ctx, weights, x, x,     0);
	mkPlat(ctx, weights, x, x, -4000 - (random(ctx->gs) % 10000));
#undef x
}

static void shape_single(builderContext *ctx, int weights[numToppings+1]) {
	mkPlat(ctx, weights, 0, 0, 0);
}

// No need to define a new type, `plat` and `shape` have the same signature
static plat const shapes[] = {
	shape_triangle,
	shape_pancakes,
	shape_single,
};
static int const numShapes = sizeof(shapes) / sizeof(shapes[0]);

static void getWeights(builderContext *ctx, int weights[numToppings+1]) {
	int tmp[numToppings+1] = {9,4,1,1,3}; // Sum, followed by weights for each topping
/*
	topping_none,
	topping_bigChungus,
	topping_fuelChunk,
	topping_kibbles,
*/
	// We've got 3 different spreads we use:
	// 40% of the time: default spread
	// 10% of the time: single item pulled from the default spread
	// 50% of the time: spread modified with 60/25/15/00 multipliers
	int spread = random(ctx->gs) % 10;
	if (spread < 4) {
		memcpy(weights, tmp, sizeof(tmp));
		return;
	}
	range(i, numToppings+1) weights[i] = 0;
	if (spread == 4) {
		int t = pullTopping(ctx, tmp);
		weights[0] = weights[t+1] = 1;
	} else {
		int multipliers[numToppings] = {12,5,3,0};
		range(i, numToppings) {
			int t = pullTopping(ctx, tmp);
			int w = tmp[t+1];
			tmp[t+1] = 0;
			tmp[0] -= w;
			w *= multipliers[i];
			weights[t+1] = w;
			weights[0] += w;
		}
	}
}

void mkRefuelIsland(builderContext *ctx) {
	int weights[numToppings+1];
	getWeights(ctx, weights);

	plat s = shapes[random(ctx->gs) % numShapes];
	(*s)(ctx, weights);
}
