#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"
#include "modules.h"

#define PREV_FUNC NULL

//Ideally these will re-define PREV_FUNC to chain them together.
// These guys aren't in use, so I never bothered converting them to the new C function business
// #include "spawner/init.c"
// #include "crate/init.c"
#include "ground/init.c"
#include "player/init.c"
// #include "move-to/init.c"
#include "stackem/init.c"
#include "platform/init.c"

void initMods() {
	void *ptr = (void*)PREV_FUNC;
	while (ptr) {
		ptr = ((void *(*)())ptr)();
	}
}
