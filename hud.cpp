#include "util.h"
#include "graphics.h"
#include "handlerRegistrar.h"
#include "ent.h"
#include "entGetters.h"

#include "modules/player.h"

static tick_t gun_tick_held;

void hud_init() {
	gun_tick_held = tickHandlers.get(TICK_HELD_GUN);
}

void hud_destroy() {}

static float hudColor[3] = {0.0, 0.5, 0.5};

static void drawEditCursor() {
	drawHudRect(0.5 - 2.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
	drawHudRect(0.5 + 1.0/128, 0.5 - 1.0/128, 1.0/128, 1.0/64, hudColor);
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
		for (e = p->holdee; e; e = e->LL.n) {
			if (type(e) & T_EQUIP) {
				// Draw UI for the equipment, not for us
				break;
			}
		}
		if (e && e->tickHeld == gun_tick_held) {
			double start = 0.5 - 5.0/64;
			int ammo = getSlider(e, 0); // out of 10
			int reload = getSlider(e, 2); // out of 90
			double width = ammo/64.0;
			drawHudRect(start, 0.5, width, 1.0/64, hudColor); // Ammo present
			drawHudRect(start + width, 0.5 + 1.0/256, 10.0/64 - width, 1.0/128, hudColor); // Ammo absent
			drawHudRect(start, 0.5 + 5.0/256, reload*10/64.0/90, 1.0/128, hudColor); // Reload progress
		} else {
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
