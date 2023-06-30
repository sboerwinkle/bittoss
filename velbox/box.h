
// If you want to define these using GCC args, that's fine -
// an alternative is to "wrap" this file (and the .cpp file)
// with files that just #define all this and then #include
// the "original" file. This should make it easier to target
// the scopes on the definitions.

#ifndef INT
#define INT int32_t
#endif

#ifndef BITS
#define BITS 32
#endif

#define MAX ((((INT) 1) << (BITS - 1)) - (INT)1)

#ifndef DIMS
#define DIMS 3
#endif

#ifndef SCALE
#define SCALE 4
#endif

#ifndef FIT
#define FIT 2
#endif

struct box {
	// Should not exceed INT_MAX/2 (so it can be safely added to itself)
	// Note for non-fish boxes, this will always be the same for all axes
	INT r[DIMS];

	INT p1[DIMS];
	INT p2[DIMS];

	box *parent;
	list<box*> kids;
	list<box*> intersects;

	void *data;
	char refreshed;
};

box* velbox_alloc();
void velbox_insert(box *guess, box *n);
void velbox_remove(box *o);
void velbox_update(box *b);
void velbox_refresh(box *root);
box* velbox_getRoot();
box* velbox_freeRoot(box *r);
void velbox_init();
void velbox_destroy();
