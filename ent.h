#pragma once

#include <stdint.h>
#include "list.h"
#include "random.h"
#include "box.h"

#define holdeesAnyOrder(var, who) for (ent *var = who->holdee; var; var = var->LL.n)
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
struct entRef;
struct gamestate;

typedef int (*whoMoves_t)(struct ent*, struct ent*, int, int);
typedef void (*tick_t)(struct gamestate *gs, struct ent*);
typedef void (*crush_t)(struct gamestate *gs, struct ent*);
typedef int (*pushed_t)(struct gamestate *gs, struct ent*, struct ent*, int, int, int, int);
typedef void (*push_t)(struct gamestate *gs, struct ent*, struct ent*, byte, int, int, int);


typedef struct {
	int32_t v, min, max;
} slider;

typedef struct ent {

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

#define axisMaxis 32
	struct {
		struct {
			char v, v2;
		} btns[4];
		struct {
			signed char min[2], max[2];
			signed char v[2];
		} axis1;
		struct {
			signed char min[3], max[3];
			signed char v[3];
		} look;
	} ctrl;

	//TODO: Orientation

	//Things not related to collisions, unless fumbling is involved.
	//Who, if anyone, is holding me.
	struct ent *holder;
	//Who's holding me ultimately (Can be myself)
	struct ent *holdRoot;
	//The first person (if any) I'm holding
	struct ent *holdee;
	// Each ent can keep track of arbitrarily many other ents. We add some more rules to this for fun:
	// Ents do not know who (if anyone) is tracking them
	// Tracked ents do not have an explicit order (even though they're stored as a list, we think of it as a set)
	// An ent cannot track another ent multiple times (again, it's a set) or associate any other info with the tracking
	list<struct ent*> wires;

	//Two sets for these, because the handler for the event may request an event of the same type, and it's got to happen *next* iteration
	struct ent *holder_max[2];
	char pickup_max[2];
	char drop_max[2];

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

	// Many, many event handlers.
	whoMoves_t whoMoves;
	tick_t tick;
	// Like above, but when someone is holding me. Doesn't technically have to be a separate method, but a lot of things are going to do nothing when held, so this encourages that "default" behavior.
	tick_t tickHeld;
	//void (*onTickHeld)(struct ent *me);
	int (*tickType)(struct ent *me, struct ent *him);
	int32_t color;
	crush_t crush;
	push_t push;
	pushed_t pushed;

	// Called first
	void (*onFumble)(struct ent *me, struct ent *him);
	void (*onFumbled)(struct ent *me, struct ent *him);
	// Should be called before onPickedUp. Sets his center, for cases such as turrets, which know where the gunner's seat position is. It's okay to cop out and just keep it where it is on screen.
	//TODO: These can't access holder / holdee information for any external references they might have.
	void (*onPickUp)(struct ent *me, struct ent *him);
	void (*onPickedUp)(struct ent *me, struct ent *him);
	//void (*onDrop)(struct ent *me, int holdee);
	//void (*onDropped)(struct ent *me);
	char (*okayFumble)(struct ent *me, struct ent *him);
	char (*okayFumbleHim)(struct ent *me, struct ent *him);

	union {
		ent *ref;
		int32_t ix;
	} clone;
} ent;

struct player {
	ent *entity;
	int32_t color;
	int32_t reviveCounter;
};

struct gamestate {
	ent *ents;
	ent *rootEnts;
	//This linked list is only maintained one-directionally so that the "next" pointer can remain unchanged. Since ents may kill themselves while being looped over, this last part is important.
	ent *deadTail;
	// TODO Maybe with luck we can do away with these???
	byte flipFlop_death, flipFlop_drop, flipFlop_pickup;
	int32_t rand;
	int32_t gamerules;
	box *rootBox;

	list<player> *players;
};

extern void flushPickups(gamestate *gs);
extern void flushCtrls(ent *e);
extern void flushMisc(ent *e, const int32_t *parent_d_center, const int32_t *parent_d_vel);

//extern void moveRecursive(ent *who, int32_t *vel);

//extern void accelRecursive(ent *who, int32_t *a);

extern byte getAxis(ent *a, ent *b);

extern void assignVelbox(ent *e, box *relBox);
extern void addEnt(gamestate *gs, ent *e, ent *relative);
extern void pickupNoHandlers(gamestate *gs, ent *x, ent *y);

extern void drawEnt(ent *e, float r, float g, float b);

extern void doUpdates(gamestate *gs);
extern void doPhysics(gamestate *gs);
extern void finishStep(gamestate *gs);
extern void doDrawing(gamestate *gs, ent *inhabit);
extern void doCleanup(gamestate *gs);
extern gamestate* mkGamestate(list<player> *players);
extern void resetGamestate(gamestate *gs);

extern rand_t random(gamestate *gs);
extern gamestate* dup(gamestate *gs, list<player> *players);

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

//Constants for `pushed` return code
enum retCodes {
	r_die = 0,
	r_drop = 1,
	r_move = 2,
	r_pass = 3
};


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
#define T_NO_DRAW_FP 128
#define TEAM_BIT 256
#define TEAM_MASK (7*TEAM_BIT)
