#include <stdio.h>
#include <string.h>
#include "BLI_mempool.h"

#include "bmesh_private.h"
#include "bmesh_walkers.h"

#include "bmesh.h"

/*
NOTE: This code needs to be read through a couple of times!!
*/

typedef struct shellWalker{
	struct shellWalker *prev;
	BMVert *base;			
	BMEdge *curedge, *current;
} shellWalker;

/*
 *	BMWalker - change this to use the filters functions.
 *	
 *	A generic structure for maintaing the state and callbacks nessecary for walking over
 *  the surface of a mesh. An example of usage:
 *
 *	     BMEdge *edge;
 *	     BMWalker *walker = BMWalker_create(BM_SHELLWALKER, BM_SELECT);
 *       walker->begin(walker, vert);
 *       for(edge = BMWalker_walk(walker); edge; edge = bmeshWwalker_walk(walker)){
 *            bmesh_select_edge(edge);
 *       }
 *       BMWalker_free(walker);
 *
 *	The above example creates a walker that walks over the surface a mesh by starting at
 *  a vertex and traveling across its edges to other vertices, and repeating the process
 *  over and over again until it has visited each vertex in the shell. An additional restriction
 *  is passed into the BMWalker_create function stating that we are only interested
 *  in walking over edges that have been flagged with the bitmask 'BM_SELECT'.
 *
 *
*/

/*Forward declerations*/
static int request_walkerMask(struct BMesh *bm);
static void *BMWalker_walk(struct BMWalker *walker);
static void BMWalker_popState(struct BMWalker *walker);
static void BMWalker_pushState(struct BMWalker *walker);
static void *shellWalker_begin(struct BMWalker *walker, void *data);
static void *shellWalker_yield(struct BMWalker *walker);
static void shellWalker_step(struct BMWalker *walker);
struct shellWalker;

/* Pointer hiding*/
typedef struct bmesh_walkerGeneric{
	struct bmesh_walkerGeneric *prev;
} bmesh_walkerGeneric;


/*
 *	REQUEST_WALKERMASK
 *
 *  Each active walker for a bmesh has its own bitmask
 *	to use for flagging elements as visited. request_walkerMask
 *	queries the bmesh passed in and returns the first free
 *  bitmask. If none are free, it returns 0. The maximum number
 *  of walkers that can be used for a single bmesh between calls to
 *  bmesh_edit_begin() and bmesh_edit_end() is defined by the constant
 *  BM_MAXWALKERS.
 *
*/

static int request_walkerMask(BMesh *bm)
{
	int i;
	for(i=0; i < BM_MAXWALKERS; i++){
		if(!(bm->walkers & (1 << i))){
			bm->walkers |= (1 << i);
			return (1 << i);
		}
	}
	return 0;
}




/*
 * BMWalker_CREATE
 * 
 * Allocates and returns a new mesh walker of 
 * a given type. The elements visited are filtered
 * by the bitmask 'searchmask'.
 *
*/

void BMWalker_init(BMWalker *walker, BMesh *bm, int type, int searchmask)
{
	int visitedmask = request_walkerMask(bm);
	int size = 0;
	
	if(visitedmask){
		memset(walker, 0, sizeof(BMWalker));
		walker->bm = bm;
		walker->visitedmask = visitedmask;
		walker->restrictflag = searchmask;
		switch(type){
			case BM_SHELLWALKER:
				walker->begin = shellWalker_begin;
				walker->step = shellWalker_step;
				walker->yield = shellWalker_yield;
				size = sizeof(shellWalker);		
				break;
			//case BM_LOOPWALKER:
			//	walker->begin = loopwalker_begin;
			//	walker->step = loopwalker_step;
			//	walker->yield = loopwalker_yield;
			//	size = sizeof(loopWalker);
			//	break;
			//case BM_RINGWALKER:
			//	walker->begin = ringwalker_begin;
			//	walker->step = ringwalker_step;
			//	walker->yield = ringwalker_yield;
			//	size = sizeof(ringWalker);
			//	break;
			default:
				break;
		}
		walker->stack = BLI_mempool_create(size, 100, 100);
		walker->currentstate = NULL;
	}
}

/*
 * BMWalker_END
 *
 * Frees a walker's stack.
 *
*/

void BMWalker_end(BMWalker *walker)
{
	BLI_mempool_destroy(walker->stack);
}


/*
 * BMWalker_STEP
 *
*/

void *BMWalker_step(BMWalker *walker)
{
	BMHeader *head;

	while(head = BMWalker_walk(walker)){
		//NOTE: figure this out 
		//if(bmesh_test_flag(head, walker->restrictflag)) return head;
		return head;
	}
	return NULL;
}

/*
 * BMWalker_WALK
 *
 * Steps a mesh walker forward by one element
 *
 * TODO:
 *  -add searchmask filtering
 *
*/

static void *BMWalker_walk(BMWalker *walker)
{
	void *current = NULL;

	while(walker->currentstate){
		walker->step(walker);
		current = walker->yield(walker);
		if(current) return current;
		else BMWalker_popState(walker);

	}
	return NULL;
}

/*
 * BMWalker_POPSTATE
 *
 * Pops the current walker state off the stack
 * and makes the previous state current
 *
*/

static void BMWalker_popState(BMWalker *walker)
{
	void *oldstate;
	oldstate = walker->currentstate;
	walker->currentstate 
		= ((bmesh_walkerGeneric*)walker->currentstate)->prev;
	BLI_mempool_free(walker->stack, oldstate);
}

/*
 * BMWalker_PUSHSTATE
 *
 * Pushes the current state down the stack and allocates
 * a new one.
 *
*/

static void BMWalker_pushState(BMWalker *walker)
{
	bmesh_walkerGeneric *newstate;
	newstate = BLI_mempool_alloc(walker->stack);
	newstate->prev = walker->currentstate;
	walker->currentstate = newstate;
}

/*	Shell Walker:
 *
 *	Starts at a vertex on the mesh and walks over the 'shell' it belongs 
 *	to via visiting connected edges.
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 * 
*/

static void *shellWalker_begin(BMWalker *walker, void *data){
	BMVert *v = data;
	shellWalker *shellWalk = NULL;
	BMWalker_pushState(walker);
	shellWalk = walker->currentstate;
	shellWalk->base = shellWalk->curedge = NULL;
	if(v->edge){
		shellWalk->base = v;
		shellWalk->curedge = v->edge;
	}
}
static void *shellWalker_yield(BMWalker *walker)
{
	shellWalker *shellWalk = walker->currentstate;
	return shellWalk->curedge;
}

static void shellWalker_step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *ov = NULL;
	int restrictpass = 1;
	shellWalker *shellWalk = walker->currentstate;

	if(!(shellWalk->base->head.flag & walker->visitedmask))
		shellWalk->base->head.flag |= walker->visitedmask;
	
	/*find the next edge whose other vertex has not been visited*/
	curedge = shellWalk->curedge;
	do{
		if(!(curedge->head.flag & walker->visitedmask)){ 
			curedge->head.flag |= walker->visitedmask;
			if(walker->restrictflag && (!(curedge->head.flag & walker->restrictflag))) restrictpass = 0;
			if(restrictpass){
				ov = BM_OtherEdgeVert(curedge, shellWalk->base);
				
				/*save current state*/
				shellWalk->curedge = curedge;
				/*push a new state onto the stack*/
				BMWalker_pushState(walker);
				
				/*populate the new state*/
				((shellWalker*)walker->currentstate)->base = ov;
				((shellWalker*)walker->currentstate)->curedge = curedge;
				/*break out of loop*/

				next = curedge;
				break;
			}
			curedge = bmesh_disk_nextedge(curedge, shellWalk->base);
		}
	}while(curedge != shellWalk->curedge);
	
	shellWalk->current = next;
	return shellWalk->current;
}