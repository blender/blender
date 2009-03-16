#ifndef BM_WALKERS_H
#define BM_WALKERS_H

#include "BLI_ghash.h"

/*
  NOTE: do NOT modify topology while walking a mesh!
*/

/*Walkers*/
typedef struct BMWalker {
	BLI_mempool *stack;
	BMesh *bm;
	void *currentstate;
	void (*begin) (struct BMWalker *walker, void *start);
	void *(*yield)(struct BMWalker *walker);
	void *(*step) (struct BMWalker *walker);
	int restrictflag;
	GHash *visithash;
} BMWalker;

/*initialize a walker.  searchmask restricts some (not all) walkers to
  elements with a specific tool flag set.*/
void BMW_Init(struct BMWalker *walker, BMesh *bm, int type, int searchmask);
void *BMW_Begin(BMWalker *walker, void *start);
void *BMW_Step(struct BMWalker *walker);
void BMW_End(struct BMWalker *walker);

/*
example of usage, walking over an island of tool flagged faces:

BMWalker walker;
BMFace *f;

BMW_Init(&walker, bm, BMW_ISLAND, SOME_OP_FLAG);
f = BMW_Begin(&walker, some_start_face);
for (; f; f=BMW_Step(&walker)) {
	//do something with f
}
BMW_End(&walker);
*/

/*walk over connected geometry.  can restrict to a search flag,
  or not, it's optional.*/
#define BMW_SHELL	0

/*walk over an edge loop.  search flag doesn't do anything.*/
#define BMW_LOOP	1
/*#define BMW_RING	2
#define BMW_UVISLANDS	3*/
/*walk over an island of flagged faces.  note, that this doesn't work on
  non-manifold geometry.  it might be better to rewrite this to extract
  boundary info from the island walker, rather then directly walking
  over the boundary.  raises an error if it encouters nonmanifold
  geometry.*/
#define BMW_ISLANDBOUND	2

/*walk over all faces in an island of tool flagged faces.*/
#define BMW_ISLAND	3

#define BMW_MAXWALKERS	4

#endif