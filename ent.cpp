#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "util.h"
#include "list.h"
#include "ent.h"
#include "main.h"
#include "graphics.h"

static void flushEntState(entState *s) {
	range(i, s->numSliders) {
		//sliders are "sticky", i.e. don't reset if left alone
		if (s->sliders[i].max < s->sliders[i].min) continue;
		s->sliders[i].v = (s->sliders[i].max + s->sliders[i].min)/2;
		s->sliders[i].max = INT32_MIN;
		s->sliders[i].min = INT32_MAX;
	}
}

void freeEntState(entState *s) {
	free(s->sliders);
}

void setupEntState(entState *s, int numSliders, int numRefs) {
	s->numSliders = numSliders;
	s->sliders = (slider*) calloc(numSliders, sizeof(slider));
	flushEntState(s);
}

void flushCtrls(ent *who) {
	int i;
	for (i = 0; i < 4; i++) {
		who->ctrl.btns[i].v = who->ctrl.btns[i].v2;
		who->ctrl.btns[i].v2 = 0;
	}

	for (i = 0; i < 2; i++) {
		who->ctrl.axis1.v[i] = (who->ctrl.axis1.max[i] + who->ctrl.axis1.min[i]) / 2;
		who->ctrl.axis1.min[i] = axisMaxis;
		who->ctrl.axis1.max[i] = -axisMaxis;
	}
	// Don't both updating the look direction unless there's been some input;
	// it will not reset to 0 on its own the same way the axis will
	if (who->ctrl.look.min[0] != axisMaxis || who->ctrl.look.max[0] != -axisMaxis) {
		for (i = 0; i < 3; i++) {
			who->ctrl.look.v[i] = (who->ctrl.look.max[i] + who->ctrl.look.min[i]) / 2;
			who->ctrl.look.min[i] = axisMaxis;
			who->ctrl.look.max[i] = -axisMaxis;
		}
	}
}

static void recursiveHoldRoot(ent *who, ent *r) {
	who->holdRoot = r;
	ent *e;
	for (e = who->holdee; e; e = e->LL.n) {
		recursiveHoldRoot(e, r);
	}
}

static void addRootEnt(gamestate *gs, ent *e) {
	if (gs->rootEnts) gs->rootEnts->LL.p = e;
	e->LL.n = gs->rootEnts;
	e->LL.p = NULL;
	gs->rootEnts = e;
	recursiveHoldRoot(e, e);
}

void addEnt(gamestate *gs, ent *e) {
	if (gs->ents) gs->ents->ll.p = e;
	e->ll.n = gs->ents;
	e->ll.p = NULL;
	gs->ents = e;
	/*
	if (entsToAdd) entsToAdd->ll.p = e;
	e->ll.n = entsToAdd;
	e->ll.p = NULL;
	entsToAdd = e;
	*/
	addRootEnt(gs, e);
	//Some basic state that we may want to get in place now, e.g. in case anyone asks if it's okay to schedule a pickup.
	//e->holdRoot = e;
}

static void moveRecursive(ent *who, int32_t *dc, int32_t *dv) {
	who->needsPhysUpdate = 1;
	int i;
	for (i = 0; i < 3; i++) {
		who->center2[i] = who->center[i] + dc[i];
		who->vel2[i] = who->vel[i] + dv[i];
	}
	ent *e;
	for (e = who->holdee; e; e = e->LL.n) {
		moveRecursive(e, dc, dv);
	}
}

static void requireCollisionRecursive(ent *who) {
	who->needsCollision = 1;
	ent *e;
	for (e = who->holdee; e; e = e->LL.n) {
		requireCollisionRecursive(e);
	}
}

static void fumble(gamestate *gs, ent *h) {
	ent *a = h->holder;
	if (!a) return;
	h->needsCollision = 1;
	//For now at least, flushDrops and flushDeaths don't work from roots, which means t
	// ...hese are restricted in what they can actually access.
	(a->onFumble)(a, h);
	(h->onFumbled)(h, a);
	int i;
	ent *x, *y;
	for (i = 0; i < 3; i++) {
		x = h;
		while ( (y = x->holder) && y->forced[i] && y->forcedHoldees[i] == x) {
			y->forced[i] = 0;
			x = y;
		}
	}
	if (h->LL.n) h->LL.n->LL.p = h->LL.p;
	if (h->LL.p) h->LL.p->LL.n = h->LL.n;
	else a->holdee = h->LL.n;
	h->holder = NULL;
	addRootEnt(gs, h);
}

static void rmRootEnt(gamestate *gs, ent *e) {
	if (e->LL.p) e->LL.p->LL.n = e->LL.n;
	else gs->rootEnts = e->LL.n;
	if (e->LL.n) e->LL.n->LL.p = e->LL.p;
}

void pickupNoHandlers(gamestate *gs, ent *x, ent *y) {
	rmRootEnt(gs, y);
	y->LL.n = x->holdee;
	y->LL.p = NULL;
	if (x->holdee) x->holdee->LL.p = y;
	x->holdee = y;
	y->holder = x;
	recursiveHoldRoot(y, x->holdRoot);
}

static void pickup(gamestate *gs, ent *x, ent *y) {
	//printf("pickup, ff is %d\n", flipFlop_pickup);
	x->onPickUp(x, y);
	y->onPickedUp(y, x);
	pickupNoHandlers(gs, x, y);
}

static void killEntNoHandlers(gamestate *gs, ent *e) {
	while (e->holdee) fumble(gs, e->holdee);
	fumble(gs, e);

	//if (ents == e) ents = e->ll.n;
	if (e->ll.p) e->ll.p->ll.n = e->ll.n;
	else gs->ents = e->ll.n;
	if (e->ll.n) e->ll.n->ll.p = e->ll.p;

	rmRootEnt(gs, e);

	e->ll.p = gs->deadTail;
	gs->deadTail = e;
	e->dead = 1;
}

static void crushEnt(gamestate *gs, ent *e) {
	if (e->dead) {
		fputs("I don't think we should ever try to re-crush!\n", stderr);
		return;
	}
	// TODO invoke `crush` (should actually exist, with a default no-op handler)
	killEntNoHandlers(gs, e);
}

static int whoMoves(ent *a, ent *b, byte axis, int dir) {
#ifndef NODEBUG
	if (a->whoMoves == NULL) {
		fputs("whoMoves unset\n", stderr);
		return MOVE_ME;
	}
#endif
	return (*a->whoMoves)(a, b, axis, dir);
}

static int arbitrateMovement(ent *a, ent *b, byte axis, int dir) {
	int A = whoMoves(a, b, axis, dir);
	int B = whoMoves(b, a, axis, -dir);
	if (B == MOVE_ME) B = MOVE_HIM;
	else if (B == MOVE_HIM) B = MOVE_ME;
	A = A&B;
	return A?A:MOVE_BOTH;
}

int getAxisAndDir(ent *a, ent *b) {
	int d1[3], d2[3], w[3];
	int i;
	for (i = 0; i < 3; i++) {
		d1[i] = b->old[i] - a->old[i];
		d2[i] = b->center[i] - a->center[i];
		w[i] = a->radius[i] + b->radius[i];
		if (d1[i] > 0) {
			if (d1[i] >= w[i] && d2[i] >= w[i]) return 0;
		} else {
			if (d1[i] <= -w[i] && d2[i] <= -w[i]) return 0;
		}
		d2[i] = d1[i] - d2[i];
	}
	//d1 is now the old vector from a to b
	//and d2 is a's velocity relative to b

	// Put the object to our upper right (in cartesian coordinates)
	int dir_by_axis[3];
	for (i = 0; i < 3; i++) {
		if (d2[i] >= 0) {
			dir_by_axis[i] = -(i+1);
		} else {
			dir_by_axis[i] = (i+1);
			d1[i] *= -1;
			d2[i] *= -1;
		}
	}
	// Now find the lower-left corner
	for (i = 0; i < 3; i++) d1[i] -= w[i];
	int best;
#define compare(target, a, b) do {\
	int64_t cmp = ((int64_t)d1[a])*d2[b] - ((int64_t)d1[b])*d2[a]; \
	if (cmp > 0) target = a; \
	else if (cmp < 0) target = b; \
	else { \
		/* TODO I had intended to also check by angle of attack compared to the dimensions */ \
		if (w[a] > w[b]) target = b; \
		else target = a; \
	} \
} while(0)
	compare(best, 0, 1);
	compare(best, best, 2);
#undef compare
	if (d1[best] < 0) {
		// In this case we've always been inside! The scandal! Time to decide if we're actively leaving
		// Here we define 'best' as 'closest to the surface', regardless of velocities.
		int depth = INT32_MAX;
		int ret;
		for (i = 0; i < 3; i++) {
			int d = d1[i] + w[i];
			int new_depth;
			int new_ret;
			if (d >= 0) {
				new_depth = w[i] - d;
				new_ret = dir_by_axis[i];
			} else {
				new_depth = w[i] + d;
				// Center is behind us, so if we have non-zero velocity along this axis we're leaving.
				new_ret = d2[i] ? 0 : -dir_by_axis[i];
			}
			if (new_depth < depth) {
				depth = new_depth;
				ret = new_ret;
			}
		}
		return ret;
	}
	// Otherwise, the collision is at some point in the future, and we need do decide if we're going to dodge it or not
	for (i = 0; i < 3; i++) {
		int far = d1[i] + 2*w[i];
		int64_t cmp = ((int64_t)d1[best])*d2[i] - ((int64_t)far)*d2[best];
		if (cmp >= 0) return 0;
	}
	return dir_by_axis[best];
}

static inline int min(int a, int b) { return a < b ? a : b; }

static char collisionBetter(ent *root, ent *leaf, ent *n, byte axis, int dir, char mutual) {
	if (!root->collisionBuddy) return 1;
	ent *folks[] = {n, root->collisionBuddy};
	ent *leafs[] = {leaf, root->collisionLeaf};
	byte axes[] = {axis, root->collisionAxis};
	int dirs[] = {dir, root->collisionDir};
	char mutuals[] = {mutual, root->collisionMutual};
	int vals[2];
	int i;
#define cb_helper(func) for (i = 0; i < 2; i++) vals[i] = func; if (vals[0]!=vals[1]) return vals[0]>vals[1]
	//Criteria for ranking collisions:
	//Prefer mutual collisions, reduces weirdness where one half will "miss" the collision by processing a different collision.
	cb_helper(mutuals[i]);
	// Distance to lateral clearance (maximize, old)
#define clearance(x) folks[i]->radius[x] + leafs[i]->radius[x] - abs(folks[i]->old[x] - leafs[i]->old[x])
	cb_helper(min(clearance((axes[i]+1)%3), clearance((axes[i]+2)%3)));
#undef clearance
	/* Removed in favor of maximizing distance to lateral miss
	//prior edge overlap (exists, old)
	cb_helper(folks[i]->radius[1^axes[i]] + leafs[i]->radius[1^axes[i]] > abs(folks[i]->old[1^axes[i]] - leafs[i]->old[1^axes[i]]));
	*/
	//required displacement (maximize, current)
	cb_helper(folks[i]->radius[axes[i]]+leafs[i]->radius[axes[i]] + (folks[i]->center[axes[i]] - leafs[i]->center[axes[i]])*dirs[i]);
	/* //Actually I don't want to do this, because area should really be in an int64_t
	//area of other (maximize)
	cb_helper(folks[i]->radius[0]*folks[i]->radius[1]);
	*/
	//TODO: In this case, miminize holder distance to colliding leaf
	//exposed surface of other (maximize)
	//cb_helper(folks[i]->radius[1^axes[i]]);
	//Sideways offset (miminize, old)
	//cb_helper(-abs(folks[i]->old[1^axes[i]] - leafs[i]->old[1^axes[i]]));
	//TODO; If that fails, maybe try exposed broadside surface of the leaf's recursive holders?
	//puts("Using a collision-ranking criterion which I should not!");
	//puts("Using collision-ranking criteria which break symmetry!");
	cb_helper(axes[i]);
	cb_helper(dirs[i]);
	//puts("Using the most evil collision-ranking criterion");
	return 0;
#undef cb_helper
}

static byte tryCollide(ent *leaf, ent *buddy, byte axis, int dir, char mutual) {
	ent *root = leaf->holdRoot;
	if (collisionBetter(root, leaf, buddy, axis, dir, mutual)) {
		root->collisionLeaf = leaf;
		root->collisionBuddy = buddy;
		root->collisionMutual = mutual;
		root->collisionAxis = axis;
		root->collisionDir = dir;
		return 1;
	}
	return 0;
}

static byte checkCollision(ent *a, ent *b) {
	char AonB = 0 != (b->typeMask & a->collideMask);
	char BonA = 0 != (a->typeMask & b->collideMask);
	if (!AonB && !BonA) return 0;
	if (a->holdRoot == b->holdRoot) return 0;
	int dir = getAxisAndDir(a, b);
	if (dir == 0) return 0;
	byte axis;
	if (dir < 0) {
		axis = (byte)(-dir - 1);
		dir = -1;
	} else {
		axis = (byte)(dir - 1);
		dir = 1;
	}
	//Okay, so we definitely have a collision now.
	//Next step is deciding who moves.
	//Then, for each person who does move, we have do decide if this is the best collision so far.
	int movee = (AonB ?
			(BonA ?
				arbitrateMovement(a, b, axis, dir) :
				whoMoves(a, b, axis, dir)) :
			3^whoMoves(b, a, axis, -dir));
	//We throw in the 3^ to flip MOVE_ME and MOVE_HIM, but the only problem is that this injures the MOVE_NONE and MOVE_BOTH symbols, so we can't just check for equality here.
	if (movee & 8) return 0; // MOVE_NONE
	if (movee & 4) { // MOVE_BOTH
		/*
		//In this case we need to test the forced values and possibly change movee...
		if ((a->forced[axis] == -dir) ^ (b->forced[axis] == dir)) {
			movee = MOVE_ME + (a->forced[axis] == -dir);
		} else {
		*/
			byte ret = tryCollide(a, b, axis, dir, 1);
			ret |= tryCollide(b, a, axis, -dir, 1);
			return ret;
		//}
	}
	if (movee == MOVE_ME) {
		return tryCollide(a, b, axis, dir, 0);
	}
	//else it's MOVE_HIM
	return tryCollide(b, a, axis, -dir, 0);
}

static char doIteration(gamestate *gs) {
	//TODO: velbox (quadtree stuff)
	char ret = 0;
	ent *i, *j;
	for (i = gs->ents; i; i = i->ll.n) {
		for (j = i->ll.n; j; j = j->ll.n) {
			if (i->needsCollision || j->needsCollision) {
				ret |= checkCollision(i, j);
			}
		}
		i->needsCollision = 0;
	}
	return ret;
}

static void clearDeads(gamestate *gs) {
	ent *d;
	while ( (d = gs->deadTail) ) {
		gs->deadTail = d->ll.p;
		freeEntState(&d->state);
		d->wires.destroy();
		d->wiresAdd.destroy();
		d->wiresRm.destroy();
		free(d);
	}
}

static ent* getPreferredFumble(ent *leaf, ent *root) {
	while (leaf != root) {
		ent *p = leaf->holder;
		if (leaf->okayFumble(leaf, p) || p->okayFumbleHim(p, leaf)) {
			return leaf;
		}
		leaf = p;
	}
	return NULL;
}

static ent* pinch(gamestate *gs, ent *e, byte axis, ent *o) {
	ent *leaf1 = e;
	while (leaf1->forcedHoldees[axis]) leaf1 = leaf1->forcedHoldees[axis];
	ent *leaf2;
	if (o) {
		leaf2 = o;
		while (leaf2->forcedHoldees[axis]) leaf2 = leaf2->forcedHoldees[axis];
	} else {
		leaf2 = e;
	}
	ent *f1 = getPreferredFumble(leaf1, e);
	ent *f2 = getPreferredFumble(leaf2, e);
	if (f1) {
		fumble(gs, f1);
		if (f2) {
			fumble(gs, f2);
			return f2;
		}
		return e;
	}
	if (f2) {
		fumble(gs, f2);
		return f2;
	}
	if (leaf1 != e) {
		fumble(gs, leaf1);
		if (leaf2 != e) {
			fumble(gs, leaf2);
			return leaf2;
		}
		return e;
	}
	if (leaf2 != e) {
		fumble(gs, leaf2);
		return leaf2;
	}
	crushEnt(gs, e);
	return NULL;
}

static char push_helper(gamestate *gs, ent *e, ent *prev, byte axis, int dir) {
	if (e->forced[axis]*dir < 0) {
		ent* ret = pinch(gs, e, axis, prev);
		if (!ret) return 1;
		if (e != ret) return 0;
	}
	e->forced[axis] += dir;
	if (e->forced[axis] == dir*3) {
		puts("Safety crush!");
		crushEnt(gs, e);
		return 1;
	}
	e->forcedHoldees[axis] = prev;
	return 0;
}

static void push(gamestate *gs, ent *e, ent *o, byte axis, int dir) {
	int displacement = (o->center[axis] - e->center[axis])*dir + e->radius[axis] + o->radius[axis];
	if (displacement < 0) printf("Shouldn't get a negative displacement!\n");
	int accel = (o->vel[axis] - e->vel[axis])*dir;
	if (accel < 0) accel = 0;
	//TODO: This guy should get to know the person who was moved, and how.
	//Does it matter that this guy might get called twice, if the other guy gets crushed? Shouldn't matter if he's told the other guy got crushed.
	//I think we need it to be called twice, so the other guy gets to know that he was pushed while not being held.
	o->push(gs, o, e, axis, dir, displacement, accel);

	ent *prev = NULL;
	while (1) {
		if (push_helper(gs, e, prev, axis, dir)) break;
		
		int ret;
		if (e->pushed == NULL) {
			ret = r_pass;
		} else {
			ret = (*e->pushed)(gs, e, o, axis, dir, displacement, accel);
		}

		prev = e;
		e = e->holder;
		if (ret == r_die) {
			puts("Chose to die!");
			crushEnt(gs, prev);
			break;
		}
		if (ret == r_pass && e) continue;
		if (ret == r_drop && e) fumble(gs, prev);
		int32_t dc[3], dv[3];
		dc[(axis+1)%3] = dv[(axis+1)%3] = dc[(axis+2)%3] = dv[(axis+2)%3] = 0;
		dc[axis] = displacement*dir;
		dv[axis] = accel*dir;
		moveRecursive(prev, dc, dv);
		break;
	}
}

static void _doTick(gamestate *gs, ent *e) {
	if (e->tick != NULL) (*e->tick)(gs, e);
}

static void _doTickHeld(gamestate *gs, ent *e) {
	if (e->tickHeld != NULL) (*e->tickHeld)(gs, e);
}

static void doTick(gamestate *gs, ent *e, int type) {
	if (type == 1) _doTickHeld(gs, e);
	else _doTick(gs, e);

	ent *i;
	for (i = e->holdee; i; i = i->LL.n) {
		int ret = e->tickType(e, i);
		if (ret) doTick(gs, i, ret);
	}
}

void flushMisc(ent *e) {
	//Negate "min" so it shows the bits we're allowed to keep
	e->d_typeMask_min = ~e->d_typeMask_min;
	e->typeMask |= e->d_typeMask_max & e->d_typeMask_min;
	e->typeMask &= e->d_typeMask_max | e->d_typeMask_min;
	e->d_typeMask_max = e->d_typeMask_min = 0;

	e->d_collideMask_min = ~e->d_collideMask_min;
	e->collideMask |= e->d_collideMask_max & e->d_collideMask_min;
	e->collideMask &= e->d_collideMask_max | e->d_collideMask_min;
	e->d_collideMask_max = e->d_collideMask_min = 0;

	range(i, 3) {
		e->center[i] += (e->d_center_min[i] + e->d_center_max[i]) / 2;
		e->vel[i] += e->d_vel[i];
		e->d_center_min[i] = e->d_center_max[i] = e->d_vel[i] = 0;
	}

	// We have a "rule" that the order of wires doesn't matter;
	// but if some module violates that assumption,
	// we don't actually want to cause a desync. Hence, we aren't going to
	// store the wires sorted (as that would depend on assigned memory addresses),
	// though we have no qualms about mangling the order (deterministically) when a wire is removed.
	range(i, e->wires.num) {
		ent *w = e->wires[i];
		if (w->dead || e->wiresRm.has(w)) {
			e->wires.quickRmAt(i); // Mangles the order, just pulls the last element into this place
			i--;
		}
	}
	e->wiresRm.num = 0;
	range(i, e->wiresAdd.num) {
		ent *w = e->wiresAdd[i];
		if (!e->wires.has(w)) e->wires.add(w);
	}
	e->wiresAdd.num = 0;
}

//TODO: If this was ordered, onCrush could have access to its whole tree (but not those of others!)
static void flushDeaths(gamestate *gs) {
	ent *i;
	for (i = gs->ents; i; i = i->ll.n) {
		if (i->dead_max[1^gs->flipFlop_death]) {
			//puts("Scripted death!");
			crushEnt(gs, i);
		}
	}
}

//TODO: onFumble[d] could *probably* have access strictly up or down the tree (but no switching!) if we fix the ordering in here.
//Recall that crushing situations may allow for 2 simultaneous fumbles on different branches.
static void flushDrops(gamestate *gs) {
	ent *i;
	for (i = gs->ents; i; i = i->ll.n) {
		if (i->drop_max[1^gs->flipFlop_drop]) {
			i->drop_max[1^gs->flipFlop_drop] = 0;
			fumble(gs, i);
		}
	}
}

static void flushPickups(gamestate *gs) {
	ent *i, *next;
	byte ff = 1^gs->flipFlop_pickup;
	for (i = gs->rootEnts; i; i = next) {
		next = i->LL.n;

		//If I'm picking someone else up, clean up and move along.
		if (i->pickup_max[ff]) {
			i->pickup_max[ff] = 0;
			i->holder_max[ff] = NULL;
			continue;
		}
		ent *x = i->holder_max[ff];
		//If I'm not being picked up, nothing to do.
		if (!x) continue;
		// TODO this ruins i->LL.n for the next iteration of the loop
		if (x != i && !x->dead) pickup(gs, x, i);
		i->holder_max[ff] = NULL;
	}
}

static void flush(gamestate *gs) {
	//Now we've got to worry about requests issued while that same request is being flushed. Lame.
	gs->flipFlop_death ^= 1;
	flushDeaths(gs);
	//I think deaths has to come first. Otherwise e.g. a pickup could schedule another pickup on something that will be gone by the end of this turn,
	//or a death could schedule an add and a pickup for this guy on that add.
	//flushAdds();
	gs->flipFlop_drop ^= 1;
	//TODO: We're leaking references to unadded ents, and that causes problems. Something could be picked up by an ent that doesn't properly exist yet. Probably the best bet is to review and make damn sure that there's no harm in adding ents immediately when requested.
	//I think our only complaint was that it permits infinitely loopy physics iterations if some jackass writes a stupid script.
	//And face it, slow / loopy scripts are an inevitable hazard. Maybe better to drop the worries in this dep't and just add immediately.
	flushDrops(gs);
	gs->flipFlop_pickup ^= 1;
	flushPickups(gs); // Pickups has to happen before these others because it's fairly likely that it will influence some of them.
	ent *i;
	for (i = gs->ents; i; i = i->ll.n) {
		flushMisc(i);
		flushEntState(&i->state);
	}
}

// This one has to be separate because we don't want it flushed out multiple times during a tick,
// only at the start.
// Specifically we don't want anything flushed between ticks and physics, since then physics can't
// read player controls.
static void flushCtrls(gamestate *gs) {
	for (ent *i = gs->ents; i; i = i->ll.n) {
		flushCtrls(i);
	}
}

void doUpdates(gamestate *gs) {
	flushCtrls(gs);
	ent *i;
	for (i = gs->rootEnts; i; i = i->LL.n) {
		doTick(gs, i, 2);
	}
}

void doPhysics(gamestate *gs) {
	flush(gs);
	ent *i;
	for (i = gs->ents; i; i = i->ll.n) {
		memcpy(i->old, i->center, sizeof(i->old));
		int j;
		for (j = 0; j < 3; j++) {
			i->center[j] += i->vel[j];
			i->forced[j] = 0;
		}
		i->needsCollision = 1;
	}
	//Note that deaths, drops get flushed in here periodically.
	//TODO: This means handlers called in here (onPush[ed], onFumble[d], onCrush)
	//can't be sure about the death status (easy, ignore death) or holditude of anything.
	//Except onPush[ed] can access members of the pushees tree
	while(doIteration(gs)) {
		for (i = gs->rootEnts; i; i = i->LL.n) {
			if (i->collisionBuddy) {
				//Do physics-y type things in here
				push(gs, i->collisionLeaf, i->collisionBuddy, i->collisionAxis, i->collisionDir);
				i->collisionBuddy = NULL;
				requireCollisionRecursive(i);
			}
		}
		for (i = gs->ents; i; i = i->ll.n) {
			if (i->needsPhysUpdate) {
				memcpy(i->center, i->center2, sizeof(i->center));
				memcpy(i->vel, i->vel2, sizeof(i->vel));
				i->needsPhysUpdate = 0;
			}
		}
	}
	/* TODO:
	Right now we're doing what we'll call "level 1" collisions - just resolve everything until it stops.
	We also might want options for:
		level 0 - only one round of resolution per tick
		level 1 - If a round has any unpushed pushers, only resolve them. This might require more careful maintenance of the "needsCollision" flags.
		level 2 - Like level 1, but if there are no unpushed pushers we next check for circular pushers.
	*/
	flush(gs);
}

void finishStep(gamestate *gs) {
	clearDeads(gs);
}

static void drawEnt(ent *e) {
	if (e->draw == NULL) {
		fputs("Entity draw function unset\n", stderr);
		return;
	}
	(*e->draw)(e);
}

void doDrawing(gamestate *gs) {
	ent *i;
	//for (i = rootEnts; i; i = i->LL.n) {
	for (i = gs->ents; i; i = i->ll.n) {
		drawEnt(i);
	}
}

void drawEnt(ent *e, float r, float g, float b) {
	rect(e->center, e->radius, r, g, b);
}

void doCleanup(gamestate *gs) {
	while (gs->rootEnts) killEntNoHandlers(gs, gs->rootEnts);
	clearDeads(gs);
}

gamestate *mkGamestate(list<player> *players) {
	gamestate *ret = (gamestate*)calloc(1, sizeof(gamestate));
	ret->rand = 1;
	players->num = 0;
	ret->players = players;
	return ret;
}

// Usually we'll just make a new one, but it's possible to need this version as well.
// Note that we're not cleaning anything up, so the caller had better do that first!
void resetGamestate(gamestate *gs) {
	// Save off `players`
	list<player> *p = gs->players;
	// Zero everything
	bzero(gs, sizeof(gamestate));
	// Restore / reset the fields we care about
	gs->players = p;
	gs->rand = 1;
}

rand_t random(gamestate *gs) {
	stepRand(&gs->rand);
	return gs->rand;
}

static ent* cloneEnt(ent *in) {
	ent* ret = (ent*) malloc(sizeof(ent));
	// Most of the fields we can just directly copy, no fuss no muss
	*ret = *in;
#ifndef NODEBUG
	if (ret->collisionBuddy || ret->holder_max[0] || ret->holder_max[1]) {
		fputs("One of the transient ent*'s wasn't null, so uhh... it's probably f*cked now\n", stderr);
	}
#endif

	entState *inState = &in->state;
	entState *retState = &ret->state;
#ifndef NODEBUG
	if (inState->numRefs) {
		fputs("Don't know how to copy over entState refs, I thought we weren't using those!\n", stderr);
	}
#endif
	size_t size = sizeof(slider) * retState->numSliders;
	retState->sliders = (slider*) malloc(size);
	// Maybe iterate assignment instead of this?
	// I think maybe this is a syscall, but even so I'm not sure if it matters for performance.
	memcpy(retState->sliders, inState->sliders, size);

	ret->wires.init(in->wires);
	ret->wiresAdd.init();
	ret->wiresRm.init();
	return ret;
}

gamestate* dup(gamestate *in, list<player> *players) {
	gamestate *ret = (gamestate*) malloc(sizeof(gamestate));

	ret->rand = in->rand;

	// Need to copy several linked lists as well...
	// Note that they may not be in the same order as stuff gets dropped etc,
	// so best bet is probably to copy down the main list and then use the `clone` or whatever
	// to get the other lists and holdees appropriately set up
	ent **dest = &ret->ents;
	ent *prev = NULL;
	for (ent *e = in->ents; e; e = e->ll.n) {
		ent *clone = cloneEnt(e);
		e->clone.ref = clone;
		*dest = clone;
		clone->ll.p = prev;

		dest = &clone->ll.n;
		prev = clone;
	}
	*dest = NULL;

	if (in->rootEnts) ret->rootEnts = in->rootEnts->clone.ref;
	else ret->rootEnts = NULL;

	for (ent *e = ret->ents; e; e = e->ll.n) {
		e->holdRoot = e->holdRoot->clone.ref;

		list<ent*> &wires = e->wires;
		range(i, wires.num) {
			wires[i] = wires[i]->clone.ref;
		}

		// Could be a function taking a property ref but this is fine too
#define cloneCopy(field) if(e->field) e->field = e->field->clone.ref
		cloneCopy(LL.n);
		cloneCopy(LL.p);
		cloneCopy(holdee);
		cloneCopy(holder);
#undef cloneCopy
	}

	if (in->deadTail) {
		fputs("deadTail should be NULL! This is bad!\n", stderr);
	}
	ret->deadTail = NULL;

	ret->flipFlop_death = in->flipFlop_death;
	ret->flipFlop_drop = in->flipFlop_drop;
	ret->flipFlop_pickup = in->flipFlop_pickup;

	players->num = 0;
	players->addAll(*in->players);
	range(i, players->num) {
		player *p = &(*players)[i];
		if (p->entity) {
			p->entity = p->entity->clone.ref;
		}
	}
	ret->players = players;

	return ret;
}

void ent_init() {
	// Used to do something lol
}
void ent_destroy() {}
