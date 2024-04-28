#include <math.h>

#include "util.h"
#include "graphics.h"
#include "handlerRegistrar.h"
#include "ent.h"
#include "entGetters.h"

#include "modules/player.h"

static tick_t gun_tick_held, blink_tick_held, jumper_tick_held;

void hud_init() {
	gun_tick_held = tickHandlers.get(TICK_HELD_GUN);
	blink_tick_held = tickHandlers.get(TICK_HELD_BLINK);
	jumper_tick_held = tickHandlers.get(TICK_HELD_JUMPER);
}

void hud_destroy() {}

static float hudColor[3] = {0.0, 0.5, 0.5};

static void drawEditCursor() {
	drawHudRect(0.5 - 2.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
	drawHudRect(0.5 + 1.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
}

static void drawCursor(double x, double y, double w, double h, float *c) {
	drawHudRect(0.5+x, 0.5+y, w, h, c);
}

static void drawCursorFlip(double x, double y, double w, double h, float *c) {
	drawHudRect(0.5-x-w, 0.5+y, w, h, c);
}

static void drawEquipUi(ent *e) {
	if ((e->typeMask & EQUIP_MASK) == T_EQUIP_SM) {
		// ambidexterous equipment draws on either side of the cursor
		void (*d)(double x, double y, double w, double h, float *c);
		d = getSlider(e, 0) ? drawCursor : drawCursorFlip;
		if (e->tickHeld == blink_tick_held) {
			double fill = (double) getSlider(e, 1) / getSlider(e, 2);
			if (!isfinite(fill)) fill = 0;
			const double unit = 1.0/128;
			d(0.1, -unit, unit, fill*2*unit, hudColor);
			// May revise this later, just need a basic cursor component to tell it apart
			d(0.1+1*unit, -unit, unit, unit, hudColor);
			d(0.1+2*unit, 0    , unit, unit, hudColor);
			d(0.1+3*unit, -unit, unit, unit, hudColor);
			d(0.1+4*unit, 0    , unit, unit, hudColor);
		} else if (e->tickHeld == jumper_tick_held) {
			int ammo = getSlider(e, 1);
			double reload = (double) getSlider(e, 2) / getSlider(e, 5);
			double offset = 0.1;
			const double unit = 1.0/128;
			// Bars with arrow for full charges
			range(i, ammo) {
				d(offset, 0, 5*unit, unit, hudColor);
				d(offset+2*unit, -3*unit, unit, 2*unit, hudColor);
				d(offset+unit, -4.5*unit, 3*unit, unit, hudColor);
				d(offset+2*unit, -5.5*unit, unit, unit, hudColor);
				offset += 6*unit;
			}
			// Progress bar (w/out arrow) for partial charge
			d(offset, 0, reload*5*unit, unit, hudColor);
		}
	} else if (e->tickHeld == gun_tick_held) {
		int ammo = getSlider(e, 0);
		int ammoMax = getSlider(e, 3);
		if (ammoMax <= 0) ammoMax = 1;
		int reload = getSlider(e, 2);
		int reloadMax = getSlider(e, 5);
		if (reloadMax <= 0) reloadMax = 1;
		double fullWidth = ammoMax/64.0;
		double start = 0.5 - fullWidth/2;
		double width = ammo/64.0;
		drawHudRect(start, 0.5, width, 1.0/64, hudColor); // Ammo present
		drawHudRect(start + width, 0.5 + 1.0/256, fullWidth - width, 1.0/128, hudColor); // Ammo absent
		drawHudRect(start, 0.5 + 5.0/256, fullWidth*reload/reloadMax, 1.0/128, hudColor); // Reload progress
	}
}

void drawHud(ent *p) {
	if (!p) return;
	if (p->holder) {
		drawHudRect(0.5-1.0/128, 0.5-1.0/128, 1.0/64, 1.0/64, hudColor);
	} else {
		if (getSlider(p, PLAYER_EDIT_SLIDER)) {
			drawEditCursor();
			return;
		}
		ent *e;
		char foundEquip = 0;
		for (e = p->holdee; e; e = e->LL.n) {
			if (type(e) & EQUIP_MASK) {
				drawEquipUi(e);
				foundEquip = 1;
			}
		}
		if (!foundEquip) {
			// UI for basic shooty-block player
			int charge = p->sliders[8].v;
			// Very old saves with obsolete "player" setups sometimes render this bar waaaaaay too wide; this at least keeps it playable until we fix things
			if (charge > 600) charge = 600;
			float x = 0.5 - 3.0/128;
			while (charge >= 60) {
				drawHudRect(x, 0.5, 1.0/64, 1.0/64, hudColor);
				x += 1.0/64;
				charge -= 60;
			}
			drawHudRect(x, 0.5 + 1.0/256, (float)charge*(1.0/64/60), 1.0/128, hudColor);
		}
	}
}
