#include <stdio.h>
#include <string.h>

#include "BKE_utildefines.h"
#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BLI_mempool.h"
#include "BLI_array.h"

#include "bmesh_private.h"
#include "bmesh_walkers.h"

#include "bmesh.h"

/*
 - joeedh -
 design notes:

 original desing: walkers directly emulation recursive functions.
 functions save their state onto a stack, and also push new states
 to implement recursive or looping behaviour.  generally only one
 state push per call with a specific state is desired.

 basic design pattern: the walker step function goes through it's
 list of possible choices for recursion, and recurses (by pushing a new state)
 using the first non-visited one.  this choise is the flagged as visited using
 the ghash.  each step may push multiple new states onto the stack at once.

 * walkers use tool flags, not header flags
 * walkers now use ghash for storing visited elements, 
   rather then stealing flags.  ghash can be rewritten 
   to be faster if necassary, in the far future :) .
 * tools should ALWAYS have necassary error handling
   for if walkers fail.
*/

typedef struct shellWalker{
	struct shellWalker *prev;
	BMVert *base;			
	BMEdge *curedge, *current;
} shellWalker;

typedef struct islandboundWalker {
	struct islandboundWalker *prev;
	BMLoop *base;
	BMVert *lastv;
	BMLoop *curloop;
} islandboundWalker;

typedef struct islandWalker {
	struct islandWalker * prev;
	BMFace *cur;
} islandWalker;

typedef struct loopWalker {
	struct loopWalker * prev;
	BMEdge *cur, *start;
	BMVert *lastv, *startv;
	int startrad, stage2;
} loopWalker;

typedef struct faceloopWalker {
	struct faceloopWalker * prev;
	BMLoop *l;
	int nocalc;
} faceloopWalker;

typedef struct edgeringWalker {
	struct edgeringWalker * prev;
	BMLoop *l;
} edgeringWalker;

typedef struct uvedgeWalker {
	struct uvedgeWalker *prev;
	BMLoop *l;
} uvedgeWalker;

/*  NOTE: this comment is out of date, update it - joeedh
 *	BMWalker - change this to use the filters functions.
 *	
 *	A generic structure for maintaing the state and callbacks nessecary for walking over
 *  the surface of a mesh. An example of usage:
 *
 *	     BMEdge *edge;
 *	     BMWalker *walker = BMW_create(BM_SHELLWALKER, BM_SELECT);
 *       walker->begin(walker, vert);
 *       for(edge = BMW_walk(walker); edge; edge = bmeshWwalker_walk(walker)){
 *            bmesh_select_edge(edge);
 *       }
 *       BMW_free(walker);
 *
 *	The above example creates a walker that walks over the surface a mesh by starting at
 *  a vertex and traveling across its edges to other vertices, and repeating the process
 *  over and over again until it has visited each vertex in the shell. An additional restriction
 *  is passed into the BMW_create function stating that we are only interested
 *  in walking over edges that have been flagged with the bitmask 'BM_SELECT'.
 *
 *
*/

/*Forward declerations*/
static void *BMW_walk(struct BMWalker *walker);
static void BMW_popstate(struct BMWalker *walker);
static void BMW_pushstate(struct BMWalker *walker);

static void shellWalker_begin(struct BMWalker *walker, void *data);
static void *shellWalker_yield(struct BMWalker *walker);
static void *shellWalker_step(struct BMWalker *walker);

static void islandboundWalker_begin(BMWalker *walker, void *data);
static void *islandboundWalker_yield(BMWalker *walker);
static void *islandboundWalker_step(BMWalker *walker);

static void islandWalker_begin(BMWalker *walker, void *data);
static void *islandWalker_yield(BMWalker *walker);
static void *islandWalker_step(BMWalker *walker);

static void loopWalker_begin(BMWalker *walker, void *data);
static void *loopWalker_yield(BMWalker *walker);
static void *loopWalker_step(BMWalker *walker);

static void faceloopWalker_begin(BMWalker *walker, void *data);
static void *faceloopWalker_yield(BMWalker *walker);
static void *faceloopWalker_step(BMWalker *walker);

static void edgeringWalker_begin(BMWalker *walker, void *data);
static void *edgeringWalker_yield(BMWalker *walker);
static void *edgeringWalker_step(BMWalker *walker);

static void uvedgeWalker_begin(BMWalker *walker, void *data);
static void *uvedgeWalker_yield(BMWalker *walker);
static void *uvedgeWalker_step(BMWalker *walker);

/* Pointer hiding*/
typedef struct bmesh_walkerGeneric{
	struct bmesh_walkerGeneric *prev;
} bmesh_walkerGeneric;


void *BMW_Begin(BMWalker *walker, void *start) {
	walker->begin(walker, start);
	
	return walker->currentstate ? walker->step(walker) : NULL;
}

/*
 * BMW_CREATE
 * 
 * Allocates and returns a new mesh walker of 
 * a given type. The elements visited are filtered
 * by the bitmask 'searchmask'.
 *
*/

void BMW_Init(BMWalker *walker, BMesh *bm, int type, int searchmask, int flag)
{
	int size = 0;
	
	memset(walker, 0, sizeof(BMWalker));

	walker->flag = flag;
	walker->bm = bm;
	walker->restrictflag = searchmask;
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	switch(type){
		case BMW_SHELL:
			walker->begin = shellWalker_begin;
			walker->step = shellWalker_step;
			walker->yield = shellWalker_yield;
			size = sizeof(shellWalker);		
			break;
		case BMW_ISLANDBOUND:
			walker->begin = islandboundWalker_begin;
			walker->step = islandboundWalker_step;
			walker->yield = islandboundWalker_yield;
			size = sizeof(islandboundWalker);		
			break;
		case BMW_ISLAND:
			walker->begin = islandWalker_begin;
			walker->step = islandWalker_step;
			walker->yield = islandWalker_yield;
			size = sizeof(islandWalker);		
			break;
		case BMW_LOOP:
			walker->begin = loopWalker_begin;
			walker->step = loopWalker_step;
			walker->yield = loopWalker_yield;
			size = sizeof(loopWalker);
			break;
		case BMW_FACELOOP:
			walker->begin = faceloopWalker_begin;
			walker->step = faceloopWalker_step;
			walker->yield = faceloopWalker_yield;
			size = sizeof(faceloopWalker);
			break;
		case BMW_EDGERING:
			walker->begin = edgeringWalker_begin;
			walker->step = edgeringWalker_step;
			walker->yield = edgeringWalker_yield;
			size = sizeof(edgeringWalker);
			break;
		case BMW_LOOPDATA_ISLAND:
			walker->begin = uvedgeWalker_begin;
			walker->step = uvedgeWalker_step;
			walker->yield = uvedgeWalker_yield;
			size = sizeof(uvedgeWalker);
			break;
		default:
			break;
	}
	walker->stack = BLI_mempool_create(size, 100, 100, 1);
	walker->currentstate = NULL;
}

/*
 * BMW_End
 *
 * Frees a walker's stack.
 *
*/

void BMW_End(BMWalker *walker)
{
	BLI_mempool_destroy(walker->stack);
	BLI_ghash_free(walker->visithash, NULL, NULL);
}


/*
 * BMW_Step
 *
*/

void *BMW_Step(BMWalker *walker)
{
	BMHeader *head;

	head = BMW_walk(walker);

	return head;
}

/*
 * BMW_WALK
 *
 * Steps a mesh walker forward by one element
 *
 * TODO:
 *  -add searchmask filtering
 *
*/

static void *BMW_walk(BMWalker *walker)
{
	void *current = NULL;

	while(walker->currentstate){
		current = walker->step(walker);
		if(current) return current;
	}
	return NULL;
}

/*
 * BMW_popstate
 *
 * Pops the current walker state off the stack
 * and makes the previous state current
 *
*/

static void BMW_popstate(BMWalker *walker)
{
	void *oldstate;
	oldstate = walker->currentstate;
	walker->currentstate 
		= ((bmesh_walkerGeneric*)walker->currentstate)->prev;
	BLI_mempool_free(walker->stack, oldstate);
}

/*
 * BMW_pushstate
 *
 * Pushes the current state down the stack and allocates
 * a new one.
 *
*/

static void BMW_pushstate(BMWalker *walker)
{
	bmesh_walkerGeneric *newstate;
	newstate = BLI_mempool_alloc(walker->stack);
	newstate->prev = walker->currentstate;
	walker->currentstate = newstate;
}

void BMW_reset(BMWalker *walker) {
	while (walker->currentstate) {
		BMW_popstate(walker);
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

static void shellWalker_begin(BMWalker *walker, void *data){
	BMIter eiter;
	BMEdge *e;
	BMVert *v = data;
	shellWalker *shellWalk = NULL;

	if (!v->edge)
		return;

	if (walker->restrictflag) {
		BM_ITER(e, &eiter, walker->bm, BM_EDGES_OF_VERT, v) {
			if (BMO_TestFlag(walker->bm, e, walker->restrictflag))
				break;
		}
	} else {
		e = v->edge;
	}

	if (!e) 
		return;

	if (BLI_ghash_haskey(walker->visithash, e))
		return;

	BMW_pushstate(walker);

	shellWalk = walker->currentstate;
	shellWalk->base = v;
	shellWalk->curedge = e;
	BLI_ghash_insert(walker->visithash, e, NULL);
}

static void *shellWalker_yield(BMWalker *walker)
{
	shellWalker *shellWalk = walker->currentstate;
	return shellWalk->curedge;
}

static void *shellWalker_step(BMWalker *walker)
{
	shellWalker *swalk = walker->currentstate;
	BMEdge *e, *e2;
	BMVert *v;
	BMIter iter;
	int i;

	BMW_popstate(walker);

	e = swalk->curedge;
	for (i=0; i<2; i++) {
		v = i ? e->v2 : e->v1;
		BM_ITER(e2, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
			if (walker->restrictflag && !BMO_TestFlag(walker->bm, e2, walker->restrictflag))
				continue;
			if (BLI_ghash_haskey(walker->visithash, e2))
				continue;
			
			BMW_pushstate(walker);
			BLI_ghash_insert(walker->visithash, e2, NULL);

			swalk = walker->currentstate;
			swalk->curedge = e2;
		}
	}

	return e;
}

#if 0
static void *shellWalker_step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *ov = NULL;
	int restrictpass = 1;
	shellWalker shellWalk = *((shellWalker*)walker->currentstate);
	
	if (!BLI_ghash_haskey(walker->visithash, shellWalk.base))
		BLI_ghash_insert(walker->visithash, shellWalk.base, NULL);

	BMW_popstate(walker);


	/*find the next edge whose other vertex has not been visited*/
	curedge = shellWalk.curedge;
	do{
		if (!BLI_ghash_haskey(walker->visithash, curedge)) { 
			if(!walker->restrictflag || (walker->restrictflag &&
			   BMO_TestFlag(walker->bm, curedge, walker->restrictflag)))
			{
				ov = BM_OtherEdgeVert(curedge, shellWalk.base);
				
				/*push a new state onto the stack*/
				BMW_pushstate(walker);
				BLI_ghash_insert(walker->visithash, curedge, NULL);
				
				/*populate the new state*/

				((shellWalker*)walker->currentstate)->base = ov;
				((shellWalker*)walker->currentstate)->curedge = curedge;
			}
		}
		curedge = bmesh_disk_nextedge(curedge, shellWalk.base);
	}while(curedge != shellWalk.curedge);
	
	return shellWalk.curedge;
}
#endif

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

static void islandboundWalker_begin(BMWalker *walker, void *data){
	BMLoop *l = data;
	islandboundWalker *iwalk = NULL;

	BMW_pushstate(walker);

	iwalk = walker->currentstate;

	iwalk->base = iwalk->curloop = l;
	iwalk->lastv = l->v;

	BLI_ghash_insert(walker->visithash, data, NULL);

}

static void *islandboundWalker_yield(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate;

	return iwalk->curloop;
}

static void *islandboundWalker_step(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate, owalk;
	BMVert *v;
	BMEdge *e = iwalk->curloop->e;
	BMFace *f;
	BMLoop *l = iwalk->curloop;
	int found=0;

	owalk = *iwalk;

	if (iwalk->lastv == e->v1) v = e->v2;
	else v = e->v1;

	if (BM_Nonmanifold_Vert(walker->bm, v)) {
		BMW_reset(walker);
		BMO_RaiseError(walker->bm, NULL,BMERR_WALKER_FAILED,
			"Non-manifold vert"
			" while searching region boundary");
		return NULL;
	}
	
	/*pop off current state*/
	BMW_popstate(walker);
	
	f = l->f;
	
	while (1) {
		l = BM_OtherFaceLoop(e, f, v);
		if (bmesh_radial_nextloop(l) != l) {
			l = bmesh_radial_nextloop(l);
			f = l->f;
			e = l->e;
			if(!BMO_TestFlag(walker->bm, f, walker->restrictflag)){
				l = l->radial.next->data;
				break;
			}
		} else {
			f = l->f;
			e = l->e;
			break;
		}
	}
	
	if (l == owalk.curloop) return NULL;
	if (BLI_ghash_haskey(walker->visithash, l)) return owalk.curloop;

	BLI_ghash_insert(walker->visithash, l, NULL);
	BMW_pushstate(walker);
	iwalk = walker->currentstate;
	iwalk->base = owalk.base;

	//if (!BMO_TestFlag(walker->bm, l->f, walker->restrictflag))
	//	iwalk->curloop = l->radial.next->data;
	iwalk->curloop = l; //else iwalk->curloop = l;
	iwalk->lastv = v;				

	return owalk.curloop;
}


/*	Island Walker:
 *
 *	Starts at a tool flagged-face and walks over the face region
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 * 
*/

static void islandWalker_begin(BMWalker *walker, void *data){
	islandWalker *iwalk = NULL;

	BMW_pushstate(walker);

	iwalk = walker->currentstate;
	BLI_ghash_insert(walker->visithash, data, NULL);

	iwalk->cur = data;
}

static void *islandWalker_yield(BMWalker *walker)
{
	islandWalker *iwalk = walker->currentstate;

	return iwalk->cur;
}

static void *islandWalker_step(BMWalker *walker)
{
	islandWalker *iwalk = walker->currentstate, *owalk;
	BMIter iter, liter;
	BMFace *f, *curf = iwalk->cur;
	BMLoop *l;
	owalk = iwalk;
	
	BMW_popstate(walker);

	l = BMIter_New(&liter, walker->bm, BM_LOOPS_OF_FACE, iwalk->cur);
	for (; l; l=BMIter_Step(&liter)) {
		f = BMIter_New(&iter, walker->bm, BM_FACES_OF_EDGE, l->e);
		for (; f; f=BMIter_Step(&iter)) {
			if (!BMO_TestFlag(walker->bm, f, walker->restrictflag))
				continue;
			if (BLI_ghash_haskey(walker->visithash, f)) continue;
			
			BMW_pushstate(walker);
			iwalk = walker->currentstate;
			iwalk->cur = f;
			BLI_ghash_insert(walker->visithash, f, NULL);
			break;

		}
	}
	
	return curf;
}


/*	Island Walker:
 *
 *	Starts at a tool flagged-face and walks over the face region
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 * 
*/

static void loopWalker_begin(BMWalker *walker, void *data){
	loopWalker *lwalk = NULL, owalk;
	BMEdge *e = data;
	BMVert *v;
	int found=1, val;

	v = e->v1;

	val = BM_Vert_EdgeCount(v);

	BMW_pushstate(walker);
	
	lwalk = walker->currentstate;
	BLI_ghash_insert(walker->visithash, e, NULL);
	
	lwalk->cur = lwalk->start = e;
	lwalk->lastv = lwalk->startv = v;
	lwalk->stage2 = 0;
	lwalk->startrad = BM_Edge_FaceCount(e);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((loopWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	*lwalk = owalk;

	if (lwalk->lastv == owalk.cur->v1) lwalk->lastv = owalk.cur->v2;
	else lwalk->lastv = owalk.cur->v1;

	lwalk->startv = lwalk->lastv;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	BLI_ghash_insert(walker->visithash, owalk.cur, NULL);
}

static void *loopWalker_yield(BMWalker *walker)
{
	loopWalker *lwalk = walker->currentstate;

	return lwalk->cur;
}

static void *loopWalker_step(BMWalker *walker)
{
	loopWalker *lwalk = walker->currentstate, owalk;
	BMEdge *e = lwalk->cur, *nexte = NULL;
	BMLoop *l, *l2;
	BMVert *v;
	int val, rlen, found=0, i=0, stopi;

	owalk = *lwalk;
	
	if (e->v1 == lwalk->lastv) v = e->v2;
	else v = e->v1;

	val = BM_Vert_EdgeCount(v);
	
	BMW_popstate(walker);
	
	rlen = owalk.startrad;
	l = e->loop;
	if (!l)
		return owalk.cur;

	if (val == 4 || val == 2 || rlen == 1) {		
		i = 0;
		stopi = val / 2;
		while (1) {
			if (rlen != 1 && i == stopi) break;

			l = BM_OtherFaceLoop(l->e, l->f, v);

			if (!l)
				break;

			l2 = bmesh_radial_nextloop(l);
			
			if (l2 == l) {
				break;
			}

			l = l2;
			i += 1;
		}
	}
	
	if (!l)
		return owalk.cur;

	if (l != e->loop && !BLI_ghash_haskey(walker->visithash, l->e)) {
		if (!(rlen != 1 && i != stopi)) {
			BMW_pushstate(walker);
			lwalk = walker->currentstate;
			*lwalk = owalk;
			lwalk->cur = l->e;
			lwalk->lastv = v;
			BLI_ghash_insert(walker->visithash, l->e, NULL);
		}
	}
	
	return owalk.cur;
}

static void faceloopWalker_begin(BMWalker *walker, void *data)
{
	faceloopWalker *lwalk, owalk;
	BMEdge *e = data;

	BMW_pushstate(walker);

	if (!e->loop) return;

	lwalk = walker->currentstate;
	lwalk->l = e->loop;
	lwalk->nocalc = 0;
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((faceloopWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	*lwalk = owalk;
	lwalk->nocalc = 0;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);
}

static void *faceloopWalker_yield(BMWalker *walker)
{
	faceloopWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;

	return lwalk->l->f;
}

static void *faceloopWalker_step(BMWalker *walker)
{
	faceloopWalker *lwalk = walker->currentstate;
	BMFace *f = lwalk->l->f;
	BMLoop *l = lwalk->l, *origl = lwalk->l;

	BMW_popstate(walker);

	l = l->radial.next->data;
	
	if (lwalk->nocalc)
		return f;

	if (BLI_ghash_haskey(walker->visithash, l->f)) {
		l = lwalk->l;
		l = l->head.next->next;
		if (l == l->radial.next->data) {
			l = l->head.prev->prev;
		}
		l = l->radial.next->data;
	}

	if (!BLI_ghash_haskey(walker->visithash, l->f)) {
		BMW_pushstate(walker);
		lwalk = walker->currentstate;
		lwalk->l = l;

		if (l->f->len != 4) {
			lwalk->nocalc = 1;
			lwalk->l = origl;
		} else
			lwalk->nocalc = 0;

		BLI_ghash_insert(walker->visithash, l->f, NULL);
	}

	return f;
}

static void edgeringWalker_begin(BMWalker *walker, void *data)
{
	edgeringWalker *lwalk, owalk;
	BMEdge *e = data;

	if (!e->loop) return;

	BMW_pushstate(walker);

	lwalk = walker->currentstate;
	lwalk->l = e->loop;
	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((edgeringWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	*lwalk = owalk;

	if (lwalk->l->f->len != 4)
		lwalk->l = lwalk->l->radial.next->data;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);
}

static void *edgeringWalker_yield(BMWalker *walker)
{
	edgeringWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;

	return lwalk->l->e;
}

static void *edgeringWalker_step(BMWalker *walker)
{
	edgeringWalker *lwalk = walker->currentstate;
	BMEdge *e = lwalk->l->e;
	BMLoop *l = lwalk->l, *origl = lwalk->l;

	BMW_popstate(walker);

	l = l->radial.next->data;
	l = l->head.next->next;
	
	if (l->f->len != 4) {
		l = lwalk->l->head.next->next;
	}

	if (l->f->len == 4 && !BLI_ghash_haskey(walker->visithash, l->e)) {
		BMW_pushstate(walker);
		lwalk = walker->currentstate;
		lwalk->l = l;

		BLI_ghash_insert(walker->visithash, l->e, NULL);
	}

	return e;
}

static void uvedgeWalker_begin(BMWalker *walker, void *data)
{
	uvedgeWalker *lwalk;
	BMLoop *l = data;

	if (BLI_ghash_haskey(walker->visithash, l))
		return;

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	lwalk->l = l;
	BLI_ghash_insert(walker->visithash, l, NULL);
}

static void *uvedgeWalker_yield(BMWalker *walker)
{
	uvedgeWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;
	
	return lwalk->l;
}

static void *uvedgeWalker_step(BMWalker *walker)
{
	uvedgeWalker *lwalk = walker->currentstate;
	BMLoop *l, *l2, *l3, *nl, *cl;
	BMIter liter;
	void *d1, *d2;
	int i, j, rlen, type;

	l = lwalk->l;
	nl = l->head.next;
	type = walker->bm->ldata.layers[walker->flag].type;

	BMW_popstate(walker);
	
	if (walker->restrictflag && !BMO_TestFlag(walker->bm, l->e, walker->restrictflag))
		return l;

	/*go over loops around l->v and nl->v and see which ones share l and nl's 
	  mloopuv's coordinates. in addition, push on l->head.next if necassary.*/
	for (i=0; i<2; i++) {
		cl = i ? nl : l;
		BM_ITER(l2, &liter, walker->bm, BM_LOOPS_OF_VERT, cl->v) {
			d1 = CustomData_bmesh_get_layer_n(&walker->bm->ldata, 
			             cl->head.data, walker->flag);
			
			rlen = BM_Edge_FaceCount(l2->e);
			for (j=0; j<rlen; j++) {
				if (BLI_ghash_haskey(walker->visithash, l2))
					continue;
				if (walker->restrictflag && !(BMO_TestFlag(walker->bm, l2->e, walker->restrictflag)))
				{
					if (l2->v != cl->v)
						continue;
				}
				
				l3 = l2->v != cl->v ? (BMLoop*)l2->head.next : l2;
				d2 = CustomData_bmesh_get_layer_n(&walker->bm->ldata, 
					     l3->head.data, walker->flag);

				if (!CustomData_data_equals(type, d1, d2))
					continue;
				
				BMW_pushstate(walker);
				BLI_ghash_insert(walker->visithash, l2, NULL);
				lwalk = walker->currentstate;

				lwalk->l = l2;

				l2 = l2->radial.next->data;
			}
		}
	}

	return l;
}

