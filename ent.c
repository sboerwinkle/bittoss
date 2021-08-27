#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ent.h"
#include "main.h"

ent *ents = NULL;
//ent *entsToAdd = NULL;
ent *rootEnts = NULL;

byte flipFlop_death = 0;
byte flipFlop_drop = 0;
byte flipFlop_pickup = 0;

static void freeEntState(entState *s);

static void freeEntRef(entRef *r) {
	freeEntState(&r->s);
	free(r);
}

static void freeEntState(entState *s) {
	int i;
	for (i = 0; i < s->numRefs; i++) {
		entRef *x = s->refs[i];
		while (x) {
			entRef *y = x->n;
			freeEntRef(x);
			x = y;
		}
	}
	free(s->sliders);
	free(s->refs);
}

static void flushEntState(entState *s) {
	int i;
	for (i = 0; i < s->numSliders; i++) {
		//sliders are "sticky", i.e. don't reset if left alone
		if (s->sliders[i].max < s->sliders[i].min) continue;
		s->sliders[i].v = (s->sliders[i].max + s->sliders[i].min)/2;
		s->sliders[i].max = INT32_MIN;
		s->sliders[i].min = INT32_MAX;
	}
	for (i = 0; i < s->numRefs; i++) {
		entRef **pp = s->refs + i;
		while (*pp) {
			if ((*pp)->status == -1 || (*pp)->e->dead) {
				entRef *tmp = *pp;
				*pp = tmp->n;
				freeEntRef(tmp);
				continue;
			}
			if ((*pp)->status == 1) (*pp)->status = 0;
			flushEntState(&(*pp)->s);
			pp = &(*pp)->n;
		}
	}
}

void setupEntState(entState *s, int numSliders, int numRefs) {
	s->numSliders = numSliders;
	s->numRefs = numRefs;
	s->refs = (entRef**) calloc(numRefs, sizeof(entRef*));
	s->sliders = (slider*) calloc(numSliders, sizeof(slider));
	flushEntState(s);
}

void flushCtrls(ent *who) {
	who->ctrl.btn1.v = who->ctrl.btn1.v2;
	who->ctrl.btn2.v = who->ctrl.btn2.v2;
	who->ctrl.btn1.v2 = 0;
	who->ctrl.btn2.v2 = 0;

	int i;
	for (i = 0; i < 2; i++) {
		who->ctrl.axis1.v[i] = (who->ctrl.axis1.max[i] + who->ctrl.axis1.min[i]) / 2;
		who->ctrl.axis1.min[i] = axisMaxis;
		who->ctrl.axis1.max[i] = -axisMaxis;
	}
	for (i = 0; i < 3; i++) {
		who->ctrl.look.v[i] = (who->ctrl.look.max[i] + who->ctrl.look.min[i]) / 2;
		who->ctrl.look.min[i] = axisMaxis;
		who->ctrl.look.max[i] = -axisMaxis;
	}
}

static void recursiveHoldRoot(ent *who, ent *r) {
	who->holdRoot = r;
	ent *e;
	for (e = who->holdee; e; e = e->LL.n) {
		recursiveHoldRoot(e, r);
	}
}

static void addRootEnt(ent *e) {
	if (rootEnts) rootEnts->LL.p = e;
	e->LL.n = rootEnts;
	e->LL.p = NULL;
	rootEnts = e;
	recursiveHoldRoot(e, e);
}

void addEnt(ent *e) {
	if (ents) ents->ll.p = e;
	e->ll.n = ents;
	e->ll.p = NULL;
	ents = e;
	/*
	if (entsToAdd) entsToAdd->ll.p = e;
	e->ll.n = entsToAdd;
	e->ll.p = NULL;
	entsToAdd = e;
	*/
	addRootEnt(e);
	//Some basic state that we may want to get in place now, e.g. in case anyone asks if it's okay to schedule a pickup.
	//e->holdRoot = e;
}

static ent *deadTail = NULL; //This linked list is only maintained one-directionally so that the "next" pointer can remain unchanged. Since ents may kill themselves while being looped over, this last part is important.

static void moveRecursive(ent *who, int32_t *dc, int32_t *dv) {
	who->needsCollision = 1;
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

static void fumble(ent *h) {
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
	addRootEnt(h);
}

static void rmRootEnt(ent *e) {
	if (e->LL.p) e->LL.p->LL.n = e->LL.n;
	else rootEnts = e->LL.n;
	if (e->LL.n) e->LL.n->LL.p = e->LL.p;
}

static void pickup(ent *x, ent *y) {
	printf("pickup, ff is %d\n", flipFlop_pickup);
	x->onPickUp(x, y);
	y->onPickedUp(y, x);
	rmRootEnt(y);
	y->LL.n = x->holdee;
	y->LL.p = NULL;
	if (x->holdee) x->holdee->LL.p = y;
	x->holdee = y;
	y->holder = x;
	recursiveHoldRoot(y, x->holdRoot);
}

void killEntNoHandlers(ent *e) {
	while (e->holdee) fumble(e->holdee);
	fumble(e);

	//if (ents == e) ents = e->ll.n;
	if (e->ll.p) e->ll.p->ll.n = e->ll.n;
	else ents = e->ll.n;
	if (e->ll.n) e->ll.n->ll.p = e->ll.p;

	rmRootEnt(e);

	e->ll.p = deadTail;
	deadTail = e;
	e->dead = 1;
}

void crushEnt(ent *e) {
	if (e->crush != sc->F) {
		save_from_C_call(sc);
		e->crush->references++;
		sc->NIL->references++;
		scheme_eval(sc, cons(sc, e->crush, cons(sc, mk_c_ptr(sc, e, 0), sc->NIL)));
	}
	killEntNoHandlers(e);
}

static int whoMoves(ent *a, ent *b, byte axis, int dir) {
#ifndef NODEBUG
	if (a->whoMoves == sc->F) {
		fputs("whoMoves unset\n", stderr);
		return ME;
	}
#endif
	save_from_C_call(sc);
	// TODO ents have an orientation and they can perceive all 3 axes differently
	a->whoMoves->references++;
	sc->NIL->references++;
	scheme_eval(sc, cons(sc, a->whoMoves, cons(sc, mk_c_ptr(sc, a, 0), cons(sc, mk_c_ptr(sc, b, 0), cons(sc, mk_integer(sc, axis), cons(sc, mk_integer(sc, dir), sc->NIL))))));
	if (!is_integer(sc->value)) {
		fputs("That's not an int!\n", stderr);
		return ME;
	}
	return ivalue(sc->value);
}

static int arbitrateMovement(ent *a, ent *b, byte axis, int dir) {
	int A = whoMoves(a, b, axis, dir);
	int B = whoMoves(b, a, axis, -dir);
	if (B == ME) B = HIM;
	else if (B == HIM) B = ME;
	A = A&B;
	return A?A:BOTH;
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
	//We throw in the 3^ to flip ME and HIM, but the only problem is that this injures the NONE and BOTH symbols, so we can't just check for equality here.
	if (movee & 8) return 0; // NONE
	if (movee & 4) { // BOTH
		/*
		//In this case we need to test the forced values and possibly change movee...
		if ((a->forced[axis] == -dir) ^ (b->forced[axis] == dir)) {
			movee = ME + (a->forced[axis] == -dir);
		} else {
		*/
			byte ret = tryCollide(a, b, axis, dir, 1);
			ret |= tryCollide(b, a, axis, -dir, 1);
			return ret;
		//}
	}
	if (movee == ME) {
		return tryCollide(a, b, axis, dir, 0);
	}
	//else it's HIM
	return tryCollide(b, a, axis, -dir, 0);
}

static char doIteration() {
	//TODO: Quadtrees
	char ret = 0;
	ent *i, *j;
	for (i = ents; i; i = i->ll.n) {
		for (j = i->ll.n; j; j = j->ll.n) {
			if (i->needsCollision || j->needsCollision) {
				ret |= checkCollision(i, j);
			}
		}
		i->needsCollision = 0;
	}
	return ret;
}

/*
static void setDeads() {
	int i;
	ent *d, *h;
	for (d = deadTail; d; d = d->prev) {
		if (d->dead) return; // They will all be dead after this
		//Fumble my holds
		for (i = d->numHolds - 1; i >= 0; i--) {
			fumble(d, i);
			//(d->onFumble)(d, i);
			//(d->holds[i]->onFumbled)(d->holds[i]);
		}
		//Fumble myself
		if ( (h = d->holder) ) {
			for (i = h->numHolds - 1; i >= 0; i--) {
				if (h->holds[i] == d) break;
			}
			fumble(h, i);
			//(h->onFumble)(h, i);
			//(d->onFumbled)(d);
		}
		//Mark myself dead
		d->dead = 1;
	}
}
*/

static void clearDeads() {
	ent *d;
	while ( (d = deadTail) ) {
		deadTail = d->ll.p;
		freeEntState(&d->state);
		(d->onFree)(d);
		decrem(sc, d->tick);
		decrem(sc, d->tickHeld);
		decrem(sc, d->crush);
		decrem(sc, d->pushed);
		decrem(sc, d->draw);
		decrem(sc, d->whoMoves);
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

static ent* pinch(ent *e, byte axis, ent *o) {
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
		fumble(f1);
		if (f2) {
			fumble(f2);
			return f2;
		}
		return e;
	}
	if (f2) {
		fumble(f2);
		return f2;
	}
	if (leaf1 != e) {
		fumble(leaf1);
		if (leaf2 != e) {
			fumble(leaf2);
			return leaf2;
		}
		return e;
	}
	if (leaf2 != e) {
		fumble(leaf2);
		return leaf2;
	}
	puts("Pinched to death!");
	crushEnt(e);
	return NULL;
}

static char push_helper(ent *e, ent *prev, byte axis, int dir) {
	if (e->forced[axis]*dir < 0) {
		ent* ret = pinch(e, axis, prev);
		if (!ret) return 1;
		if (e != ret) return 0;
	}
	e->forced[axis] += dir;
	if (e->forced[axis] == dir*3) {
		puts("Safety crush!");
		crushEnt(e);
		return 1;
	}
	e->forcedHoldees[axis] = prev;
	return 0;
}

static void push(ent *e, ent *o, byte axis, int dir) {
	int displacement = (o->center[axis] - e->center[axis])*dir + e->radius[axis] + o->radius[axis];
	if (displacement < 0) printf("Shouldn't get a negative displacement!\n");
	int accel = (o->vel[axis] - e->vel[axis])*dir;
	if (accel < 0) accel = 0;
	//TODO: This guy should get to know the person who was moved, and how.
	//Does it matter that this guy might get called twice, if the other guy gets crushed? Shouldn't matter if he's told the other guy got crushed.
	//I think we need it to be called twice, so the other guy gets to know that he was pushed while not being held.
	o->onPush(e, o, axis, dir, displacement, accel);

	ent *prev = NULL;
	while (1) {
		if (push_helper(e, prev, axis, dir)) break;
		
		//int ret = e->onPushed(e, o, axis, dir, displacement, accel);

		//TODO: Invoke e->onPushed, which should be a tinyscheme pointer
		int ret;
		if (e->pushed == sc->F) {
			ret = r_pass;
		} else {
			save_from_C_call(sc);
			e->pushed->references++;
			sc->NIL->references++;
			pointer Ret = scheme_eval(sc,
				cons(sc, e->pushed,
				cons(sc, mk_c_ptr(sc, e, 0),
				cons(sc, mk_c_ptr(sc, o, 0),
				cons(sc, mk_integer(sc, axis),
				cons(sc, mk_integer(sc, dir),
				cons(sc, mk_integer(sc, displacement),
				cons(sc, mk_integer(sc, accel),
			sc->NIL))))))));
			if (is_integer(Ret)) ret = ivalue(Ret);
			else {
				ret = r_die;
				fputs("'pushed' didn't return an integer!\n", stderr);
			}
		}

		prev = e;
		e = e->holder;
		if (ret == r_die) {
			puts("Chose to die!");
			crushEnt(prev);
			break;
		}
		if (ret == r_pass && e) continue;
		if (ret == r_drop && e) fumble(prev);
		int32_t dc[3], dv[3];
		dc[(axis+1)%3] = dv[(axis+1)%3] = dc[(axis+2)%3] = dv[(axis+2)%3] = 0;
		dc[axis] = displacement*dir;
		dv[axis] = accel*dir;
		moveRecursive(prev, dc, dv);
		break;
	}
}

//TODO: This is a thing for later, but we should probably burn this down and just do recursive ticks
//      (Eliminating the need for e.g. tickType as a fn)
static void _doTick(ent *e) {
	if (e->tick == sc->F) return;
	save_from_C_call(sc);
	e->tick->references++;
	sc->NIL->references++;
	scheme_eval(sc, cons(sc, e->tick, cons(sc, mk_c_ptr(sc, e, 0), sc->NIL)));
}

static void _doTickHeld(ent *e) {
	if (e->tickHeld == sc->F) return;
	save_from_C_call(sc);
	e->tickHeld->references++;
	sc->NIL->references++;
	scheme_eval(sc, cons(sc, e->tickHeld, cons(sc, mk_c_ptr(sc, e, 0), sc->NIL)));
}

static void doTick(ent *e, int type) {
	if (type == 1) _doTickHeld(e);
	else _doTick(e);

	ent *i;
	for (i = e->holdee; i; i = i->LL.n) {
		int ret = e->tickType(e, i);
		if (ret) doTick(i, ret);
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

	int i;
	for (i = 0; i < 3; i++) {
		e->center[i] += (e->d_center_min[i] + e->d_center_max[i]) / 2;
		e->vel[i] += e->d_vel[i];
		e->d_center_min[i] = e->d_center_max[i] = e->d_vel[i] = 0;
	}

	//TODO: Flush orientation, sprite/size
}

//TODO: If this was ordered, onCrush could have access to its whole tree (but not those of others!)
static void flushDeaths() {
	ent *i;
	for (i = ents; i; i = i->ll.n) {
		if (i->dead_max[1^flipFlop_death]) {
			puts("Scripted death!");
			crushEnt(i);
		}
	}
}

/*
static void flushAdds() {
	//Requests for these ents to be killed will have been just missed, so we go through here and clean them up.
	ent *e;
	for (e = entsToAdd; e != ents; e = e->ll.n) {
		recursiveHoldRoot(e, e);
		//Add it as a root ent here?
		e->dead_max[1^flipFlop_death] = 0;
	}
	ents = entsToAdd;
}
*/

//TODO: onFumble[d] could *probably* have access strictly up or down the tree (but no switching!) if we fix the ordering in here.
//Recall that crushing situations may allow for 2 simultaneous fumbles on different branches.
static void flushDrops() {
	ent *i;
	for (i = ents; i; i = i->ll.n) {
		if (i->drop_max[1^flipFlop_drop]) {
			i->drop_max[1^flipFlop_drop] = 0;
			fumble(i);
		}
	}
}

static void flushPickups() {
	ent *i, *next;
	byte ff = 1^flipFlop_pickup;
	for (i = rootEnts; i; i = next) {
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
		if (x != i && !x->dead) pickup(x, i);
		i->holder_max[ff] = NULL;
	}
}

static void flush() {
	//Now we've got to worry about requests issued while that same request is being flushed. Lame.
	flipFlop_death ^= 1;
	flushDeaths();
	//I think deaths has to come first. Otherwise e.g. a pickup could schedule another pickup on something that will be gone by the end of this turn,
	//or a death could schedule an add and a pickup for this guy on that add.
	//flushAdds();
	flipFlop_drop ^= 1;
	//TODO: We're leaking references to unadded ents, and that causes problems. Something could be picked up by an ent that doesn't properly exist yet. Probably the best bet is to review and make damn sure that there's no harm in adding ents immediately when requested.
	//I think our only complaint was that it permits infinitely loopy physics iterations if some jackass writes a stupid script.
	//And face it, slow / loopy scripts are an inevitable hazard. Maybe better to drop the worries in this dep't and just add immediately.
	flushDrops();
	flipFlop_pickup ^= 1;
	flushPickups(); // Pickups has to happen before these others because it's fairly likely that it will influence some of them.
	ent *i;
	for (i = ents; i; i = i->ll.n) {
		flushCtrls(i);
		flushMisc(i);
		flushEntState(&i->state);
	}
}

void doPhysics() {
	flush();
	clearDeads();
	ent *i;
	for (i = rootEnts; i; i = i->LL.n) {
		doTick(i, 2);
	}
	flush();

	for (i = ents; i; i = i->ll.n) {
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
	while(doIteration()) {
		for (i = rootEnts; i; i = i->LL.n) {
			if (i->collisionBuddy) {
				//Do physics-y type things in here
				push(i->collisionLeaf, i->collisionBuddy, i->collisionAxis, i->collisionDir);
				i->collisionBuddy = NULL;
			}
		}
		for (i = ents; i; i = i->ll.n) {
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
}

static void drawEnt(ent *e) {
	if (e->draw == sc->F) {
		fputs("Entity draw function unset\n", stderr);
		return;
	}
	save_from_C_call(sc);
	e->draw->references++;
	sc->NIL->references++;
	scheme_eval(sc, cons(sc, e->draw, cons(sc, mk_c_ptr(sc, e, 0), sc->NIL)));
}

void doDrawing() {
	ent *i;
	//for (i = rootEnts; i; i = i->LL.n) {
	for (i = ents; i; i = i->ll.n) {
		drawEnt(i);
	}
}

pointer ts_draw(scheme *sc, pointer args) {
	if (list_length(sc, args) != 4) {
		fputs("draw requires 4 args\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	pointer E = pair_car(args);
	args = pair_cdr(args);
	pointer r = pair_car(args);
	args = pair_cdr(args);
	pointer g = pair_car(args);
	args = pair_cdr(args);
	pointer b = pair_car(args);

	if (!is_c_ptr(E, 0)) {
		fputs("draw arg 1 must be an ent*\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}
	if (!is_real(r) || !is_real(g) || !is_real(b)) {
		fputs("draw args 2-4 must be reals\n", stderr);
		sc->NIL->references++;
		return sc->NIL;
	}

	ent* e = (ent*) c_ptr_value(E);
	rect(e->center, e->radius, rvalue(r), rvalue(g), rvalue(b));
	sc->NIL->references++;
	return sc->NIL;
}
