#include <math.h>

#include "util.h"
#include "graphics.h"
#include "handlerRegistrar.h"
#include "ent.h"
#include "entGetters.h"
#include "edit.h"

#include "modules/player.h"
#include "modules/bottle.h"
#include "modules/puddlejumper.h"

static tick_t gun_tick_held, blink_tick_held, jumper_tick_held, bottle_tick, pj_tick;
static pushed_t flag_pushed;

void hud_init() {
	gun_tick_held = tickHandlers.get(TICK_HELD_GUN);
	blink_tick_held = tickHandlers.get(TICK_HELD_BLINK);
	jumper_tick_held = tickHandlers.get(TICK_HELD_JUMPER);
	bottle_tick = tickHandlers.get(TICK_BOTTLE);
	pj_tick = tickHandlers.get(TICK_PUDDLEJUMP);
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
		} else if (e->tick == bottle_tick) {
			int32_t bottleType = getSlider(e, BOTTLE_TYPE_SLIDER);
			int32_t amt = getSlider(e, BOTTLE_AMT_SLIDER);
			// This little bit of work could be a function maybe
			char multiple;
			if (amt >= 10'000) {
				amt /= 1'000;
				if (amt >= 10'000) {
					amt /= 1'000;
					multiple = 'M';
				} else {
					multiple = 'k';
				}
			} else {
				multiple = ' ';
			}
			char typeChar;
			if (bottleType == BOTTLE_JUICE) typeChar = 'J';
			else if (bottleType == BOTTLE_FUEL) typeChar = 'F';
			else typeChar = '?';
			char str[7];
			snprintf(str, 7, "%d%c%c", amt, multiple, typeChar);
			int width = strlen(str);
			double x;
			if (hands == 1) x = -2-width;
			else x = 2;
			drawHudText(str, 0.5, 0.5, x, 0, 0.5, hudColor);
		}
		return hands;
	} else if (e->tickHeld == gun_tick_held) {
		int ammo = getSlider(e, 1);
		int ammoMax = getSlider(e, 4);
		if (ammoMax <= 0) ammoMax = 1;
		int reload = getSlider(e, 3);
		int reloadMax = getSlider(e, 6);
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
	// We already handled the offhand case, so this would be the 2-handed case
	if (e->typeMask & T_EQUIP_SM) return 7;
	// Only remaining case is 1-handed primary
	return 4 + 1 + !!getSlider(e, 0);
}

static void drawToolsCursor(ent *p) {
	ent *e;
	char hands = 0;
	for (e = p->holdee; e; e = e->LL.n) {
		if (type(e) & EQUIP_MASK) {
			hands |= drawEquipUi(e);
		}
	}
	// If we have some equipment, but nothing in the
	// middle of the screen yet, we draw a lil mark.
	if (hands && !(hands & 4)) {
		drawHudRect(0.5 - 1.0/256, 0.5, 1.0/128, 1.0/256, hudColor);
	}
	// We don't need the `4` bit anymore
	hands &= ~4;
	if (hands != 3) {
		// UI for basic shooty-block player

		int charge = getSlider(p, 8);
		// Very old saves with obsolete "player" setups sometimes render this bar waaaaaay too wide; this at least keeps it playable until we fix things
		if (charge > 600) charge = 600;

		float x;
		if (hands == 1) x = 0.6; // Right side
		else if (hands == 2) x = 0.4 - 3.0/64; // Left side
		else x = 0.5 - 3.0/128; // Centered (no equipment)

		while (charge >= 60) {
			drawHudRect(x, 0.5, 1.0/64, 1.0/64, hudColor);
			x += 1.0/64;
			charge -= 60;
		}
		drawHudRect(x, 0.5 + 1.0/256, (float)charge*(1.0/64/60), 1.0/128, hudColor);
	}
}

static void drawHeldCursor(ent *p) {
	int displayPx = displayHeight / 128;
	float x_unit = (float)displayPx / displayWidth;
	float y_unit = (float)displayPx / displayHeight;
	drawHudRect(0.5-x_unit, 0.5-y_unit, 2*x_unit, 2*y_unit, hudColor);
	ent *root = p->holdRoot;
	if (root->tick == pj_tick) {
		// Draw some additional notches to indicate horizontal engine power output
		if (root->numSliders > PJ_POWER_SLIDER) {
			int power = root->sliders[PJ_POWER_SLIDER].v;
			if (power) {
				drawHudRect(0.5-0.5*x_unit, 0.5-3*y_unit, x_unit, y_unit, hudColor);
				if (power > 1) {
					drawHudRect(0.5-0.5*x_unit, 0.5-5*y_unit, x_unit, y_unit, hudColor);
				}
			}
		}

		// Up-facing arrow (on the left) - a hint that LMB is for thrust
		double offset = 0.5 - 6*x_unit;
		// Arrow stem
		drawHudRect(offset-2*x_unit, 0.5           ,   x_unit, 2*y_unit, hudColor);
		// Arrow head (base)
		drawHudRect(offset-3*x_unit, 0.5-1.5*y_unit, 3*x_unit,   y_unit, hudColor);
		// Arrow head (tip)
		drawHudRect(offset-2*x_unit, 0.5-2.5*y_unit,   x_unit,   y_unit, hudColor);
		// Used to be something on the right, but we no longer have RMB bound to anything
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

	int32_t v[2] = {getSlider(p, 5), getSlider(p, 6)};
	boundVec(v, PLAYER_SPD*1.25, 2);
	float o_x = (float)v[0]/PLAYER_SPD;
	float o_y = (float)v[1]/PLAYER_SPD;

	flatCamRotated(displayPx, x, y);
	drawHudRect(o_x - 0.5, o_y - 0.5, 1, 1, hudColor);
	drawHudRect(-0.5, -0.5, 1, 1, hudColorDark);
	flatCamDefault();
}

void drawHud(ent *p) {
	if (!p) return;
	if (p->holder) {
		drawHeldCursor(p);
	} else {
		if (getSlider(p, PLAYER_EDIT_SLIDER)) {
			drawEditCursor(p);
		} else {
			drawToolsCursor(p);
		}
		// 2px line for grounded / not grounded indicator
		if (getSlider(p, 7)) {
			drawHudRect(0.5 - 1.0/64, 0.6, 1.0/32, 2.0/displayHeight, hudColor);
		} else {
			drawHudRect(0.5 - 1.0/ 64, 0.6, 1.0/128, 2.0/displayHeight, hudColor);
			drawHudRect(0.5 + 1.0/128, 0.6, 1.0/128, 2.0/displayHeight, hudColor);
		}
		drawVeloceter(p);
	}
}
