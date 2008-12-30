#ifndef BMESH_WALKERS_H
#define BMESH_WALKERS_H

/*Walkers*/
typedef struct BMWalker{
	BLI_mempool *stack;
	BMesh *bm;
	void *currentstate;
	void *(*begin) (struct BMWalker *walker, void *start);
	void *(*yield)(struct BMWalker *walker);
	void   (*step) (struct BMWalker *walker);
	int visitedmask;
}BMWalker;

void BMWalker_Init(struct BMWalker *walker, BMesh *bm, int type, int searchmask);
void *BMWalker_Step(struct BMWalker *walker);
void BMWalker_End(struct BMWalker *walker);

#define BMESH_SHELLWALKER	1
#define BMESH_LOOPWALKER	2
#define BMESH_RINGWALKER	3
#define BMESH_UVISLANDS		4

#endif