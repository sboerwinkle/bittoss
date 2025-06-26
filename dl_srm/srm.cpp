#include "../util.h"
#include "../ent.h"
#include "../handlerRegistrar.h"

static void srmHoldy(gamestate *gs) {
	tick_t cursed = tickHandlers.get(TICK_CURSED);
	for (ent *i = gs->ents; i; i = i->ll.n) {
		int32_t hf = i->holdFlags;
		if (i->holder) {
			int32_t base = 0x000000;
			int32_t plus = 0x80;
			if (i->tick == cursed) {
				base = 0x303030;
				plus = 0xCF;
			}
			if (hf == HOLD_PASS) {
				i->color = base + plus * 0x010000;
			} else if (hf == HOLD_DROP) {
				i->color = base + plus * 0x000100;
			} else if (hf == HOLD_MOVE) {
				i->color = base + plus * 0x000001;
			} else {
				i->color = base + plus * 0x010001;
			}
		} else {
			if (hf == HOLD_PASS) {
				i->color = 0x808080;
			} else {
				i->color = 0xA060A0;
			}
		}
	}
}

static void srmMaterial(gamestate *gs) {
	for (ent *i = gs->ents; i; i = i->ll.n) {
		uint32_t t = i->typeMask & (T_OBSTACLE | T_TERRAIN | T_HEAVY | T_DECOR);
		if      (t == T_OBSTACLE)
			i->color = 0xFF0000;
		else if (t == T_OBSTACLE + T_HEAVY)
			i->color = 0xFFFF00;
		else if (t == T_TERRAIN)
			i->color = 0x00FF00;
		else if (t == T_TERRAIN + T_HEAVY)
			i->color = 0x00FFFF; // TODO wall vs metal
		else if (t == T_DECOR)
			i->color = 0xFF8080;
		else if (t == 0)
			i->color = 0xFFFFFF;
		else
			i->color = 0xFF00FF;
		if (!(i->typeMask & T_WEIGHTLESS)) i->color = 3*((i->color/4) & 0x3F3F3F);
	}
}

extern "C" void dl_applySpecialRender(gamestate *gs, int mode) {
	// We can assume `mode` is non-zero.
	if (mode == 1) srmHoldy(gs);
	else if (mode == 2) srmMaterial(gs);
}
