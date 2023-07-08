
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

// Things that are just touching should not count as intersecting,
// since that's the logic we've got in place for the individual `ent`s in bittoss
static char intersects(box *o, box *n) {
	range(d, DIMS) {
		INT d1 = o->p1[d] - n->p1[d];
		INT d2 = o->p2[d] - n->p2[d];
		if (d1 < 0) {
			d1 *= -1;
			d2 *= -1;
		}
		INT r = o->r[d] + n->r[d];
		if (d1 >= r && d2 >= r) return 0;
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

static void setIntersects(box *n) {
#ifndef NODEBUG
	if (n->intersects.num) { fputs("Intersect assertion 2 failed\n", stderr); exit(1); }
#endif
	n->intersects.add(n);

	list<box*> &aunts = n->parent->intersects;
	range(i, aunts.num) {
		box *aunt = aunts[i];
		if (!intersects(aunt, n)) continue;
		if (isWhale(aunt)) {
			aunt->intersects.add(n);
			n->intersects.add(aunt);
		} else {
			list<box*> &cousins = aunt->kids;
			// Not sure if this really matters, but we reverse the iteration order here.
			// This should mean that more recently moved boxes (end of `kids`) wind up
			// considered first for placement of new children. Maybe this is good?
			// Hopefully it's not bad.
			for (int j = cousins.num - 1; j >= 0; j--) {
				box *test = cousins[j];
				if (intersects(test, n) && test != n) {
					test->intersects.add(n);
					n->intersects.add(test);
				}
			}
		}
	}
}

static void setIntersects_refresh(box *b) {
	b->refreshed = 1;
	// Pretty similar to `setIntersects`, but we can take some shortcuts since we know everyone's being re-evaluated
	list<box*> &aunts = b->parent->intersects;
	range(i, aunts.num) {
		box *aunt = aunts[i];
		if (!intersects(aunt, b)) continue;
		if (isWhale(aunt)) {
			aunt->intersects.add(b);
			b->intersects.add(aunt);
		} else {
			list<box*> &cousins = aunt->kids;
			range(j, cousins.num) {
				box *test = cousins[j];
				// We can't rule out the whole aunt just because this `test` has been refreshed;
				// the family structure might have been messed with after the iteration order was determined.
				if (!test->refreshed && intersects(test, b)) {
					test->intersects.add(b);
					b->intersects.add(test);
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

static box* mkContainer(box *parent, box *n) {
	box *ret = velbox_alloc();
	parent->kids.add(ret);
	INT r = parent->r[0] / SCALE;
	range(d, DIMS) {
		ret->r[d] = r;
		ret->p1[d] = n->p1[d];
		ret->p2[d] = n->p2[d];
	}
	ret->parent = parent;
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
#ifndef NODEBUG
		if (!guess) {
			fputs("Please don't do that.\n", stderr);
			exit(1);
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
			b->intersects.add(w);
			w->intersects.add(b);
			globalOptionsDest->addAll(b->kids);
		}
		swap(globalOptionsSrc, globalOptionsDest);
		opts = globalOptionsSrc;
	}
}

static void clearIntersects(box *b) {
	list<box*> &intersects = b->intersects;
#ifndef NODEBUG
	if (!intersects.num) { fputs("Intersect assertion 0 failed\n", stderr); exit(1); }
	if (intersects[0] != b) { fputs("Intersect assertion 1 failed\n", stderr); exit(1); }
#endif
	// We skip the first one because it's going to be ourselves
	for (int i = 1; i < intersects.num; i++) {
		// Todo should this be a quickRm (which would be a new function)?
		intersects[i]->intersects.rm(b);
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

static void reposition(box *b) {
	box *p = b->parent;
	if (contains(p, b)) return;
	p->kids.rm(b);
	velbox_insert(p, b); // the first arg here is just a starting point, we're not adding it right back to `p` lol
	if (!p->kids.num) velbox_remove(p);
}

void velbox_update(box *b) {
	reposition(b);
	clearIntersects(b);
	setIntersects(b);
	int x = b->intersects.num;
	for (int i = 1; i < x; i++) {
		addWhale(b, &b->intersects[i]->kids);
	}
}

static void addToAncestor(box *level, box *n, INT minParentR) {
	INT minGrandparentR = SCALE*minParentR;
	while (level->r[0] >= minGrandparentR) {
		level = mkContainer(level, n);
	}
	level->kids.add(n);
	n->parent = level;
}

static char tryLookDown(box *mergeBase, box *n, INT minParentR) {
	const list<box*> *optionsSrc;
	INT optionsR;
	if (mergeBase->parent) {
		optionsSrc = &mergeBase->intersects;
		optionsR = mergeBase->r[0];
	} else if (mergeBase->kids.num) {
		optionsSrc = &mergeBase->kids;
		// TODO we are once again assuming no titanically large whales
		optionsR = (*optionsSrc)[0]->r[0];
	} else {
		// This will should only happen for the first non-root box added
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
		list<box*> &intersects = level->intersects;
		range(i, intersects.num) {
			box *test = intersects[i];
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

void velbox_insert(box *guess, box *n) {
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
	box *mergeBase = getMergeBase(guess, n, minParentR);
	if (!tryLookDown(mergeBase, n, minParentR)) {
		// Please, mergeBase->parent was my father
		lookUp(mergeBase->parent ? mergeBase->parent : mergeBase, n, minParentR);
	}
}

void velbox_refresh(box *root) {
	// The root obviously never needs revalidation.
	// The direct children of the root need their intersects re-evaluated,
	// but they don't need their parentage checked.
	// Maybe I'll just unwrap that half of the loop like a lunatic?
	globalOptionsSrc->num = 0;
	globalOptionsSrc->addAll(root->kids);

	while (1) {
		// Clear intersects of all guys in the list.
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			b->refreshed = 0;
			// Todo optimize to just `num = 1`?
			b->intersects.num = 0;
			b->intersects.add(b);
		}

		// Set the intersects (we take some shortcuts in this version),
		// and also populate the list for the next iteration
		globalOptionsDest->num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			setIntersects_refresh(b);
			globalOptionsDest->addAll(b->kids);
		}

		// Go to next level down, if there is one.
		if (!globalOptionsDest->num) return;
		swap(globalOptionsSrc, globalOptionsDest);

		// Make sure they have valid parentage, and put them in the right place in the tree if not.
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			reposition(b);
		}
	}
}

box* velbox_getRoot() {
	box *ret = velbox_alloc();
	// `kids` can just stay empty for now
	ret->intersects.add(ret);
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