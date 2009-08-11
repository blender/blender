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
	int flag;
} BMWalker;

/*initialize a walker.  searchmask restricts some (not all) walkers to
  elements with a specific tool flag set.  flags is specific to each walker.*/
void BMW_Init(struct BMWalker *walker, BMesh *bm, int type, int searchmask, int flags);
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

enum {
	/*walk over connected geometry.  can restrict to a search flag,
	or not, it's optional.
	
	takes a vert as an arugment, and spits out edges, restrict flag acts
	on the edges as well.*/
	BMW_SHELL,
	/*walk over an edge loop.  search flag doesn't do anything.*/
	BMW_LOOP,
	BMW_FACELOOP,
	BMW_EDGERING,
	/*#define BMW_RING	2*/
	//walk over uv islands; takes a loop as input.  restrict flag 
	//restricts the walking to loops whose vert has restrict flag set as a
	//tool flag.
	//
	//the flag parameter to BMW_Init maps to a loop customdata layer index.
	BMW_UVISLAND,
	/*walk over an island of flagged faces.  note, that this doesn't work on
	  non-manifold geometry.  it might be better to rewrite this to extract
	  boundary info from the island walker, rather then directly walking
	  over the boundary.  raises an error if it encouters nonmanifold
	  geometry.*/
	BMW_ISLANDBOUND,
	/*walk over all faces in an island of tool flagged faces.*/
	BMW_ISLAND,
	BMW_MAXWALKERS,
};

#endif