#include "../util.h"
#include "../ent.h"
#include "../handlerRegistrar.h"

extern "C" void dl_applySpecialRender(gamestate *gs, int mode) {
	// We can assume `mode` is non-zero.
	// At present there's only one non-zero mode, so we don't even check `mode` hahahh
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
				i->color = 0xC0C080;
			} else {
				i->color = 0xA060A0;
			}
		}
	}
}
