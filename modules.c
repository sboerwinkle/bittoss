#include "util.h"
#include "ent.h"
#include "main.h"
#include "entFuncs.h"
#include "entGetters.h"
#include "entUpdaters.h"
#include "handlerRegistrar.h"

// At some point maybe this will do dynamic loading?
// That might be overkill though, really this is just
// to make sure that all this non-engine stuff ("modules")
// gets propertly initialized, and for that I think
// it's okay to have a laundry-list file

#include "modules/common.h"
#include "modules/colors.h"
#include "modules/explosion.h"
#include "modules/flag.h"
#include "modules/ground.h"
#include "modules/platform.h"
#include "modules/edittool.h"
#include "modules/player.h"
#include "modules/stackem.h"

void initMods() {
	common_init();
	colors_init();
	explosion_init();
	flag_init();
	ground_init();
	platform_init();
	edittool_init();
	player_init();
	stackem_init();
}
