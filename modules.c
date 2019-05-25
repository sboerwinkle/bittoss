#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entUpdaters.h"
#include "modules.h"

#define PREV_FUNC NULL

//Ideally these will re-define PREV_FUNC to chain them together.
#include "crate/init.c"
#include "ground/init.c"
#include "player/init.c"

void initMods() {
	void *ptr = (void*)PREV_FUNC;
	while (ptr) {
		ptr = ((void *(*)())ptr)();
	}
}
