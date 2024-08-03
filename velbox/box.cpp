
// Don't forget to include the reified header wherever you plunk this down, please!

static char isLeaf(box *b) {
	return !!b->data;
}

// Todo This is definitely messy stuff, but it's where I am for now.
//      The only other option I can think of is allocating a box,
//      and then completely abusing `kids` and `intersects` to mean something
//      completely different.
//      What I'm trying to avoid is re-allocating the list memory each time
//      one of the functions that needs a local list is entered,
//      and this achieves that, just at the cost that I have to be careful
//      about which methods might potentially be on the callstack when the
//      method which needs global lists is in use.
static list<box*> *globalOptionsDest, *globalOptionsSrc;
static list<box*> *lookDownSrc, *lookDownDest;

static inline void swap(list<box*> *&a, list<box*> *&b) {
	list<box*> *tmp = a;
	a = b;
	b = tmp;
}

// Branchless `min`, assuming `a` and `b` are non-negative.
// Is it "fast"? Idk, but it's branchless.
static INT pmin(INT a, INT b) {
	INT diff = b-a;
	INT a_greater = ((UINT) diff) >> (BITS-1);
	return a + a_greater * diff;
}

static INT pmax(INT a, INT b) {
	INT diff = b-a;
	INT a_greater = ((UINT) diff) >> (BITS-1);
	return b - a_greater * diff;
}

// return 1 or -1, depending on if `a` is non-negative or negative.
static INT int_sign(INT a) {
	/*
	0xxx -> 0001 (non-negatives give 1)
	1xxx -> 1111 (negatives give -1)
	So we shift the sign bit into the 1's position (to get 0 or 1, call this `x`),
	then compute our return value as `1 - x*2`
	*/
	return 1 - (((UINT) a) >> (BITS-1))*2;
}

// Things that are just touching should not count as intersecting,
// since that's the logic we've got in place for the individual `ent`s in bittoss
static char intersects(box *o, box *n, UINT *start_ptr, UINT *end_ptr) {
	// So our first stab at making this *beautiful* involves getting intersect
	// start/end times. Once we have those, we can restrict which ones
	// children have to look at, and we can worry about propagating them
	// down to children only as needed.
	UINT start = 0;
	UINT end = pmin(o->t_end - o->t_now, n->t_end - n->t_now);
	range(d, DIMS) {
		INT d1 = o->p1[d] - n->p1[d];
		INT vel = o->p2[d] - n->p2[d] - d1;
		INT sgn = int_sign(vel);
		INT r = o->r[d] + n->r[d];

		d1 = -d1*sgn - r;
		vel *= sgn;

		// If distance is non-negative, we can do normal math to figure out when we'll hit it.
		// If distance is negative, either we're inside or we've missed.
		// If we haven't missed, we can get the exit time as a UINT without too much problem
		if (d1 >= 0) {
			// We're not touching (yet), but headed the right way
			if (!vel) return 0; // We're not touching
			INT t1 = d1 / vel;
			start = pmin(start, t1);
		} else if (d1 <= -2*r) {
			// We already passed it, we know we won't intersect again
			// without the parent intersect being re-created.
			return 0;
		} else {
			// Else, we're currently inside.
			if (!vel) continue; // We intersect forever on this axis, current start/end are fine
		}
		// Cases which can hit here are when vel!=0, and position is either "ahead" or "inside of".
		UINT t2 = (UINT)(d1 + 2*r + vel - 1) / (UINT)vel;
		if (t2 < end) end = t2;

		if (start >= end) return 0;
	}
	*start_ptr = start;
	*end_ptr = end;
	return 1;
}

static char contains(box *p, box *b) {
	range(d, DIMS) {
		INT tolerance = p->r[d] - b->r[d];
		INT d1 = abs(p->p1[d] - b->p1[d]);
		INT d2 = abs(p->p2[d] - b->p2[d]);
		if (d1 > tolerance || d2 > tolerance) return 0;
	}
	return 1;
}

// This assumes that `p` does in fact contain `b`. Makes the zero-velocity case simpler.
static void computeValidity(box *b, box *p) {
	INT val = p->t_end - p->t_now;
	if (p->parent) {
		range(d, DIMS) {
			INT v = b->p2[d] - b->p1[d] - p->p2[d] + p->p1[d];
			if (!v) continue;
			INT flip = int_sign(v);
			INT distance = flip*(p->r[d] - b->r[d]) + p->p1[d] - b->p1[d];
			// distance and v should have the same sign, so `t` is non-negative
			INT t = distance / v;
			val = pmin(t, val);
		}
	} else {
		// This has to be a separate branch partly because the root's children
		// may *not* be contained by the root (numerically at least; logically we make an exception),
		// and this messes with the above math.
		// The other reason we need a separate branch is because we have a concern here that lower levels
		// do not - namely that potential intersects can wrap around and hit us *again*.
		// Considering a single sibling, and the validity constraint it imposes:
		//  - If we're non-overlapping on an axis, we can always take the time-to-overlap (even if this is quite large)
		//  - The usual `intersects` check will catch the first intersect (if any) that occurs within a cube of size MAX.
		//    As such, we can also take the time it would take to reach the edge of such cube (so basically the fastest axis).
		list<box*> &kids = p->kids;
		int num = kids.num;
		range(k_ix, num) {
			box *o = kids[k_ix];
			// Can't start at zero due to a quirk of our usage of pmax,
			// but doesn't matter since this would result in max validity anyway
			INT max_v = 1;
			INT max_clear_time = 0;
			range(d, DIMS) {
				INT v = b->p2[d] - b->p1[d] - o->p2[d] + o->p1[d];
				INT s = int_sign(v);
				v *= s;
				// We probably know the radius for sure based on where we are in the tree,
				// but I feel it's more future-proof than a constant expression w/ MAX.
				// That said, we don't need to mess around with indexing into `r` or anything.
				INT r = 2*b->r[0];
				INT dist = s*(o->p1[d] - b->p1[d])  -  r;
				if (!v) {
					// If we're stationary and outside it's "lane",
					// this box can never cause invalidity.
					if (dist >= 0 || dist <= -r) goto next_box;
					// If we're stationary and inside, then our clear_time and v are zero,
					// so no need to bother checking.
					continue;
				}
				if (dist > 0 || dist <= -r) {
					// Some weirdness here, because we want to avoid a clear_time > MAX.
					// We know v is at least 1, and if we make it at least 2 then we know
					// that even with `dist` being a UINT we will get a quotient < MAX.
					// This underestimates other clear times by a bit, but not *too* much.
					// Our alternative is another branch to deal with the "negative dist" case.
					max_clear_time = pmax(max_clear_time, ((UINT)dist) / ((UINT)v + 1));
				}

				// pmax behaves so long as its inputs are within a range of size MAX.
				// Typically we use [0, MAX], but in this case we use
				// [1, MAX+1] (MAX+1 == MIN). This means we succesfully treat MIN
				// as the max velocity - it's the only number we can't make positive
				// because of 2's complement, so treating it as big is imporatnt.
				max_v = pmax(max_v, v);
			}
			{ // Scope here just placates compiler about jump to `next_box:`
				INT min_wrap_time = ((UINT)MAX) / ((UINT)max_v);
				INT limit = pmax(min_wrap_time, max_clear_time);
				// As a special case, if the other box hasn't had validity re-done yet,
				// this condition will never pass (and it's up to that box to make sure
				// this pair doesn't have some future collision to worry about)
				if (limit < val && (UINT)limit < o->t_end - o->t_now) val = limit;
			}
			next_box:;
		}
	}
	b->t_now = 0;
	b->t_end = val;
	b->t_next = val;
}

static void moveIntersectHalf(box *b, int ix, int dest) {
	// For some cases this is actually required
	// (e.g. item at `dest` is invalid, but we read item at `ix`, and dest==ix).
	// It may represent an optimization anyway, so we always check for this early exit.
	if (ix == dest) return;
	sect &s = b->intersects[ix];
	b->intersects[dest] = s;
#ifdef DEBUG
	if (s.b->intersects[s.i].i != ix) {
		fputs("Intersect halves not aligned\n", stderr);
		exit(1);
	}
#endif
	s.b->intersects[s.i].i = dest;
}

static void recordIntersect(box *a, box *b, UINT start, UINT end) {
#ifdef DEBUG
	if (a == b) {
		fputs("What the ASSSS\n", stderr);
		exit(1);
	}
#endif
	char active = 0;
	if (!start) {
		// If start is 0 (the lowest possible value),
		// intersect is active, which is indicated by `t_next == t_end` on the intersect.
		active = 1;
		start = end;
	}
	// In either case, we've got something to schedule - either an add or a remove
	a->t_next = a->t_now + pmin(start, a->t_next - a->t_now);

	// `b` doesn't get any actionable times (it's up to `a` to handle that),
	// but it still needs to not cause overflow when computing remaining time to update
	UINT b_time = b->t_now + MAX;

	if (active) {
		int aActive = a->activeIntersects;
		int aNum = a->intersects.num;
		int bActive = b->activeIntersects;
		int bNum = b->intersects.num;
		a->intersects.add();
		b->intersects.add();
		if (aActive != aNum) {
			moveIntersectHalf(a, aActive, aNum);
		}
		if (bActive != bNum) {
			moveIntersectHalf(b, bActive, bNum);
		}

		UINT a_time = a->t_now + end;
		a->intersects[aActive] = {.b=b, .i=bActive, .t_next=a_time, .t_end=a_time};
		b->intersects[bActive] = {.b=a, .i=aActive, .t_next=b_time, .t_end=b_time};

		a->activeIntersects++;
		b->activeIntersects++;
	} else {
		int aNum = a->intersects.num;
		a->intersects.add({.b=b, .i=b->intersects.num, .t_next=a->t_now + start, .t_end=a->t_now + end});
		b->intersects.add({.b=a, .i=aNum, .t_next=b_time, .t_end=b_time + 1});
	}
}

static void setIntersects_refresh(box *n) {
#ifdef DEBUG
	if (n->intersects.num) { fputs("Intersect assertion 2 failed\n", stderr); exit(1); }
	if (n->t_now != 0) { fputs("t_now was expected to be 0 here\n", stderr); exit(1); } // mostly for t_foo on the self-intersect
	if (n->t_end-n->t_now < 0) { fputs("Bad validity\n", stderr); exit(1); }
#endif
	// We sometimes check if `n->intersects.num` is non-zero to determine if something is eligible for
	// intersect checking (mostly to avoid checking things in both directions when there's bulk updates).
	// This effectively marks the box as eligible for intersects, and then we check it against other
	// eligible boxes.
	n->intersects.add({.b=n, .i=0, .t_next=MAX, .t_end=MAX});
	n->activeIntersects = 1;

	list<sect> &aunts = n->parent->intersects;
	int num = n->parent->activeIntersects;
	range(i, num) {
#ifdef DEBUG
		if (!aunt->intersects.num) {
			fputs("Aunt should be in a valid state\n", stderr);
			exit(1);
		}
#endif

		box *aunt = aunts[i].b;
		UINT start, end;
		if (!intersects(aunt, n, &start, &end) && aunt->parent) continue;
		if (isLeaf(aunt)) {
			// Order is important here, non-leaf box is responsible for updating this intersect.
			// We expect the leaf to have more, on balance, so it's more efficient to look at it this way
			recordIntersect(n, aunt, start, end);
		} else {
			list<box*> &cousins = aunt->kids;
			// Todo: is reverse iteration faster here?
			range(j, cousins.num) {
				box *test = cousins[j];
				if (test->intersects.num && intersects(test, n, &start, &end) && test != n) {
					// It's okay if `n` or `test` is a leaf, since they're definitely on the same level
					recordIntersect(n, test, start, end);
				}
			}
		}
	}
}

#define ALLOC_SIZE 100
static list<box*> boxAllocs, freeBoxes;

box* velbox_alloc() {
	if (freeBoxes.num == 0) {
		// Todo This initialization is kinda slow... Maybe chuck it in another thread or something?
		box *newAlloc = new box[ALLOC_SIZE];
		boxAllocs.add(newAlloc);
		range(i, ALLOC_SIZE) {
			newAlloc[i].kids.init();
			newAlloc[i].intersects.init();
			freeBoxes.add(&newAlloc[i]);
		}
	}
	box *ret = freeBoxes[--freeBoxes.num];
	ret->intersects.num = 0;
	ret->activeIntersects = 0;
	ret->kids.num = 0;
	ret->data = NULL;
	return ret;
}

static box* mkContainer(box *parent, box *n, char aligned) {
	box *ret = velbox_alloc();
	parent->kids.add(ret);
	INT r = parent->r[0] / SCALE;
	range(d, DIMS) {
		ret->r[d] = r;
		ret->p1[d] = n->p1[d];
		ret->p2[d] = n->p2[d];
	}
	ret->parent = parent;
	if (aligned) {
		ret->t_now = parent->t_now;
		ret->t_next = parent->t_next;
		ret->t_end = parent->t_end;
	} else {
		computeValidity(ret, parent);
	}
	setIntersects_refresh(ret);

	return ret;
}

static box* getMergeBase(box *guess, box *n, INT minParentR) {
	// Leafs can have intersects at other levels, which makes them a bad merge base.
	// Plus they can't be parents anyways.
	if (isLeaf(guess)) guess = guess->parent;

	// Go up only until we can confidently say
	// we can list all potential parents (as intersects)

	// This happens as soon as we intersect `n`,
	// but we also skip up until we're the right height because it simplifies some later logic,
	// and we'd have to climb that high anyway.

	while (guess->r[0] < minParentR) {
		guess = guess->parent;
#ifdef DEBUG
		if (!guess) {
			fputs("Looks like we're adding a box which is far too large.\n", stderr);
		}
#endif
	}
	UINT start, end;
	for (; ; guess = guess->parent) {
		// The "no more parents" case is a bit weird, but other funcs handle it appropriately I'm told.
		// Note also we could probably be a little lazier that doing the full thorough intersect check,
		//   but it's easier to write this way!
		if (intersects(guess, n, &start, &end) || !guess->parent) return guess;
	}
}

static void addLeaf(box *l, const list<box*> *opts) {
	UINT start, end;
	while (opts->num) {
		lookDownDest->num = 0;
		range(i, opts->num) {
			box *b = (*opts)[i];
			if (!b->intersects.num || !intersects(b, l, &start, &end)) continue;
			// Leaf *must* be second. `handledScheduledIntersects` may be above us in the stack operating on `l`,
			// so anything we schedule on `l` here might not properly affect its `t_next`.
			// Intersects are only scheduled on the first `box*` provided, which is why `l` must be second.
			recordIntersect(b, l, start, end);
			if (!start) lookDownDest->addAll(b->kids);
		}
		swap(lookDownSrc, lookDownDest);
		opts = lookDownSrc;
	}
}

// Intersects are recorded in pairs; this removes one half of a pair.
// This requires moving some other intersect usually, so that pair gets
// updated so they are still correctly linked.
static void removeIntersectHalf(box *b, int ix, char active) {
#ifdef DEBUG
	if (!ix) {
		fputs("Shouldn't be removing first intersect\n", stderr);
		exit(1);
	}
	if (b->intersects.num == 1) {
		fputs("Shouldn't be removing last intersect?\n", stderr);
		exit(1);
	}
	if (ix >= b->intersects.num) {
		fputs("Invalid index to `removeIntersectHalf`\n", stderr);
		exit(1);
	}
#endif
	if (active) {
		b->activeIntersects--;
		int numActive = b->activeIntersects;
		moveIntersectHalf(b, numActive, ix);
		ix = numActive;
	}
	b->intersects.num--;
	moveIntersectHalf(b, b->intersects.num, ix);
}

static void removeExpiredIntersect(box *b, int ix) {
	sect &s = b->intersects[ix];
	removeIntersectHalf(s.b, s.i, 1);
	removeIntersectHalf(b, ix, 1);
}

static void handleIntersectStart(box *b, box *other) {
#ifdef DEBUG
	if (b->intersects.num || other->intersects.num) {
		fputs("`handleIntersectStart` args shouldn't be expired\n", stderr);
		exit(1);
	}
#endif
	if (isLeaf(other)) {
		addLeaf(other, &b->kids);
	} else if (isLeaf(b)) {
		addLeaf(b, &other->kids);
	} else {
		UINT start, end;
		range(k1_ix, b->kids.num) {
			box *k1 = b->kids[k1_ix];
			if (!k1->intersects.num || !intersects(k1, other, &start, &end)) continue;
			range(k2_ix, other->kids.num) {
				box *k2 = b->kids[k2_ix];
				if (!k2->intersects.num) continue;
				if (intersects(k1, k2, &start, &end)) {
					// Either of these could be a leaf,
					// doesn't matter since they're at the same level
					recordIntersect(k1, k2, start, end);
					if (!start) handleIntersectStart(k1, k2);
				}
			}
		}
	}
}

static UINT promoteIntersect(box *b, int ix) {
	// Not a reference like we do elsewhere, an actual local copy
	sect s = b->intersects[ix];

	box *o = s.b;

	int otherIx = o->activeIntersects;
	o->activeIntersects++;
	moveIntersectHalf(o, otherIx, s.i);

	int myIx = b->activeIntersects;
	b->activeIntersects++;
	moveIntersectHalf(b, myIx, ix);

	UINT otherTime = o->t_now + MAX;
	b->intersects[myIx] = {.b=o, .i=otherIx, .t_next=s.t_end, .t_end=s.t_end};
	o->intersects[otherIx] = {.b=b, .i=myIx, .t_next=otherTime, .t_end=otherTime};

	handleIntersectStart(b, s.b);
	return s.t_end;
}

static void clearIntersects(box *b) {
	list<sect> &intersects = b->intersects;
	int total = intersects.num;
#ifndef NODEBUG
	if (!total) {
		fputs("Intersect assertion 0 failed\n", stderr);
		exit(1);
	}
	if (intersects[0].b != b) { fputs("Intersect assertion 1 failed\n", stderr); exit(1); }
#endif
	// Not sure if this matters, but we're removing these in reverse order in the hope that we
	// slightly increase the ratio of other boxes that can take a faster path in `removeIntersectHalf`
	// (since inactive intersects will be at the end of their lists as well).
	int activeNum = b->activeIntersects;
	int i = total-1;
	for (; i >= activeNum; i--) {
		sect &s = intersects[i];
		box *o = s.b;
		int ix = s.i;
#ifdef DEBUG
		if (ix >= o->intersects.num) {
			fputs("Blam!\n", stderr);
		}
		if (o->intersects[ix].b != b) {
			fputs("well shit\n", stderr);
		}
#endif
		removeIntersectHalf(o, ix, 0);
	}
	// We skip the first one because it's going to be ourselves
	for (; i >= 1; i--) {
		sect &s = intersects[i];
		box *o = s.b;
		int ix = s.i;
#ifdef DEBUG
		if (ix >= o->intersects.num) {
			fputs("Blam!\n", stderr);
		}
		if (o->intersects[ix].b != b) {
			fputs("well shit\n", stderr);
		}
#endif
		removeIntersectHalf(o, ix, 1);
	}
	intersects.num = 0;
	b->activeIntersects = 0;
}

void velbox_remove(box *o) {
	do {
		clearIntersects(o);
		box *parent = o->parent;
		parent->kids.rm(o);
		freeBoxes.add(o);
		o = parent;
	} while (!o->kids.num && o->parent);
}

static void addToAncestor(box *level, box *n, INT minParentR) {
	INT minGrandparentR = SCALE*minParentR;
	if (level->r[0] >= minGrandparentR) {
		level = mkContainer(level, n, 0);
		while (level->r[0] >= minGrandparentR) {
			level = mkContainer(level, n, 1);
		}
		n->t_now = level->t_now;
		n->t_next = level->t_next;
		n->t_end = level->t_end;
	} else {
		computeValidity(n, level);
	}
	level->kids.add(n);
	n->parent = level;
}

static char tryLookDown(box *mergeBase, box *n, INT minParentR) {
	const list<box*> *optionsSrc;
	INT optionsR;
	if (mergeBase->parent) {
		lookDownSrc->num = 0;
		list<sect> &sects = mergeBase->intersects;
		range (i, mergeBase->activeIntersects) {
			if (sects[i].t_next == sects[i].t_end) {
				lookDownSrc->add(sects[i].b);
			}
		}
		optionsSrc = lookDownSrc;
		optionsR = mergeBase->r[0];
	} else if (mergeBase->kids.num) {
		optionsSrc = &mergeBase->kids;
		// We are once again assuming no titanically large leaves
		optionsR = (*optionsSrc)[0]->r[0];
	} else {
		// This will (should) only happen for the first non-root box added
		return 0;
	}

	box* fosterParent = NULL;
	char fosterStale, finalLevel;

	do {
		lookDownDest->num = 0;
		fosterStale = 1;

		INT nextR = optionsR / SCALE;
		finalLevel = nextR < minParentR;
		INT fosterR = optionsR - nextR;
		range(i, optionsSrc->num) {
			box *test = (*optionsSrc)[i];
			if (isLeaf(test)) continue;

			char foster = 1;
			range(d, DIMS) {
				INT d1 = abs(test->p1[d] - n->p1[d]);
				INT d2 = abs(test->p2[d] - n->p2[d]);
				INT ancestorR = optionsR - n->r[d];
				if (d1 > ancestorR || d2 > ancestorR) goto fail;
				if (d1 > fosterR || d2 > fosterR) foster = 0;
			}

			if (finalLevel || (foster && fosterStale)) {
				fosterParent = test;
				if (finalLevel) break;
				fosterStale = 0;
			}
			lookDownDest->addAll(test->kids);

			fail:;
		}
		swap(lookDownSrc, lookDownDest);
		optionsSrc = lookDownSrc;
		optionsR = nextR;
	} while(!finalLevel);

	if (!fosterParent) return 0;
	addToAncestor(fosterParent, n, minParentR);
	return 1;
}

static void lookUp(box *level, box *n, INT minParentR) {
	for (; level->parent; level = level->parent) {
		INT r = level->r[0];
		r = r - r / SCALE;
		list<sect> &intersects = level->intersects;
		range(i, level->activeIntersects) {
			sect &s = intersects[i];
			box *test = s.b;
			if (s.t_next != s.t_end || isLeaf(test)) continue;

			range(d, DIMS) {
				INT d1 = abs(test->p1[d] - n->p1[d]);
				INT d2 = abs(test->p2[d] - n->p2[d]);
				if (d1 > r || d2 > r) goto fail;
			}

			// Other option here would be to add to a list,
			// which then we would sort somehow
			level = test;
			goto done;

			fail:;
		}
	}
	done:;

	addToAncestor(level, n, minParentR);
}

static void insert(box *guess, box *n) {
	// It is IMPORTANT to note that `n` must be small enough to have at least one nested level
	// below the apex. The apex is weird enough as it is, this condition reduces the number of
	// edge cases we have to consider.
	// (Note I'm not sure that's true any more, but why would you want boxes that apocalyptically big?)

	// Todo Maybe provide a faster path in the case where it fits fine in `guess->parent`,
	//      i.e. when it hasn't changed much from last time?
	INT max = 0;
	range(d, DIMS) {
		max = pmax(max, n->r[d]);
	}
	INT minParentR = max * FIT;
	// `t_end` / `t_next` will be set better later, as we always wind up in addToAncestor at some point from here.
	// However, we need them set here so `getMergeBase` can check for basic intersections sanely.
	// We could also have getMergeBase call a special `intersects()` which is optimized for that case...
	n->t_now = 0;
	n->t_end = 1;
	box *mergeBase = getMergeBase(guess, n, minParentR);
	if (!tryLookDown(mergeBase, n, minParentR)) {
		// Please, mergeBase->parent was my father
		lookUp(mergeBase->parent ? mergeBase->parent : mergeBase, n, minParentR);
	}
}

void velbox_insert(box *guess, box *n) {
	insert(guess, n);
#ifndef NODEBUG
	if (n->intersects.num) {
		fputs("Intersect assertion 3 failed\n", stderr);
		exit(1);
	}
#endif
	// Todo would `velbox_single_refresh` be better here?
	// As-is, we basically give it the minimal valid state and
	// force it to expire on the next step.
	n->t_now = 0;
	n->t_next = n->t_end = 1;
	n->intersects.add({.b=n, .i=0, .t_next=1, .t_end=1});
	n->activeIntersects = 1;
}

static void reposition(box *b) {
	box *p = b->parent;
	if (contains(p, b)) {
		computeValidity(b, p);
		return;
	}
	p->kids.rm(b);
	insert(p, b); // the first arg here is just a starting point, we're not adding it right back to `p` lol
	if (!p->kids.num) velbox_remove(p);
}

// The "bulk update" is done by performing velbox_update on multiple boxes,
// followed by velbox_single_refresh on those same boxes. Avoids checking
// collisions between old+new versions in this manner.
void velbox_update(box *b) {
	reposition(b);
	clearIntersects(b);
}

void velbox_single_refresh(box *b) {
	setIntersects_refresh(b);
	int x = b->activeIntersects;
	for (int i = 1; i < x; i++) {
		sect &s = b->intersects[i];
		addLeaf(b, &s.b->kids);
	}
}

static void handleScheduledIntersects(box * const b) {
	UINT now = b->t_now;
	INT next_next = b->t_end - now;

	for (int i = 1; i < b->intersects.num; i++) {
		sect &s = b->intersects[i];
#ifdef DEBUG
		if ((s.t_end == s.t_next) != (i < b->activeIntersects)) {
			fputs("Mismatch between intersect \"active\" metrics\n", stderr);
			exit(1);
		}
#endif
		if (s.t_next == now) {
			if (s.t_end == now) {
				removeExpiredIntersect(b, i);
				// Only one half of each intersect pair actually gets scheduled,
				// so since we're scheduled our partner shouldn't need a t_next update
				// (and we are already doing a t_next update).
				i--;
				continue;
			}

			// Else, intersect window is starting.
			UINT t_end = promoteIntersect(b, i);
			// Any number of active/inactive intersects may be added by the above (if we're a leaf),
			// not to mention that `s` had to be moved into the "active" segment.
			// We should be fine though; any added intersects aren't ones we have to schedule
			// (because we were careful about that), and any intersects that got moved to make
			// space for other intersects are (at worst) just going to be harmlessly re-processed here.
			next_next = pmin(next_next, t_end - now);
		} else {
			// Intersect isn't ready yet, but could always be next
			next_next = pmin(next_next, s.t_next - now);
		}
	}

	b->t_next = now + next_next;
}

void velbox_step(box *b, INT *p1, INT *p2) {
	b->t_now++;
	range(i, DIMS) {
		// This check is a little clumbsy.
		// We could exit this loop and resume in a version w/out this check after the first time it triggers;
		// Alternatively, we could skip it completely and always set validity=0.
		// Not sure about the impact of either of those frankly, so we'll leave it as-is for now.
		INT d1 = b->p1[i];
		INT d2 = b->p2[i];
		if (p1[i] != d2 || p2[i] != d2*2-d1) {
			b->t_next = b->t_end = b->t_now;
		}

		b->p1[i] = p1[i];
		b->p2[i] = p2[i];
	}
	if (b->t_end == b->t_now) clearIntersects(b);
}

static void stepContainers(box *root) {
	globalOptionsSrc->num = 0;
	globalOptionsSrc->addAll(root->kids);

	while (globalOptionsSrc->num) {
		globalOptionsDest->num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			// For leaves, this stuff is already handled in `velbox_step`
			if (isLeaf(b)) continue;

			range(i, DIMS) {
				INT tmp = b->p1[i];
				b->p1[i] = b->p2[i];
				// This feels wacky, but I think it's fine?
				b->p2[i] = b->p2[i]*2 - tmp;
			}

			b->t_now++;
			if (b->t_now == b->t_end) clearIntersects(b);

			globalOptionsDest->addAll(b->kids);
		}
		swap(globalOptionsSrc, globalOptionsDest);
	}
}

void velbox_refresh(box *root) {
	// The first pass is just updating all non-fish positions and counting down the validity counter
	// If validity hits 0, we clearIntersects.
	stepContainers(root);

	// The root never needs revalidation or intersect checking.
	globalOptionsSrc->num = 0;
	globalOptionsSrc->addAll(root->kids);

	while (globalOptionsSrc->num) {

		// Set the intersects (we take some shortcuts in this version),
		// and also populate the list for the next iteration
		globalOptionsDest->num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			globalOptionsDest->addAll(b->kids);
			if (b->t_now != b->t_next) continue;

			if (b->t_now == b->t_end) {
#ifdef DEBUG
				if (b->intersects.num) {
					fputs("It should not have had intersects at this point\n", stderr);
				}
#endif
				reposition(b);
				setIntersects_refresh(b);
				if (isLeaf(b)) {
					int x = b->activeIntersects;
					for (int i = 1; i < x; i++) {
						addLeaf(b, &b->intersects[i].b->kids);
					}
				}
			} else {
				// Otherwise, t_now is t_next (but not t_end),
				// meaning we have one or more scheduled intersects to "activate"
				handleScheduledIntersects(b);
			}
		}

		swap(globalOptionsSrc, globalOptionsDest);
	}
}

box* velbox_getRoot() {
	box *ret = velbox_alloc();
	// This serves as the max validity anything can have, since a box never exceeds its parent.
	ret->t_now = 0;
	ret->t_next = ret->t_end = MAX;
	// `kids` can just stay empty for now
	ret->intersects.add({.b=ret, .i=0, .t_next=MAX, .t_end=MAX});
	ret->activeIntersects = 1;
	ret->parent = NULL;
	range(d, DIMS) {
		// This used to be MAX/SCALE, but I think that was a transcription error from when
		// SCALE was 2. I think this should be MAX/2 regardless of SCALE.
		ret->r[d] = MAX/2 + 1;
		// This probably affects something given that we try to avoid special "root" checks wherever possible -
		// it should still make a valid state for any values here, but we really need it to be valid _and_
		// consistent across instances.
		ret->p1[d] = ret->p2[d] = 0;
	}
	return ret;
}

void velbox_freeRoot(box *r) {
	if (r->kids.num) {
		fputs("Tried to return a root box with children! Someone isn't cleaning up properly!\n", stderr);
		exit(1);
	}
	freeBoxes.add(r);
}

// This does not set `parent` or `validity` on the returned `box*`.
static box* cloneBox(box *b) {
	box *ret = velbox_alloc();

#ifdef DEBUG
	if (!b->intersects.num) { fputs("Cloned box should have intersects\n", stderr); exit(1); }
	if (b->intersects[0].b != b) { fputs("Cloned box's first intersect should be self\n", stderr); exit(1); }
	if (ret->intersects.num) { fputs("Fail 0\n", stderr); exit(1); }
#endif
	UINT t = b->intersects[0].t_next;
	// Since we're using the first intersect to track clones temporarily,
	// we can't use the regular logic to copy it over. Add it manually.
	ret->intersects.add({.b=ret, .i=0, .t_next=t, .t_end=t});
	b->intersects[0].b = ret;

	ret->activeIntersects = b->activeIntersects;
	range(d, 3) {
		ret->p1[d] = b->p1[d];
		ret->p2[d] = b->p2[d];
		ret->r[d] = b->r[d];
	}
	ret->t_now = b->t_now;
	ret->t_next = b->t_next;
	ret->t_end = b->t_end;

	if (b->data) {
		ret->data = copyBoxData(b, ret);
	} else {
		list<box*> &oldKids = b->kids;
		list<box*> &newKids = ret->kids;
		range(i, oldKids.num) {
			box *clone = cloneBox(oldKids[i]);
			newKids.add(clone);
			clone->parent = ret;
		}
	}

	return ret;
}

static void copyIntersects(box *b) {
	list<sect> &intersects = b->intersects;
	list<sect> &i2 = b->intersects[0].b->intersects;
#ifdef DEBUG
	if (i2.num != 1) { fputs("`i2` should have length exactly 1 here\n", stderr); exit(1); }
#endif
	// Skip the first intersect, as it always points to the cloned box here.
	// Copy the other intersects over, translating the references to the cloned boxes.
	for (int i = 1; i < intersects.num; i++) {
		const sect old = intersects[i];
		i2.add({.b = old.b->intersects[0].b, .i=old.i, .t_next=old.t_next, .t_end=old.t_end});
	}
	// Rinse and repeat for children. This could maybe be optimized
	// if we made a flat list of all boxes, since we iterate them
	// a few times in the course of the "clone" operation.
	list<box*> &kids = b->kids;
	range(i, kids.num) copyIntersects(kids[i]);
}

static void resetSelf(box *b) {
	b->intersects[0].b = b;
	list<box*> &kids = b->kids;
	range(i, kids.num) resetSelf(kids[i]);
}

box* velbox_clone(box *oldRoot) {
	box *ret = cloneBox(oldRoot);
	ret->parent = NULL;
	copyIntersects(oldRoot);
	resetSelf(oldRoot);
	return ret;
}

void velbox_init() {
	boxAllocs.init();
	freeBoxes.init();

	globalOptionsSrc = new list<box*>();
	globalOptionsSrc->init();
	globalOptionsDest = new list<box*>();
	globalOptionsDest->init();
	lookDownSrc = new list<box*>();
	lookDownSrc->init();
	lookDownDest = new list<box*>();
	lookDownDest->init();
}

void velbox_destroy() {
	range(i, boxAllocs.num) {
		box *chunk = boxAllocs[i];
		range(j, ALLOC_SIZE) {
			chunk[j].kids.destroy();
			chunk[j].intersects.destroy();
		}
		delete[] chunk;
	}
	boxAllocs.destroy();
	freeBoxes.destroy();
	globalOptionsSrc->destroy();
	delete globalOptionsSrc;
	globalOptionsDest->destroy();
	delete globalOptionsDest;
	lookDownSrc->destroy();
	delete lookDownSrc;
	lookDownDest->destroy();
	delete lookDownDest;
}
