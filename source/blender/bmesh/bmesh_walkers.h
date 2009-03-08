#ifndef BM_WALKERS_H
#define BM_WALKERS_H

#include "BLI_ghash.h"

/*
  NOTE: do NOT modify topology while walking a mesh!
*/

/*Walkers*/
typedef struct BMWalker{
	BLI_mempool *stack;
	BMesh *bm;
	void *currentstate;
	void *(*begin) (struct BMWalker *walker, void *start);
	void *(*yield)(struct BMWalker *walker);
	void *(*step) (struct BMWalker *walker);
	int restrictflag;
	GHash *visithash;
}BMWalker;

void BMWalker_Init(struct BMWalker *walker,BMesh *bm,int type, int searchmask);
void *BMWalker_Step(struct BMWalker *walker);
void BMWalker_End(struct BMWalker *walker);

#define BMW_SHELL	0
/*#define BMW_LOOP	1
#define BMW_RING	2
#define BMW_UVISLANDS	3*/
#define BMW_ISLANDBOUND	1
#define BMW_MAXWALKERS	2

#endif