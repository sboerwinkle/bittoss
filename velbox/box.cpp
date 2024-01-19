
// Don't forget to include the reified header wherever you plunk this down, please!

static char isWhale(box *b) {
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

static INT small_min(INT a, INT b) {
#ifdef DEBUG
	if (a < 0 || b < 0 || a+b < 0) {
		fprintf(stderr, "bad args for min: %d, %d\n", a, b);
	}
#endif
	// I have no idea if this is better than a naive `min`!
	// I have no idea if `abs` is branchless!
	// But I still did it, and I'll fix it later if it's a problem!!!
	return (a+b-abs(a-b))/2;
}

#define DELT_V_MAX (MAX/VALID_MAX)

// Things that are just touching should not count as intersecting,
// since that's the logic we've got in place for the individual `ent`s in bittoss
static char intersects(box *o, box *n) {
	range(d, DIMS) {
		INT d1 = o->p1[d] - n->p1[d];
		INT vel = o->p2[d] - n->p2[d] - d1;
		// We're going fast enough we'd probably switch signs on the displacement,
		// or worse, wrap around. That's probably a collision on this axis.
		if (vel > DELT_V_MAX || vel < -DELT_V_MAX) continue;

		// I could do sign math here to force d1 positive? IDK if that's worth it
		INT d2 = d1 + vel * VALID_MAX;
		INT r = o->r[d] + n->r[d];
		if ((d1 >= r && d2 >= r) || (d1 <= -r && d2 <= -r)) return 0;
		// Technically this will also register a potential collision on this axis
		// on the frame when their offset switches sign. This is a weird quirk,
		// but it's fine since we're relying on the "actual" collision processing
		// to weed out the occasional false positive
		// (such as diagonal near-misses inside a single frame)
		// Update:
		// I'm very tired, but I think this handles the sign-switch case as well (though not the near-miss case).
		// The cost is that there are some `abs` thrown in (are those expensive?),
		// and I'm not totally sure how it handles relative velocity of INT_MIN (which is a weird case anyway).
		// And also it makes much less sense at first glance.
		// if (abs(d1) >= r && abs(d2) >= r && (d2-d1 > 0) == (d2-INT_MIN > d1-INT_MIN)) return 0;
	}
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
static INT computeValidity(box *b, box *p) {
	int val = p->validity;
	range(d, DIMS) {
		INT v = b->p2[d] - b->p1[d] - p->p2[d] + p->p1[d];
		if (!v) continue;
		INT flip = 1-2*(v<0);
		INT distance = flip*(p->r[d] - b->r[d]) + p->p1[d] - b->p1[d];
		// distance and v should have the same sign, so `t` is non-negative
		INT t = distance / v;
		val = small_min(t, val);
	}
	return val;
}

static void recordIntersect(box *a, box *b) {
#ifdef DEBUG
	if (a == b) {
		fputs("What the ASSSS\n", stderr);
		exit(1);
	}
#endif
	int aNum = a->intersects.num;
	a->intersects.add({.b=b, .i=b->intersects.num});
	b->intersects.add({.b=a, .i=aNum});
}

static void setIntersects(box *n) {
#ifndef NODEBUG
	if (n->intersects.num) { fputs("Intersect assertion 2 failed\n", stderr); exit(1); }
#endif
	n->intersects.add({.b=n, .i=0});

	list<sect> &aunts = n->parent->intersects;
	range(i, aunts.num) {
		box *aunt = aunts[i].b;
		if (!intersects(aunt, n)) continue;
		if (isWhale(aunt)) {
			recordIntersect(n, aunt);
		} else {
			list<box*> &cousins = aunt->kids;
			// Not sure if this really matters, but we reverse the iteration order here.
			// Originally this related to who we chose for child fostering, but now
			// it's just leftover and possibly to have a fixed end condition (micro-optimization).
			// Hopefully it's not bad.
			for (int j = cousins.num - 1; j >= 0; j--) {
				box *test = cousins[j];
				if (intersects(test, n) && test != n) {
					recordIntersect(n, test);
				}
			}
		}
	}
}

static void setIntersects_refresh(box *b) {
	b->intersects.add({.b=b, .i=0});
	// Pretty similar to `setIntersects`, but we can take some shortcuts since we know everyone's being re-evaluated
	list<sect> &aunts = b->parent->intersects;
	range(i, aunts.num) {
		box *aunt = aunts[i].b;
		if (!intersects(aunt, b)) continue;
		if (isWhale(aunt)) {
			// Aunt is assumed valid, since it's from a previous generation.
			recordIntersect(b, aunt);
		} else {
			list<box*> &cousins = aunt->kids;
			range(j, cousins.num) {
				box *test = cousins[j];
				if (test->validity && b != test && intersects(test, b)) {
					recordIntersect(b, test);
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
		ret->validity = parent->validity;
	} else {
		ret->validity = computeValidity(ret, parent);
	}
	setIntersects(ret);

	return ret;
}

static box* getMergeBase(box *guess, box *n, INT minParentR) {
	// Fish can have intersects at other levels, which makes them a bad merge base.
	// Plus they can't be parents anyways.
	if (isWhale(guess)) guess = guess->parent;

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
	for (; ; guess = guess->parent) {
		// The "no more parents" case is a bit weird, but other funcs handle it appropriately I'm told.
		// Note also we could probably be a little lazier that doing the full thorough intersect check,
		//   but it's easier to write this way!
		if (intersects(guess, n) || !guess->parent) return guess;
	}
}

static void addWhale(box *w, const list<box*> *opts) {
	while (opts->num) {
		globalOptionsDest->num = 0;
		range(i, opts->num) {
			box *b = (*opts)[i];
			if (!intersects(b, w)) continue;
			recordIntersect(b, w);
			globalOptionsDest->addAll(b->kids);
		}
		swap(globalOptionsSrc, globalOptionsDest);
		opts = globalOptionsSrc;
	}
}

// Just like `addWhale`, except we skip checking any boxes that haven't been revalidated.
// Depending on how `intersects` gets updated, that check might be moot, but better to wait and see.
static void addWhale_refresh(box *w, const list<box*> *opts) {
	while (opts->num) {
		lookDownDest->num = 0;
		range(i, opts->num) {
			box *b = (*opts)[i];
			if (!b->validity) continue;
			if (!intersects(b, w)) continue;
			recordIntersect(b, w);
			lookDownDest->addAll(b->kids);
		}
		swap(lookDownSrc, lookDownDest);
		opts = lookDownSrc;
	}
}

static void clearIntersects(box *b) {
	list<sect> &intersects = b->intersects;
#ifndef NODEBUG
	if (!intersects.num) {
		fputs("Intersect assertion 0 failed\n", stderr);
		exit(1);
	}
	if (intersects[0].b != b) { fputs("Intersect assertion 1 failed\n", stderr); exit(1); }
#endif
	// We skip the first one because it's going to be ourselves
	for (int i = 1; i < intersects.num; i++) {
		sect &s = intersects[i];
		box *o = s.b;
		int ix = s.i;

		if (ix >= o->intersects.num) {
			fputs("Blam!\n", stderr);
		}
		if (o->intersects[ix].b != b) {
			fputs("well shit\n", stderr);
		}
		// Remove the corresponding intersect (o->intersects[ix]).
		// This reads a little funny at first; remember the `if` will usually be true.
		o->intersects.num--;
		if (o->intersects.num > ix) {
			o->intersects[ix] = o->intersects[o->intersects.num];
			sect &moved = o->intersects[ix];
			if (moved.b->intersects[moved.i].b != o) {
				fputs("well dang\n", stderr);
			}
			moved.b->intersects[moved.i].i = ix;
		}
	}
	intersects.num = 0;
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
		n->validity = level->validity;
	} else {
		n->validity = computeValidity(n, level);
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
		for (int i = sects.num - 1; i >= 0; i--) {
			lookDownSrc->add(sects[i].b);
		}
		optionsSrc = lookDownSrc;
		optionsR = mergeBase->r[0];
	} else if (mergeBase->kids.num) {
		optionsSrc = &mergeBase->kids;
		// We are once again assuming no titanically large whales
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
			if (isWhale(test)) continue;

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
		range(i, intersects.num) {
			box *test = intersects[i].b;
			if (isWhale(test)) continue;

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
	INT minParentR;
	{
		INT max = 0;
		range(d, DIMS) {
			if (n->r[d] > max) max = n->r[d];
		}
		minParentR = max * FIT;
	}
	// `n->validity` will be set better later, as we always wind up in addToAncestor at some point from here.
	// However, we need it set here so `getMergeBase` can check for basic intersections sanely.
	// We could also have getMergeBase call a special `intersects()` which is optimized for that case...
	n->validity = 1;
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
	n->intersects.add({.b=n, .i=0});
}

static void reposition(box *b) {
	box *p = b->parent;
	if (contains(p, b)) {
		b->validity = computeValidity(b, p);
		return;
	}
	p->kids.rm(b);
	insert(p, b); // the first arg here is just a starting point, we're not adding it right back to `p` lol
	if (!p->kids.num) velbox_remove(p);
}

void velbox_update(box *b) {
	reposition(b);
	clearIntersects(b);
	setIntersects(b);
	int x = b->intersects.num;
	for (int i = 1; i < x; i++) {
		addWhale(b, &b->intersects[i].b->kids);
	}
}

void velbox_step(box *b, INT *p1, INT *p2) {
	b->validity--;
	range(i, DIMS) {
		// This check is a little clumbsy.
		// We could exit this loop and resume in a version w/out this check after the first time it triggers;
		// Alternatively, we could skip it completely and always set validity=0.
		// Not sure about the impact of either of those frankly, so we'll leave it as-is for now.
		INT d1 = b->p1[i];
		INT d2 = b->p2[i];
		if (p1[i] != d2 || p2[i] != d2*2-d1) {
			b->validity = 0;
		}

		b->p1[i] = p1[i];
		b->p2[i] = p2[i];
	}
	if (!b->validity) clearIntersects(b);
}

static void stepContainers(box *root) {
	globalOptionsSrc->num = 0;
	globalOptionsSrc->addAll(root->kids);

	while (globalOptionsSrc->num) {
		globalOptionsDest->num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			if (isWhale(b)) continue;

			b->validity--;

			range(i, DIMS) {
				INT tmp = b->p1[i];
				b->p1[i] = b->p2[i];
				// This feels wacky, but I think it's fine?
				b->p2[i] = b->p2[i]*2 - tmp;
			}

			if (!b->validity) clearIntersects(b);

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
			if (b->validity) continue;
#ifdef DEBUG
			if (b->intersects.num) {
				fputs("It should not have had intersects at this point\n", stderr);
			}
#endif
			reposition(b);
			setIntersects_refresh(b);
			if (isWhale(b)) {
				int x = b->intersects.num;
				for (int i = 1; i < x; i++) {
					addWhale_refresh(b, &b->intersects[i].b->kids);
				}
			}
		}

		swap(globalOptionsSrc, globalOptionsDest);
	}
}

box* velbox_getRoot() {
	box *ret = velbox_alloc();
	// This serves as the max validity anything can have, since a box never exceeds its parent.
	// Since most of the effort is put into transient gamestates that only last 5-8 frames depending
	// on the configured server latency, extending the max validity past that is just going to add
	// more potential intersects that are never going to come to pass.
	ret->validity = 5;
	// `kids` can just stay empty for now
	ret->intersects.add({.b=ret, .i=0});
	ret->parent = NULL;
	range(d, DIMS) {
		ret->r[d] = MAX/2 + 1;
		// This *shouldn't* matter, but uninitialized data is never a *good* thing.
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
