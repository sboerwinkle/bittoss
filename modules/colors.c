#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static void no_draw(ent *e) {}
static void colors_white(ent *e) { drawEnt(e, 1.0, 1.0, 1.0); }
static void colors_blue(ent *e) { drawEnt(e, 0.8, 0.8, 1.0); }
static void colors_mag_1(ent *e) { drawEnt(e, 1.0, 0.0, 1.0); }
static void colors_mag_2(ent *e) { drawEnt(e, 0.7, 0.0, 0.7); }
static void colors_mag_3(ent *e) { drawEnt(e, 0.4, 0.0, 0.4); }

static void team_draw(ent *e) {
	int typ = type(e);
	drawEnt(
		e,
		(typ & TEAM_BIT) ? 1.0 : 0.5,
		(typ & (2*TEAM_BIT)) ? 1.0 : 0.5,
		(typ & (4*TEAM_BIT)) ? 1.0 : 0.5
	);
}

void module_colors() {
	drawHandlers.reg("no-draw", no_draw);
	drawHandlers.reg("clr-white", colors_white);
	drawHandlers.reg("clr-blue", colors_blue);
	drawHandlers.reg("clr-mag-1", colors_mag_1);
	drawHandlers.reg("clr-mag-2", colors_mag_2);
	drawHandlers.reg("clr-mag-3", colors_mag_3);
	drawHandlers.reg("team-draw", team_draw);
}
