// If you want to define these using GCC args, that's fine -
// an alternative is to "wrap" this file (and the .cpp file)
// with files that just #define all this and then #include
// the "original" file. This should make it easier to target
// the scopes on the definitions.

// This file needs `list` defined,
// and the .cpp file needs `list`, `range`, and `void* cloneBoxData(box *in, box *out)`.
// Because I have no idea how "packages" are done in professional C,
// and because I'm just doing as a personal project, it's up to
// whoever includes this file to provide those definitions.

#ifndef INT
#define INT int32_t
#endif

#ifndef UINT
#define UINT uint32_t
#endif

#ifndef BITS
#define BITS 32
#endif

#ifndef MAX
#define MAX INT32_MAX
#endif

#ifndef DIMS
#define DIMS 3
#endif

#ifndef SCALE
#define SCALE 4
#endif

#ifndef FIT
#define FIT 3
#endif

struct box;

struct sect {
	box *b;
	int i;
	UINT t_next, t_end; // If t_next==t_end, it's an active intersect. Otherwise it's scheduled for the future
};

struct box {
	// Should not exceed INT_MAX/2 (so it can be safely added to itself)
	// Note for non-fish boxes, this will always be the same for all axes
	INT r[DIMS];

	INT p1[DIMS];
	INT p2[DIMS];

	box *parent;
	list<box*> kids;
	list<sect> intersects;

	void *data;
	// t_now  increments, thereby counting towards t_next / t_end.
	// t_next is when the next `sect` needs maintenance (or when t_end is, whichever's sooner).
	// t_end  is when this box becomes invalid (replaces `validity`).
	UINT t_now, t_next, t_end;
};

box* velbox_alloc();
void velbox_insert(box *guess, box *n);
void velbox_remove(box *o);

// Intent is that you call `velbox_step` on all your leaf boxes,
// and then call velbox_refresh to step all the interal boxes and update intersects as necessary.
void velbox_step(box *b, INT *p1, INT *p2);
void velbox_refresh(box *root);
// If you need to make changes to a subset of boxes, you call velbox_update on all of them,
// and then velbox_single_refresh on all of them.
void velbox_update(box *b);
void velbox_single_refresh(box *b);

box* velbox_getRoot();
void velbox_freeRoot(box *r);
box* velbox_clone(box *oldRoot);
void velbox_init();
void velbox_destroy();
