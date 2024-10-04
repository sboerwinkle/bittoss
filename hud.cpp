#include <math.h>

#include "util.h"
#include "graphics.h"
#include "handlerRegistrar.h"
#include "ent.h"
#include "entGetters.h"
#include "edit.h"

#include "modules/player.h"

static tick_t gun_tick_held, blink_tick_held, jumper_tick_held;
static pushed_t flag_pushed;

void hud_init() {
	gun_tick_held = tickHandlers.get(TICK_HELD_GUN);
	blink_tick_held = tickHandlers.get(TICK_HELD_BLINK);
	jumper_tick_held = tickHandlers.get(TICK_HELD_JUMPER);
	flag_pushed = pushedHandlers.get(PUSHED_FLAG);
}

void hud_destroy() {}
static float hudColorDark[3] = {0.0, 0.3, 0.3};
static float hudColor[3] = {0.0, 0.5, 0.5};
static float bluColor[3] = {0.0, 0.0, 1.0};
static float redColor[3] = {1.0, 0.0, 0.0};

static void drawEditCount(float x, int count, float *color) {
	const float range = 1.0/16;
	const float slice = range/8;
	const float y = 0.5 - range/2;
	float step = (range - slice) / (2*count);
	if (count >= 8) {
		// At 8 or so, they'll touch, and we can just draw a big bar
		drawHudRect(x, y + step, 1.0/128, range - 2*step, color);
	} else {
		range(i, count) {
			drawHudRect(x, y + (1+2*i)*step, 1.0/128, slice, color);
		}
	}
}

static void drawEditCursor(ent *p) {
	drawHudRect(0.5 - 2.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
	drawHudRect(0.5 + 1.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
	int blu, red;
	countSelections(p, &blu, &red);
	drawEditCount(0.5 - 3.5/128, blu, bluColor);
	drawEditCount(0.5 + 2.5/128, red, redColor);
}

static void drawCursor(double x, double y, double w, double h, float *c) {
	drawHudRect(0.5+x, 0.5+y, w, h, c);
}

static void drawCursorFlip(double x, double y, double w, double h, float *c) {
	drawHudRect(0.5-x-w, 0.5+y, w, h, c);
}

// Returns the hands in use by this equipment.
// We treat these hands as a bitfield in a couple places,
// but this method also returns the "4" bit for primary + 2-handed equipments (which draw a central reticle)
static char drawEquipUi(ent *e) {
	if ((e->typeMask & EQUIP_MASK) == T_EQUIP_SM) {
		// ambidexterous equipment draws on either side of the cursor
		void (*d)(double x, double y, double w, double h, float *c);
		char hands;
		if (getSlider(e, 0)) {
			hands = 2;
			d = drawCursor;
		} else {
			hands = 1;
			d = drawCursorFlip;
		}

		if (e->tickHeld == blink_tick_held) {
			double fill = 1.0 - (double) getSlider(e, 1) / getSlider(e, 2);
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
		return hands;
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
	} else if (e->pushed == flag_pushed) {
		double unit = 1.0/64;
		double line = 1.0/256;
		// Sketch a simple flag icon.
		// Some lines overlap, that's fine
		drawHudRect(0.5 - unit, 0.5, line, 2*unit, hudColor); // flagstaff
		drawHudRect(0.5 - unit, 0.5, 2*unit, line, hudColor); // Top of flag
		drawHudRect(0.5 - unit, 0.5+unit-line, 2*unit, line, hudColor); // Bottom of flag
		drawHudRect(0.5+unit-line, 0.5, line, unit, hudColor); // Far edge of flag
	}
	return 4 + ((e->typeMask & T_EQUIP_SM) ? 3 : 1);
}

static void drawToolsCursor(ent *p) {
	ent *e;
	char hands = 0;
	for (e = p->holdee; e; e = e->LL.n) {
		if (type(e) & EQUIP_MASK) {
			hands |= drawEquipUi(e);
		}
	}
	if ((hands & 3) != 3) {
		// UI for basic shooty-block player

		int charge = getSlider(p, 8);
		// Very old saves with obsolete "player" setups sometimes render this bar waaaaaay too wide; this at least keeps it playable until we fix things
		if (charge > 600) charge = 600;

		float x;
		if ((hands & 3) == 1) x = 0.6; // Draw it to the right if only our primary hand is full
		else {
			x = 0.5 - 3.0/128; // Otherwise (empty hands / off-hand full), draw in center
			hands |= 4; // Also mark central reticle as filled so we don't add the dot
		}

		while (charge >= 60) {
			drawHudRect(x, 0.5, 1.0/64, 1.0/64, hudColor);
			x += 1.0/64;
			charge -= 60;
		}
		drawHudRect(x, 0.5 + 1.0/256, (float)charge*(1.0/64/60), 1.0/128, hudColor);
	}
	// Small mark if the central reticle hasn't been filled by anything else
	if (!(hands & 4)) {
		drawHudRect(0.5 - 1.0/256, 0.5, 1.0/128, 1.0/256, hudColor);
	}
}

static void drawVeloceter(ent *p) {
	// Integer truncation is maybe helpful here.
	// If we were to allow fractional number of pixels, it could come out different
	// along the X axis than the Y axis after aspect ratio scaling (when they both get truncated independently).
	int displayPx = displayHeight / 64;
	float height = (float)displayPx / displayHeight;
	float width  = (float)displayPx / displayWidth;
	float x = width*2;
	float y = 1 - 1.0/64 - height*2;

	float o_x = (float)getSlider(p, 5)/PLAYER_SPD;
	float o_y = (float)getSlider(p, 6)/PLAYER_SPD;

	flatCamRotated(displayPx, x, y);
	drawHudRect(o_x - 0.5, o_y - 0.5, 1, 1, hudColor);
	drawHudRect(-0.5, -0.5, 1, 1, hudColorDark);
	flatCamDefault();
}

void drawHud(ent *p) {
	if (!p) return;
	if (p->holder) {
		drawHudRect(0.5-1.0/128, 0.5-1.0/128, 1.0/64, 1.0/64, hudColor);
	} else {
		if (getSlider(p, PLAYER_EDIT_SLIDER)) {
			drawEditCursor(p);
		} else {
			drawToolsCursor(p);
		}
		// 2px line for grounded / not grounded indicator
		float *c = getSlider(p, 7) ? hudColor : hudColorDark;
		drawHudRect(0.5 - 1.0/64, 0.6, 1.0/32, 2.0/displayHeight, c);
		drawVeloceter(p);
	}
}
