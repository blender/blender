#ifndef BM_WALKERS_H
#define BM_WALKERS_H

/*Walkers*/
typedef struct BMWalker{
	BLI_mempool *stack;
	BMesh *bm;
	void *currentstate;
	void *(*begin) (struct BMWalker *walker, void *start);
	void *(*yield)(struct BMWalker *walker);
	void   (*step) (struct BMWalker *walker);
	int visitedmask;
	int restrictflag;
}BMWalker;

void BMWalker_Init(struct BMWalker *walker, BMesh *bm, int type, int searchmask);
void *BMWalker_Step(struct BMWalker *walker);
void BMWalker_End(struct BMWalker *walker);

#define BM_SHELLWALKER	0
#define BM_LOOPWALKER	1
#define BM_RINGWALKER	2
#define BM_UVISLANDS		3
#define BM_MAXWALKERS	4

#endif