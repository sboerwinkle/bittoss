#pragma once

#include <stdint.h>
#include "list.h"
#include "random.h"
#include "box.h"

// Largest power of 2 where both +/- fit inside a byte.
// Can increase more if we set aside more space for player input in the network message layout.
#define axisMaxis 64
#define holdeesAnyOrder(var, who) for (ent *var = who->holdee; var; var = var->LL.n)
#define wiresAnyOrder(var, who) ent *var; for (int __i = 0; __i < who->wires.num && ((var = who->wires[__i]) || 1); __i++)
#define NUM_CTRL_BTN 4
// Despite the name, the iteration order is very much deterministic and boring.
// However, it is a reminder that it is against the spirit of the game to treat
// things differently for no visible reason - thus you shouldn't depend on
// iteration order to make any decisions.

/*
Basically what we're after is a
simple,
fast,
frame-of-reference agnostic,
aligned rectangles only
collision framework.

We'd like to allow for things to be crushed a la Broforce. Aside from being fun, it allows us to cop out of the toughest collision situations: Large stacks of things pushed from both ends
We allow a special "holding" relation between objects that allows them to aglomerate into bigger, more complex objects. The holding object manages most things past that point.
*/

typedef unsigned char byte;

extern const int32_t zeroVec[3];

/*
Anatomy of a tick
tasks run
-onTick / onTickHeld
	Not allowed to look at anyone else (to enforce parallelism)
	Also the only place the masks can change!
-onDraw
	Just in case we want multiple physics frames per drawing frame, this is separate.
Physics loop
	Determining who collides
		-whoMoves (can look anywhere, no changes anywhere)
	Running the collisions
		-onPushed for everyone who was pushed
			Can change everything about state, since old state was preserved
		-onPush for everyone who pushed someone
			This is where the gnarly shit like teleportation can happen

*/

struct ent;
struct gamestate;

typedef int (*whoMoves_t)(struct ent*, struct ent*, int, int);
typedef void (*tick_t)(struct gamestate *gs, struct ent*);
typedef void (*crush_t)(struct gamestate *gs, struct ent*);
typedef void (*push_t)(struct gamestate *gs, struct ent*, struct ent*, byte, int, int, int);
typedef char (*pushed_t)(struct gamestate *gs, struct ent*, struct ent*, int, int, int, int);
typedef void (*entPair_t)(struct gamestate *gs, struct ent*, struct ent*);

typedef struct {
	int32_t v, min, max;
} slider;

// Later this might only contain some of the fields, for now it's everything
typedef struct ent_blueprint {

	//// Things related to collisions ////
	// If set, means it should be tested for collisions. Once it passes a single iteration w/o collisions, this is turned off.
	char needsCollision;
	// If set, means this ent's position and/or velocity changed during this physics iteration.
	char needsPhysUpdate;
	// Who the collision is applied to. All of these will only be set for root entities.
	struct ent *collisionLeaf;
	// The best collision we've found so far
	struct ent *collisionBuddy;
	// Pending collision is for both people; used to rank which collision to process first
	char collisionMutual;
	byte collisionAxis;
	int collisionDir;
	// which direction, if any, this ent has been forced this tick. If forced in conflicting ways, splatters.
	//	If forced the same direction too many times, splatters anyway, because funky business is probably afoot.
	int32_t forced[3];
	// Which of my descendants this forcing came from.
	struct ent *forcedHoldees[3];

	uint32_t typeMask, collideMask;
	uint32_t newTypeMask, newCollideMask;
	// Relative to the world even when held.
	int32_t center[3], d_center[3];
	// Where the center will be at the end of this iteration
	int32_t center2[3];
	// Center at the beginning of this tick
	int32_t old[3];
	int32_t vel[3], d_vel[3];
	int32_t vel2[3];
	int32_t radius[3];
	// Thing for custom R Tree impl
	box *myBox;

	struct {
		struct {
			char v, v2;
		} btns[NUM_CTRL_BTN];
	} ctrl;

	//TODO: Orientation

	//Things not related to collisions, unless fumbling is involved.
	//Who, if anyone, is holding me.
	struct ent *holder;
	int32_t holdFlags;
	//Who's holding me ultimately (Can be myself)
	struct ent *holdRoot;
	//The first person (if any) I'm holding
	struct ent *holdee;
	// Each ent can keep track of arbitrarily many other ents. We add some more rules to this for fun:
	// Ents do not know who (if anyone) is tracking them
	// Tracked ents do not have an explicit order (even though they're stored as a list, we think of it as a set)
	// An ent cannot track another ent multiple times (again, it's a set) or associate any other info with the tracking
	list<struct ent*> wires;

	struct ent *newHolder;
	int32_t newHoldFlags;
	char newDrop;

	// Don't need two per of these, since there's no handler code called when a wire is actually added / removed
	list<struct ent*> wiresAdd;
	list<struct ent*> wiresRm;

	//Things really definitely not related to collisions
	// For whatever internal state they need
	slider *sliders;
	int numSliders;
	struct {
		struct ent *n, *p;
	} ll, LL;
	char dead, dead_max[2];
	char fullFlush;

	// Many, many event handlers.
	whoMoves_t whoMoves;
	tick_t tick;
	// Like above, but when someone is holding me. Doesn't technically have to be a separate method, but a lot of things are going to do nothing when held, so this encourages that "default" behavior.
	tick_t tickHeld;
	int32_t color;
	int32_t friction;
	crush_t crush;
	push_t push;
	pushed_t pushed;

	// Called first
	entPair_t onFumble;
	entPair_t onFumbled;
	// Should be called before onPickedUp. Sets his center, for cases such as turrets, which know where the gunner's seat position is. It's okay to cop out and just keep it where it is on screen.
	//TODO: These can't access holder / holdee information for any external references they might have.
	entPair_t onPickUp;
	entPair_t onPickedUp;

	union {
		ent *ref;
		int32_t ix;
	} clone;
} ent_blueprint;

typedef struct ent : ent_blueprint {
} ent;

// Maybe later we'll do something fancier with dynamic allocation.
// For now, this gets it off the ground
#define NAME_BUF_LEN 8
struct player {
	ent *entity;
	char name[NAME_BUF_LEN];
	int32_t color;
	int32_t reviveCounter;
	int32_t data;
};

struct gamestate {
	ent *ents;
	ent *rootEnts;
	//This linked list is only maintained one-directionally so that the "next" pointer can remain unchanged. Since ents may kill themselves while being looped over, this last part is important.
	ent *deadTail;
	// TODO Maybe with luck we can do away with this???
	byte flipFlop_death;
	int32_t rand;
	int32_t gamerules;
	box *rootBox;

	list<player> players;
};

extern void boundVec(int32_t *values, int32_t bound, int32_t len);

extern void flushCtrls(ent *e);
extern void flushPickups(gamestate *gs);

//extern void moveRecursive(ent *who, int32_t *vel);

//extern void accelRecursive(ent *who, int32_t *a);

void entsLlAdd(gamestate *gs, ent *e);
void entsLlRemove(gamestate *gs, ent *e);

extern void assignVelbox(ent *e, box *relBox);
extern void addEnt(gamestate *gs, ent *e, ent *relative);
extern void pickupNoHandlers(gamestate *gs, ent *x, ent *y, int32_t holdFlags);

extern void doUpdates(gamestate *gs);
extern void prepPhysics(gamestate *gs);
extern void doPhysics(gamestate *gs);
extern void finishStep(gamestate *gs);
extern void drawSign(ent *e, char const *text, int size, int32_t const *const oldPos, int32_t const *const newPos, float const ratio);
extern void doDrawing(gamestate *gs, ent *inhabit, char thirdPerson, int32_t const *oldPos, int32_t const *newPos, float interpRatio);
extern void doCleanup(gamestate *gs);
extern gamestate* mkGamestate();
extern void resetGamestate(gamestate *gs);

extern rand_t random(gamestate *gs);
extern gamestate* dup(gamestate *gs);

extern void ent_init();
extern void ent_destroy();

// A matrix describing the resolution for two different whoMoves calls:
/* 
			| me	| him	| both	| none	
--------------------------------------------------------
him (that is, me)	| me	| both	| me	| me	
me (that is, him)	| both	| him	| him	| him	
both			| me	| him	| both	| both	
none			| me	| him	| both	| none	
*/

//Precedence for results is something like: me/him, both, none.
//This is because "none" is the ultimate cop-out ("I didn't actually mean to collide with you, just couldn't tell from your typeMask"), so the behavior is as if your whoMoves was never called.
//"both" is still kind of a cop-out: It suggests that the entity responding "both" can't see a good way to resolve the conflict, but knows that they do have to collide somehow.

//Also know that, as per my usual habits, these values actually matter and can't be changed at will
#define MOVE_ME 1
#define MOVE_HIM 2
#define MOVE_BOTH 7
#define MOVE_NONE 15

// holdFlags values
#define HOLD_PASS 0
#define HOLD_DROP 1
#define HOLD_MOVE 2
#define HOLD_MASK 3
// Things outside the mask may be used when processing pickups,
// but aren't stored on the resulting connection
#define HOLD_FREEZE 4
// TODO This should actually do something, I know how it'll work I just haven't done it yet
#define HOLD_SINGLE 8


//Collisions are slippery, but objects can update their internal vel's to create friction if'n they so choose

//When crushing, our first preference is to force a drop if that has a chance to resolve the conflict. So we locate the two leaf objects responsible, and working back up the tree in synchrony, look for an okay fumble.
//If none are found, well too bad, both leaf nodes are fumbled.

#define T_HEAVY 1
#define T_TERRAIN 2
#define T_OBSTACLE 4
#define T_DEBRIS 8
#define T_WEIGHTLESS 16
#define T_FLAG 32
#define T_DECOR 64
#define T_NO_DRAW_SELF 128 // Don't draw this ent if the player is holding it
#define TEAM_BIT 256
#define TEAM_MASK (7*TEAM_BIT)
#define T_INPUTS (1<<11)
#define T_EQUIP (1<<12)
#define T_EQUIP_SM (1<<13)
#define EQUIP_MASK (T_EQUIP+T_EQUIP_SM)
// T_ACTIVE used to be 2048, that really had no reason to be a type flag and is now gone.

// This is kind of messy, but not all type flags necessarily require an ent to do collision checking.
// For instance, it's assumed nobody has T_WEIGHTLESS in their collideMask.
// This messiness is because typeMask is de facto two things - types for collisions, and behavioral types.
#define T_COLLIDABLE (T_HEAVY + T_TERRAIN + T_OBSTACLE + T_DEBRIS + T_FLAG)
