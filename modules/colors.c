#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

static void no_draw(ent *e) {}
static void colors_white(ent *e) { drawEnt(e, 1.0, 1.0, 1.0); }
static void colors_blue(ent *e) { drawEnt(e, 0.8, 0.8, 1.0); }
static void colors_mag_1(ent *e) { drawEnt(e, 1.0, 0.0, 1.0); }
static void colors_mag_2(ent *e) { drawEnt(e, 0.7, 0.0, 0.7); }
static void colors_mag_3(ent *e) { drawEnt(e, 0.4, 0.0, 0.4); }

void colors_init() {
	regDrawHandler("no-draw", no_draw);
	regDrawHandler("clr-white", colors_white);
	regDrawHandler("clr-blue", colors_blue);
	regDrawHandler("clr-mag-1", colors_mag_1);
	regDrawHandler("clr-mag-2", colors_mag_2);
	regDrawHandler("clr-mag-3", colors_mag_3);
}
