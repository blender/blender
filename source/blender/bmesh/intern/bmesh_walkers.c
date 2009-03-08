#include <stdio.h>
#include <string.h>
#include "BLI_mempool.h"

#include "bmesh_private.h"
#include "bmesh_walkers.h"

#include "bmesh.h"

/*
 - joeedh -
 design notes:

 * walkers should use tool flags, not header flags
 * walkers now use ghash rather then stealing flags.
   ghash can be rewritten to be faster if necassary.
 * walkers should always raise BMERR_WALKER_FAILED,
   with a custom error message.  This message will
   probably be replaced by operator-specific messages
   in most cases.
 * tools should ALWAYS have necassary error handling
   for if walkers fail.
*/

/*
NOTE: This code needs to be read through a couple of times!!
*/

typedef struct shellWalker{
	struct shellWalker *prev;
	BMVert *base;			
	BMEdge *curedge, *current;
} shellWalker;

typedef struct islandboundWalker {
	struct islandboundWalker *prev;
	BMEdge *base;
	BMVert *lastv;
	BMEdge *curedge;
} islandboundWalker;

/*  NOTE: this comment is out of date, update it - joeedh
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
static void *BMWalker_walk(struct BMWalker *walker);
static void BMWalker_popstate(struct BMWalker *walker);
static void BMWalker_pushstate(struct BMWalker *walker);

static void *shellWalker_Begin(struct BMWalker *walker, void *data);
static void *shellWalker_Yield(struct BMWalker *walker);
static void *shellWalker_Step(struct BMWalker *walker);

static void *islandboundWalker_Begin(BMWalker *walker, void *data);
static void *islandboundWalker_Yield(BMWalker *walker);
static void *islandboundWalker_Step(BMWalker *walker);

struct shellWalker;

/* Pointer hiding*/
typedef struct bmesh_walkerGeneric{
	struct bmesh_walkerGeneric *prev;
} bmesh_walkerGeneric;


/*
 * BMWalker_CREATE
 * 
 * Allocates and returns a new mesh walker of 
 * a given type. The elements visited are filtered
 * by the bitmask 'searchmask'.
 *
*/

void BMWalker_Init(BMWalker *walker, BMesh *bm, int type, int searchmask)
{
	int size = 0;
	
	memset(walker, 0, sizeof(BMWalker));
	walker->bm = bm;
	walker->restrictflag = searchmask;
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	switch(type){
		case BMW_SHELL:
			walker->begin = shellWalker_Begin;
			walker->step = shellWalker_Step;
			walker->yield = shellWalker_Yield;
			size = sizeof(shellWalker);		
			break;
		case BMW_ISLANDBOUND:
			walker->begin = islandboundWalker_Begin;
			walker->step = islandboundWalker_Step;
			walker->yield = islandboundWalker_Yield;
			size = sizeof(islandboundWalker);		
			break;
		//case BMW_LOOP:
		//	walker->begin = loopwalker_Begin;
		//	walker->step = loopwalker_Step;
		//	walker->yield = loopwalker_Yield;
		//	size = sizeof(loopWalker);
		//	break;
		//case BMW_RING:
		//	walker->begin = ringwalker_Begin;
		//	walker->step = ringwalker_Step;
		//	walker->yield = ringwalker_Yield;
		//	size = sizeof(ringWalker);
		//	break;
		default:
			break;
	}
	walker->stack = BLI_mempool_create(size, 100, 100);
	walker->currentstate = NULL;
}

/*
 * BMWalker_End
 *
 * Frees a walker's stack.
 *
*/

void BMWalker_End(BMWalker *walker)
{
	BLI_mempool_destroy(walker->stack);
}


/*
 * BMWalker_Step
 *
*/

void *BMWalker_Step(BMWalker *walker)
{
	BMHeader *head;

	head = BMWalker_walk(walker);

	return head;
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
		else BMWalker_popstate(walker);

	}
	return NULL;
}

/*
 * BMWalker_popstate
 *
 * Pops the current walker state off the stack
 * and makes the previous state current
 *
*/

static void BMWalker_popstate(BMWalker *walker)
{
	void *oldstate;
	oldstate = walker->currentstate;
	walker->currentstate 
		= ((bmesh_walkerGeneric*)walker->currentstate)->prev;
	BLI_mempool_free(walker->stack, oldstate);
}

/*
 * BMWalker_pushstate
 *
 * Pushes the current state down the stack and allocates
 * a new one.
 *
*/

static void BMWalker_pushstate(BMWalker *walker)
{
	bmesh_walkerGeneric *newstate;
	newstate = BLI_mempool_alloc(walker->stack);
	newstate->prev = walker->currentstate;
	walker->currentstate = newstate;
}

void BMWalker_reset(BMWalker *walker) {
	while (walker->currentstate) {
		BMWalker_popstate(walker);
	}
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

static void *shellWalker_Begin(BMWalker *walker, void *data){
	BMVert *v = data;
	shellWalker *shellWalk = NULL;
	BMWalker_pushstate(walker);
	shellWalk = walker->currentstate;
	shellWalk->base = NULL;
	shellWalk->curedge = NULL;
	if(v->edge){
		shellWalk->base = v;
		shellWalk->curedge = v->edge;
	}

	return v->edge;
}
static void *shellWalker_Yield(BMWalker *walker)
{
	shellWalker *shellWalk = walker->currentstate;
	return shellWalk->curedge;
}

static void *shellWalker_Step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *ov = NULL;
	int restrictpass = 1;
	shellWalker *shellWalk = walker->currentstate;
	
	if (!BLI_ghash_lookup(walker->visithash, shellWalk->base))
		BLI_ghash_insert(walker->visithash, shellWalk->base, NULL);

	/*find the next edge whose other vertex has not been visited*/
	curedge = shellWalk->curedge;
	do{
		if (!BLI_ghash_lookup(walker->visithash, curedge)) { 
			BLI_ghash_insert(walker->visithash, curedge, NULL);
			if(walker->restrictflag && (!BMO_TestFlag(walker->bm, curedge, walker->restrictflag))) restrictpass = 0;
			if(restrictpass) {
				ov = BM_OtherEdgeVert(curedge, shellWalk->base);
				
				/*save current state*/
				shellWalk->curedge = curedge;
				/*push a new state onto the stack*/
				BMWalker_pushstate(walker);
				
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

/*	Island Boundary Walker:
 *
 *	Starts at a edge on the mesh and walks over the boundary of an
 *      island it belongs to.
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 * 
*/

static void *islandboundWalker_Begin(BMWalker *walker, void *data){
	BMEdge *e = data;
	islandboundWalker *iwalk = NULL;

	BMWalker_pushstate(walker);

	iwalk = walker->currentstate;
	iwalk->base = iwalk->curedge = e;

	return e;
}

static void *islandboundWalker_Yield(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate;

	return iwalk->curedge;
}

static void *islandboundWalker_Step(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate, *owalk;
	BMIter iter, liter;
	BMVert *v;
	BMEdge *e = iwalk->curedge;
	BMFace *f;
	BMLoop *l;
	int found=0, radlen, sellen;;

	owalk = iwalk;

	if (iwalk->lastv == e->v1) v = e->v2;
	else v = e->v1;

	if (BM_Nonmanifold_Vert(v)) {
		BMWalker_reset(walker);
		BMO_RaiseError(walker->bm, NULL,BMERR_WALKER_FAILED,
			"Non-manifold vert"
			"while searching region boundary");
		return NULL;
	}

	BMWalker_popstate(walker);
	
	/*find start face*/
	l = BMIter_New(&liter, walker->bm, BM_LOOPS_OF_EDGE; e);
	for (; l; l=BMIter_Step(&liter)) {
		if (BMO_TestFlag(walker->bm, l->f, walker->restrictflag)) {
			f = l->f;
			break;
		}
	}
	
	while (1) {
		l = BM_OtherFaceLoop(e, v, f);
		if (l) {
			l = l->radial.next->data;
			f = l->f;
			e = l->e;
			if(!BMO_TestFlag(walker->bm,l->f,walker->restrictflag))
				break;
		} else {
			break;
		}
	}
	
	if (e == iwalk->curedge) return NULL;
	if (BLI_ghash_haskey(walker->visithash, e)) return NULL;

	BLI_ghash_insert(walker->visithash, e, NULL);
	BMWalker_pushstate(walker);
	
	iwalk = walker->currentstate;
	iwalk->base = owalk->base;
	iwalk->curedge = e;
	iwalk->lastv = v;				

	return iwalk->curedge;
}
