#include <stdlib.h>

#include "../util.h"
#include "../ent.h"
#include "../main.h"
#include "../entFuncs.h"
#include "../entGetters.h"
#include "../entUpdaters.h"
#include "../handlerRegistrar.h"

static void teleport_push(gamestate *gs, ent *me, ent *him, byte axis, int dir, int dx, int dv) {
	// All kinds of stuff can be pushed, some of it invisible or otherwise not "real".
	// This should be good enough to not have phantom activations
	if (type(him) & (T_TERRAIN | T_OBSTACLE | T_DEBRIS)) pushBtn(me, 0);
	ent *dest = NULL;
	wiresAnyOrder(w, me) {
		if (getButton(w, 2)) {
			if (dest == NULL) dest = w;
			else {
				puts("Behavior with multiple teleport destinations active is undefined!");
				return;
			}
		}
	}
	if (dest == NULL) return;
	int32_t v[3];
	range(i, 3) {
		v[i] = dest->center[i] - me->center[i];
	}
	uCenter(him->holdRoot, v);
	range(i, 3) {
		v[i] = dest->vel[i] - me->vel[i];
	}
	uVel(him->holdRoot, v);
}

void module_teleport() {
	pushHandlers.reg(PUSH_TELEPORT, teleport_push);
}
