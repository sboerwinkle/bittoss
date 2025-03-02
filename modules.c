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
#include "modules/explosion.h"
#include "modules/flag.h"
#include "modules/ground.h"
#include "modules/platform.h"
#include "modules/spider.h"
#include "modules/edittool.h"
#include "modules/player.h"
#include "modules/eyes.h"
#include "modules/stackem.h"
#include "modules/logic.h"
#include "modules/door.h"
#include "modules/dust.h"
#include "modules/legg.h"
#include "modules/loadblock.h"
#include "modules/material.h"
#include "modules/puddlejumper.h"
#include "modules/respawn.h"
#include "modules/seat.h"
#include "modules/thrust.h"
#include "modules/factory.h"
#include "modules/gun.h"
#include "modules/blink.h"
#include "modules/jumper.h"
#include "modules/teamselect.h"
#include "modules/scoreboard.h"
#include "modules/teleport.h"
#include "modules/sign.h"

void initMods() {
	module_common();
	module_explosion();
	module_flag();
	module_ground();
	module_platform();
	module_spider();
	module_edittool();
	module_player();
	module_eyes();
	module_stackem();
	module_logic();
	module_door();
	module_dust();
	module_legg();
	module_loadblock();
	module_material();
	module_puddlejumper();
	module_respawn();
	module_seat();
	module_thrust();
	module_factory();
	module_gun();
	module_blink();
	module_jumper();
	module_teamselect();
	module_scoreboard();
	module_teleport();
	module_sign();
}
