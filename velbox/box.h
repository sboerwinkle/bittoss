
// If you want to define these using GCC args, that's fine -
// an alternative is to "wrap" this file (and the .cpp file)
// with files that just #define all this and then #include
// the "original" file. This should make it easier to target
// the scopes on the definitions.

#ifndef INT
#define INT int32_t
#endif

/*
#ifndef UINT
#define UINT uint32_t
#endif
*/

#ifndef BITS
#define BITS 32
#endif

#define MAX ((((INT) 1) << (BITS - 1)) - (INT)1)

#ifndef DIMS
#define DIMS 3
#endif

struct box {
	INT r; // Should not exceed INT_MAX/2 (so it can be safely added to itself)
	INT pos[DIMS];
	INT vel[DIMS];

	// Time is measured according to some global clock;
	// `begin` and `end` only really have meaning when considered vs some reference time.
	// So long at the root `box` doesn't encompass too large a time interval,
	// we shouldn't have to worry about overflow in time differences.
	INT begin;
	INT end; // Boxes have an end when they pass out of their parent, so we know they can't continue w/out rechecking

	box *parent;
	list<box*> kids;
	// Todo: Should intersects include time information? Might save some unnecessary checks e.g. for regions that only touch in the future
	list<box*> intersects;
};

struct interval { INT begin, end; };


list<box*> *globalOptionsDest, *globalOptionsSrc, *globalOptionsXor;

// Things that are just touching should not count as intersecting,
// since that's the logic we've got in place for the individual `ent`s in bittoss

interval window(box *o, box *n) {
	#define retNone do { interval ret = {.begin=0, .end=0}; return ret; } while(0)
	// This will only be done for new boxes, since we don't believe in updates
	INT begin = n->begin;
	INT initBegin = begin;
	INT end = o->end - n->end > 0 ? n->end : o->end;
	if (end - begin <= 0) retNone; // Nothing to be done
	INT timeOffset = n->begin - o->begin;
	INT r = n->r + o->r;
	range(d, DIMS) {
		// Trouble is if the velocity is high enough we expect it to wrap eventually,
		// and with these boxes lasting as long as they can that could actually happen a lot.
		// This isn't even a math error per se, just a result of having really long boxes.
		// This actually seems pretty unlikely though, given we avoid creating new parents
		// unless absolutely necessary.

		// If it wraps (from the start) we can just take `INT_MAX / vel` and assume it collides at some point after that.
		// At this point we might have moved the start by INT_MAX (vel is non-zero) so we'd immediately check for
		// empty window-ness.
		// If the velocity is really high we just inch `start` forward a bit and that's fine.
		INT dvel = n->vel[d] - o->vel[d];
		INT initOffset = n->pos[d] - o->pos[d] - o->vel[d]*timeOffset;
		INT dpos = initOffset + dvel*(begin-initBegin);
		if (dvel == 0) {
			if (abs(dpos) < r) continue; // Full overlap, start/end aren't moved
			else retNone; // No overlap, we can exit early
		}
		if (abs(dpos) >= r) {
			// Not currently intersecting on this axis, but we are moving, so we might get an updated time
			INT spd, dist;
			if (dvel > 0) {
				spd = dvel;
				dist = -dpos;
			} else {
				spd = -dvel;
				dist = dpos;
			}
			dist -= r;
			if (dist < 0) dist = INT_MAX; // In this case the bound won't be as tight as it could, but it'll be pretty good
			begin += dist/spd;
			// Have to test this as soon as either changes since we don't
			// want them to overflow from successive operations
			if (end-begin <= 0) retNone;
		}

		dpos = initOffset + dvel*(end-initBegin);
		if (abs(dpos) >= r) {
			// Current projected end position doesn't intersect on this axis,
			// but we know we're moving from previously (dvel hasn't changed),
			// so we might get an updated end time.

			// We might want to de-duplicate this with the above at some point,
			// but I'm not trying to shoot myself in the foot with cleverness at this point.
			// (maybe move the sign-flipping to immediately after dvel is computed?)
			INT spd, dist;
			if (dvel > 0) {
				spd = dvel;
				dist = dpos;
			} else {
				spd = -dvel;
				dist = -dpos;
			}
			dist -= r;
			if (dist < 0) dist = INT_MAX; // In this case the bound won't be as tight as it could, but it'll be pretty good
			end -= dist/spd;
			if (end-begin <= 0) retNone;
		}
	}

	interval ret = {.begin = begin, .end = end};
	return ret;
	#undef retNone
}

static void setIntersects(box *n) {
	list<box*> &aunts = n->parent->intersects;
	range(i, aunts.num) {
		list<box*> &cousins = aunts[i]->kids;
		// Reverse iteration here is to make sure `n` is its own first intersect.
		// The first aunt will be its parent, and since it was just added it should be
		// the last child of its parent.
		// In general this results in a bias towards more recent boxes, which probably is a good thing.
		for (int j = cousins.num - 1; j >= 0; j--) {
			box *test = cousins[j];
			interval x = window(test, n);
			if (x.begin != x.end) {
				// This looks a little clumsy, but it's so that if `test == n`
				// we don't add itself to its intersects twice.
				// It saves a branch most of the time compared to doing this the naive way,
				// but it's very possible I overengineered this.
				int num = n->intersects.num;
				n->intersects.setMaxUp(num+1);
				n->intersects[num] = test;
				test->intersects.add(n);
				n->intersects.num = num+1;
			}
		}
	}
}

#define ALLOC_SIZE 100
list<box*> boxAllocs, freeBoxes;

static box* allocBox() {
	if (freeBoxes.num == 0) {
		// Todo This initialization is kinda slow... Maybe chuck it in another thread or something?
		box *newAlloc = new box[ALLOC_SIZE];
		boxAllocs.add(newAlloc);
		range(i, ALLOC_SIZE) {
			newAlloc[i].kids.init();
			newAlloc[i].intersects.init();
			freeBoxes.add(newAlloc[i]);
		}
	}
	box *ret = freeBoxes[--freeBoxes.num];
	ret->intersects.num = 0;
	ret->kids.num = 0;
	return ret;
}

// Note this does not populate `end` or `intersects`,
// since in some cases we might already know `end`
// and `intersects` depends on `end`.
static box* mkContainer(box *parent, box *n) {
	box *ret = allocBox();
	parent->kids.add(ret);
	ret->r = parent->r / 2;
	range(d, DIMS) {
		ret->pos[d] = n->pos[d];
		ret->vel[d] = n->vel[d];
	}
	ret->parent = parent;

	ret->begin = n->begin;
	return ret;
}

// Is is assumed `guess` is a leaf node, and not a valid parent.
static box* getMergeBase(box *guess, box *n) {
	// Go up only until we can confidently say
	// we can list all potential parents (as intersects)

	// This happens as soon as we intersect `n`,
	// but we also skip up until we're the right height because it simplifies some later logic,
	// and we'd have to climb that high anyway.

	box *parent = guess->parent;
	INT reqdParentR = n->r * 2; // Maybe this is 1.5, doesn't exactly have to match since inbound boxes are probably of any size
	// This assumes we're never trying to add anybody too perposterously big.
	while (parent->r < reqdParentR) parent = parent->parent;
	for (; ; parent = parent->parent) {
		// This case is a bit weird, since it may not even properly intersect `n`.
		// However, otehr funcs handle this edge case appropriately.
		if (!parent->parent) return parent;

		if (parent->end - n->begin <= 0) continue;
		INT r = parent->r + n->r;
		INT offset = new->begin - parent->begin;
		range(d, DIMS) {
			INT delta = parent->pos[d] + offset * parent->vel[d] - n->pos[d];
			if (abs(delta) >= r) goto next;
		}
		return parent;

		next:;
	}
}

void insert(box *guess, box *n) {
	// It is IMPORTANT to note that `n` must be small enough to have at least one nested level
	// below the apex. The apex is weird enough as it is, this condition reduces the number of
	// edge cases we have to consider.

	// Todo Maybe provide a faster path in the case where it fits fine in `guess->parent`,
	//      i.e. when it hasn't changed much from last time?
	box *mergeBase = getMergeBase(guess, n);
	if (tryLookDown(mergeBase, n)) return;
	// Please, mergeBase->parent was my father
	lookUp(mergeBase->parent ? mergeBase->parent : mergeBase, n);

	/* Todo
	Once it's created, we can look at the old guy's parent;
		in the special case that it's expiring when the new guy is being created,
		and so long as this is the case going up,
		we can move all of that parent's children to be under the new guy's parent of the corresponding rank.
			And delete the childless parent ofc, that'll be a little adventure
		Notes:
			Quick to check, thankfully
			Doesn't violate integrity of structure, that's all outdated
				(assuming time never flows backwards)
				and therefore contains 0 relevant volume,
				which is trivially contained by any parent at all.
			Increases chances that other people can quickly find a home when their time updates
	*/
}

void remove(box *o) {
	do {
		list<box*> &intersects = o->intersects;
		for (int i = 1; i < intersects.num; i++) {
			// Todo should this be a quickRm (which would be a new function)?
			intersects[i]->intersects.rm(o);
		}
		box *parent = o->parent;
		parent->kids.rm(o);
		freeBoxes.add(o);
		o = parent;
	} while (!o->kids.num && o->parent);
}

void listIntersects(box *a, list<box*> *out) {
	list<box*> &l = *out;
	list<box*> &intersects = *a->intersects;
	range(i, intersects.num) {
		box *test = intersects[i];
		if (test->kids.num) {
			addAll(out, test);
		} else {
			if (test > a) l.add(test);
		}
	}
}

static void addAll(list<box*> *dest, box *root) {
	list<box*> *optionsSrc = &root->kids;
	list<box*> *optionsDest = globalOptionsDest;

	while (optionsSrc->num) {
		optionsDest->num = 0;
		range(i, optionsSrc->num) {
			box *test = (*optionsSrc)[i];
			if (test->kids.num) {
				optionsDest->addAll(test->kids);
			} else {
				dest->add(test);
			}
		}
		optionsSrc = optionsDest;
		optionsDest = globalOptionsXor ^ optionsSrc;
	}
}

char tryLookDown(box *mergeBase, box *n) {
	const list<box*> *optionsSrc;
	INT optionsR;
	if (mergeBase->parent) {
		optionsSrc = &mergeBase->intersects;
		optionsR = mergeBase->r;
	} else if (mergeBase->kids.num) {
		optionsSrc = &mergeBase->kids;
		optionsR = (*optionsSrc)[0]->r;
	} else {
		return 0;
	}

	// Todo global lists aren't friendly to multithreading,
	//      but I don't really want to reallocate two each time we come in here
	list<box*> *optionsDest = globalOptionsDest;

	box* fosterParent = NULL;
	char fosterStale;

	INT minParentR = n->r * 2;

	do {
		optionsDest->num = 0;
		fosterStale = 1;

		char finalLevel = optionsR / 2 < minParentR;
		INT ancestorR = optionsR - n->r;
		INT fosterR;
		if (finalLevel) fosterR = ancestorR;
		else fosterR = optionsR / 2; // optionsR * (1 - 1/scale), and `scale` is probably 2
		range(i, optionsSrc->num) {
			box *test = (*optionsSrc)[i];
			if (!test.kids.num) continue;
			if (test->end - n->begin <= 0) continue;

			char foster = 1;
			INT offset = n->begin - test->begin;
			range(d, DIMS) {
				INT d1 = test->pos[d] + offset * test->vel[d] - n->pos[d];
				INT d2 = d1 + test->vel[d] - n->vel[d];
				d1 = abs(d1);
				d2 = abs(d2);
				if (d1 > ancestorR || d2 > ancestorR) goto fail;
				if (d1 > fosterR || d2 > fosterR) foster = 0;
			}

			if (foster && fosterStale) {
				fosterParent = test;
				if (finalLevel) break;
				fostersStale = 0;
			}
			if (!finalLevel) optionsDest->addAll(test.kids);

			fail:;
		}
		optionsSrc = optionsDest;
		optionsDest = globalOptionsXor ^ optionsSrc;
		optionsR /= 2;
	} while(!finalLevel);

	if (!fosterParent) return 0;

	box* level = fosterParent;
	while (level->r >= 2*minParentR) {
		box *next = mkContainer(level, n);
		if (level == fosterParent) {
			interval overlap = window(level, next);
			next->end = overlap.end;
		} else {
			next->end = level->end;
		}
		setIntersects(next);
		level = next;
	}
	level->kids.add(n);
	n->parent = level;
	// TODO It is assumed n->end is already set to n->begin + 1
	setIntersects(n);

	return 1;
}

void lookUp(box *level, box *n) {
	for (; level->parent; level = level->parent) {
		INT r = level->r / 2;
		list<box*> &intersects = level->intersects;
		range(i, intersects.num) {
			box *test = intersects[i];
			// Todo this "get offset and iterate dimensions" thing is pretty common, should be de-duped
			INT offset = n->begin - test->begin;
			range(d, DIMS) {
				INT d1 = test->pos[d] + offset * test->vel[d] - n->pos[d];
				INT d2 = d1 + test->vel[d] - n->vel[d];
				if (abs(d1) > r || abs(d2) > r) goto fail;
			}

			// Other option here would be to add to a list,
			// which then we would sort somehow
			level = test;
			goto done;

			fail:;
		}
	}
	done:;

	box *parent = level;
	level = mkContainer(parent, n);

	if (parent->parent) {
		// Todo We don't need to check the beginning part of the overlap, we know it's good.
		//      Anything we can do to reduce the effort here?
		interval overlap = window(parent, level);
		level->end = overlap.end;
	} else {
		// If we're at the very top, the apex is considered to contain all of space and time,
		// and now is as good an occasion as any to update its start and end.
		parent->begin = level->begin;
		parent->end = level->end = parent->begin + MAX/2;
	}

	setIntersects(level);
	parent = level;

	while (parent->r >= n->r * 4) { // Depends on scaling factor + fit factor, which are both expected to be 2.
		level = mkContainer(parent, n);
		level->end = parent->end;
		setIntersects(level);
		parent = level;
	}

	parent->kids.add(n);
	n->parent = parent;
	setIntersects(n);
}

/*
Updates are just inserts + removals

Removals are pretty simple, remove from all intersects, remove from parent's kids, and then consider parent for removal.

Inserts are considerably harder.
	First, sizing: The box itself will be snug, but will fit into a box which is one size larger than necessary, I think.
		This could also have some invisible constant like 1.5x, that would probably be fine
	Secondly, positioning:
		Not sure exactly how this is going to be laid out, but I can see it going up or down.
		Our first priority is minimizing new box creation (hence down/up check order),
		but the second priority is to put it in the one that will let it be as long as possible.
			(if finding a candidate on the way down maybe we just list candidates so we can rank them later,
				after all lists are still pretty cheap)
		(further notes moved to function stubs, above)
	Creation (of non-leaf boxes):
		Proceeds top-to-bottom
		Always includes self as an intersect
		Checks parent's intersects to see which parents to check for intersects
	Once it's created, we can look at the old guy's parent;
		in the special case that it's expiring when the new guy is being created,
		and so long as this is the case going up,
		we can move all of that parent's children to be under the new guy's parent of the corresponding rank.
			And delete the childless parent ofc, that'll be a little adventure
		Notes:
			Quick to check, thankfully
			Doesn't violate integrity of structure, that's all outdated
				(assuming time never flows backwards)
				and therefore contains 0 relevant volume,
				which is trivially contained by any parent at all.
			Increases chances that other people can quickly find a home when their time updates
*/

box *velbox_root;

void velbox_init() {
	boxAllocs.init();
	freeBoxes.init();

	globalOptionsDest = new list<box*>();
	globalOptionsDest->init();
	globalOptionsSrc = new list<box*>();
	globalOptionsSrc->init();
	globalOptionsXor = globalOptionsDest ^ globalOptionsSrc;

	velbox_root = allocBox();
	// `kids` can just stay empty for now
	velbox_root->intersects.add(velbox_root);
	velbox_root->parent = NULL;
	velbox_root->r = MAX/2 + 1;
	velbox_root->begin = 0;
	velbox_root->end = MAX/2; // Not sure if this could safely be `MAX` but really why bother this is plenty good
	range(d, DIMS) {
		// This *shouldn't* matter, but uninitialized data is never a *good* thing.
		velbox->root.pos[d] = 0;
		velbox->root.vel[d] = 0;
	}
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
}