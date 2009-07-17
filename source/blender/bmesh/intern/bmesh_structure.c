/**
 * bmesh_structure.c    jan 2007
 *
 *	Low level routines for manipulating the BM structure.
 *
 * $Id: bmesh_structure.c,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <limits.h>
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "BKE_utildefines.h"
#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_ghash.h"
/**
 *	MISC utility functions.
 *
 */
 
int bmesh_vert_in_edge(BMEdge *e, BMVert *v){
	if(e->v1 == v || e->v2 == v) return 1;
	return 0;
}
int bmesh_verts_in_edge(BMVert *v1, BMVert *v2, BMEdge *e){
	if(e->v1 == v1 && e->v2 == v2) return 1;
	else if(e->v1 == v2 && e->v2 == v1) return 1;
	return 0;
}

BMVert *bmesh_edge_getothervert(BMEdge *e, BMVert *v){	
	if(e->v1 == v) return e->v2;
	else if(e->v2 == v) return e->v1;
	return NULL;
}

int bmesh_edge_swapverts(BMEdge *e, BMVert *orig, BMVert *new){
	if(e->v1 == orig){ 
		e->v1 = new;
		e->d1.next = NULL;
		e->d1.prev = NULL;
		return 1;
	}
	else if(e->v2 == orig){
		e->v2 = new;
		e->d2.next = NULL;
		e->d2.prev = NULL;
		return 1;
	}
	return 0;
}

/**
 *	ALLOCATION/DEALLOCATION FUNCTIONS
 */

BMVert *bmesh_addvertlist(BMesh *bm, BMVert *example){
	BMVert *v=NULL;
	v = BLI_mempool_calloc(bm->vpool);
	v->head.next = v->head.prev = NULL;
	v->head.flag = 0;
	v->head.EID = bm->nextv;
	v->head.type = BM_VERT;
	v->co[0] = v->co[1] = v->co[2] = 0.0f;
	v->no[0] = v->no[1] = v->no[2] = 0.0f;
	v->edge = NULL;
	v->head.data = NULL;
	v->bweight = 0.0f;
	BLI_addtail(&(bm->verts), &(v->head));
	bm->nextv++;
	bm->totvert++;

	if(example){
		VECCOPY(v->co,example->co);
		CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, example->head.data, &v->head.data);
	}
	else
		CustomData_bmesh_set_default(&bm->vdata, &v->head.data);

	/*allocate flags*/
	v->head.flags = BLI_mempool_calloc(bm->flagpool);
	
	return v;
}
BMEdge *bmesh_addedgelist(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge *example){
	BMEdge *e=NULL;
	e = BLI_mempool_calloc(bm->epool);
	e->head.next = e->head.prev = NULL;
	e->head.EID = bm->nexte;
	e->head.type = BM_EDGE;
	e->head.flag = 0;
	e->v1 = v1;
	e->v2 = v2;
	e->d1.next = e->d1.prev = e->d2.next = e->d2.prev = NULL;
	e->d1.data = e;
	e->d2.data = e;
	e->loop = NULL;
	e->head.data = NULL;
	e->crease = e->bweight = 0.0f;
	bm->nexte++;
	bm->totedge++;
	BLI_addtail(&(bm->edges), &(e->head));
	
	if(example)
		CustomData_bmesh_copy_data(&bm->edata, &bm->edata, example->head.data, &e->head.data);
	else
		CustomData_bmesh_set_default(&bm->edata, &e->head.data);

	/*allocate flags*/
	e->head.flags = BLI_mempool_calloc(bm->flagpool);

	return e;
}
BMLoop *bmesh_create_loop(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f, BMLoop *example){
	BMLoop *l=NULL;
	l = BLI_mempool_calloc(bm->lpool);
	l->head.next = l->head.prev = NULL;
	l->head.EID = bm->nextl;
	l->head.type = BM_LOOP;
	l->head.flag = 0;
	l->radial.next = l->radial.prev = NULL;
	l->radial.data = l;
	l->v = v;
	l->e = e;
	l->f = f;
	l->head.data = NULL;
	bm->nextl++;
	bm->totloop++;
	
	if(example)
		CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
	else
		CustomData_bmesh_set_default(&bm->ldata, &l->head.data);

	return l;
}

BMFace *bmesh_addpolylist(BMesh *bm, BMFace *example){
	BMFace *f = NULL;
	f = BLI_mempool_calloc(bm->ppool);
	f->head.flag = 0;
	f->head.next = f->head.prev = NULL;
	f->head.EID = bm->nextp;
	f->head.type = BM_FACE;
	f->loopbase = NULL;
	f->len = 0;
	f->head.data = NULL;
	f->mat_nr = 0;
	BLI_addtail(&(bm->polys),&(f->head));
	bm->nextp++;
	bm->totface++;

	if(example)
		CustomData_bmesh_copy_data(&bm->pdata, &bm->pdata, example->head.data, &f->head.data);
	else
		CustomData_bmesh_set_default(&bm->pdata, &f->head.data);

	/*allocate flags*/
	f->head.flags = BLI_mempool_calloc(bm->flagpool);

	return f;
}

/*	free functions dont do much *yet*. When per-vertex, per-edge and per-face/faceloop
	data is added though these will be needed.
*/
void bmesh_free_vert(BMesh *bm, BMVert *v){
	bm->totvert--;
	BM_remove_selection(bm, v);

	CustomData_bmesh_free_block(&bm->vdata, &v->head.data);
	BLI_mempool_free(bm->flagpool, v->head.flags);
	BLI_mempool_free(bm->vpool, v);
}
void bmesh_free_edge(BMesh *bm, BMEdge *e){
	bm->totedge--;
	BM_remove_selection(bm, e);

	CustomData_bmesh_free_block(&bm->edata, &e->head.data);
	BLI_mempool_free(bm->flagpool, e->head.flags);
	BLI_mempool_free(bm->epool, e);
}
void bmesh_free_poly(BMesh *bm, BMFace *f){
	if (f == bm->act_face)
		bm->act_face = NULL;
	BM_remove_selection(bm, f);

	bm->totface--;
	CustomData_bmesh_free_block(&bm->pdata, &f->head.data);
	BLI_mempool_free(bm->flagpool, f->head.flags);
	BLI_mempool_free(bm->ppool, f);
}
void bmesh_free_loop(BMesh *bm, BMLoop *l){
	bm->totloop--;
	CustomData_bmesh_free_block(&bm->ldata, &l->head.data);
	BLI_mempool_free(bm->lpool, l);
}
/**
 *	BMESH CYCLES
 *
 *	Cycles are circular doubly linked lists that form the basis of adjacency
 *	information in the BME modeller. Full adjacency relations can be derived
 *	from examining these cycles very quickly. Although each cycle is a double
 *  circular linked list, each one is considered to have a 'base' or 'head',
 *	and care must be taken by Euler code when modifying the contents of a cycle.
 *
 *	The contents of this file are split into two parts. First there are the 
 *	bmesh_cycle family of functions which are generic circular double linked list 
 *	procedures. The second part contains higher level procedures for supporting 
 *	modification of specific cycle types.
 *
 *	The three cycles explicitly stored in the BM data structure are as follows:
 *
 *	1: The Disk Cycle - A circle of edges around a vertex
 *     Base: vertex->edge pointer.
 *	   
 *     This cycle is the most complicated in terms of its structure. Each bmesh_Edge contains	
 *	   two bmesh_CycleNode structures to keep track of that edge's membership in the disk cycle
 *	   of each of its vertices. However for any given vertex it may be the first in some edges
 *	   in its disk cycle and the second for others. The bmesh_disk_XXX family of functions contain
 *	   some nice utilities for navigating disk cycles in a way that hides this detail from the 
 *	   tool writer.
 *
 *		Note that the disk cycle is completley independant from face data. One advantage of this
 *		is that wire edges are fully integrated into the topology database. Another is that the 
 *	    the disk cycle has no problems dealing with non-manifold conditions involving faces.
 *
 *		Functions relating to this cycle:
 *		
 *			bmesh_disk_append_edge
 *			bmesh_disk_remove_edge
 *			bmesh_disk_nextedge
 *			bmesh_disk_getpointer
 *
 *	2: The Radial Cycle - A circle of face edges (bmesh_Loop) around an edge
 *	   Base: edge->loop->radial structure.
 *
 *		The radial cycle is similar to the radial cycle in the radial edge data structure.*
 *		Unlike the radial edge however, the radial cycle does not require a large amount of memory 
 *		to store non-manifold conditions since BM does not keep track of region/shell
 *		information.
 *		
 *		Functions relating to this cycle:
 *			
 *			bmesh_radial_append
 *			bmesh_radial_remove_loop
 *			bmesh_radial_nextloop
 *			bmesh_radial_find_face
 *		
 *
 *	3: The Loop Cycle - A circle of face edges around a polygon.
 *     Base: polygon->loopbase.
 *
 *	   The loop cycle keeps track of a faces vertices and edges. It should be noted that the
 *     direction of a loop cycle is either CW or CCW depending on the face normal, and is 
 *     not oriented to the faces editedges. 
 *
 *		Functions relating to this cycle:
 *		
 *			bmesh_cycle_XXX family of functions.
 *
 *	
 *	Note that the order of elements in all cycles except the loop cycle is undefined. This 
 *  leads to slightly increased seek time for deriving some adjacency relations, however the 
 *  advantage is that no intrinsic properties of the data structures are dependant upon the 
 *  cycle order and all non-manifold conditions are represented trivially.
 *
*/
 
 
void bmesh_cycle_append(void *h, void *nt)
{
	BMNode *oldtail, *head, *newtail;
	
	head = (BMNode*)h;
	newtail = (BMNode*)nt;
	
	if(head->next == NULL){
		head->next = newtail;
		head->prev = newtail;
		newtail->next = head;
		newtail->prev = head;
	}
	else{
		oldtail = head->prev;
		oldtail->next = newtail;
		newtail->next = head;
		newtail->prev = oldtail;
		head->prev = newtail;
		
	}
}

/**
 *			bmesh_cycle_length
 *
 *	Count the nodes in a cycle.
 *
 *  Returns -
 *	Integer
 */

int bmesh_cycle_length(void *h){
	
	int len = 0;
	BMNode *head, *curnode;
	head = (BMNode*)h;
	
	if(head){ 
		len = 1;
		for(curnode = head->next; curnode != head; curnode=curnode->next){ 
			if(len == INT_MAX){ //check for infinite loop/corrupted cycle
					return -1;
			}
			len++;
		}
	}
	return len;
}


/**
 *			bmesh_cycle_remove
 *
 *	Removes a node from a cycle.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */

int bmesh_cycle_remove(void *h, void *remn)
{
	int i, len;
	BMNode *head, *remnode, *curnode;
	
	head = (BMNode*)h;
	remnode = (BMNode*)remn;
	len = bmesh_cycle_length(h);
	
	if(len == 1 && head == remnode){
		head->next = NULL;
		head->prev = NULL;
		return 1;
	}
	else{
		for(i=0, curnode = head; i < len; curnode = curnode->next){
			if(curnode == remnode){
				remnode->prev->next = remnode->next;
				remnode->next->prev = remnode->prev;
				/*zero out remnode pointers, important!*/
				//remnode->next = NULL;
				//remnode->prev = NULL;
				return 1;
		
			}
		}
	}
	return 0;
}

/**
 *			bmesh_cycle_validate
 *
 *	Validates a cycle. Takes as an argument the expected length of the cycle and
 *	a pointer to the cycle head or base.
 *
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */

int bmesh_cycle_validate(int len, void *h){
	int i;
	BMNode *curnode, *head;
	head = (BMNode*)h;
	
	/*forward validation*/
	for(i = 0, curnode = head; i < len; i++, curnode = curnode->next);
	if(curnode != head) return 0;
	
	/*reverse validation*/
	for(i = 0, curnode = head; i < len; i++, curnode = curnode->prev);
	if(curnode != head) return 0;
	
	return 1;
}

/*Begin Disk Cycle routines*/

/**
 *			bmesh_disk_nextedge
 *
 *	Find the next edge in a disk cycle
 *
 *  Returns -
 *	Pointer to the next edge in the disk cycle for the vertex v.
 */
 
BMEdge *bmesh_disk_nextedge(BMEdge *e, BMVert *v)
{	
	if(bmesh_vert_in_edge(e, v)){
		if(e->v1 == v) return e->d1.next->data;
		else if(e->v2 == v) return e->d2.next->data;
	}
	return NULL;
}

/**
 *			bmesh_disk_getpointer
 *
 *	Given an edge and one of its vertices, find the apporpriate CycleNode
 *
 *  Returns -
 *	Pointer to bmesh_CycleNode.
 */
BMNode *bmesh_disk_getpointer(BMEdge *e, BMVert *v){
	/*returns pointer to the cycle node for the appropriate vertex in this disk*/
	if(e->v1 == v) return &(e->d1);
	else if (e->v2 == v) return &(e->d2);
	return NULL;
}

/**
 *			bmesh_disk_append_edge
 *
 *	Appends edge to the end of a vertex disk cycle.
 *
 *  Returns -
 *	1 for success, 0 for failure
 */

int bmesh_disk_append_edge(BMEdge *e, BMVert *v)
{ 
	
	BMNode *base, *tail;
	
	if(bmesh_vert_in_edge(e, v) == 0) return 0; /*check to make sure v is in e*/
	
	/*check for loose vert first*/
	if(v->edge == NULL){
		v->edge = e;
		base = tail = bmesh_disk_getpointer(e, v);
		bmesh_cycle_append(base, tail); /*circular reference is ok!*/
		return 1;
	}
	
	/*insert e at the end of disk cycle and make it the new v->edge*/
	base = bmesh_disk_getpointer(v->edge, v);
	tail = bmesh_disk_getpointer(e, v);
	bmesh_cycle_append(base, tail);
	return 1;
}

/**
 *			bmesh_disk_remove_edge
 *
 *	Removes an edge from a disk cycle. If the edge to be removed is
 *	at the base of the cycle, the next edge becomes the new base.
 *
 *
 *  Returns -
 *	Nothing
 */

void bmesh_disk_remove_edge(BMEdge *e, BMVert *v)
{
	BMNode *base, *remnode;
	BMEdge *newbase;
	int len;
	
	base = bmesh_disk_getpointer(v->edge, v);
	remnode = bmesh_disk_getpointer(e, v);
	
	/*first deal with v->edge pointer...*/
	len = bmesh_cycle_length(base);
	if(len == 1) newbase = NULL;
	else if(v->edge == e) newbase = base->next-> data;
	else newbase = v->edge;
	
	/*remove and rebase*/
	bmesh_cycle_remove(base, remnode);
	v->edge = newbase;
}

/**
 *			bmesh_disk_next_edgeflag
 *
 *	Searches the disk cycle of v, starting with e, for the 
 *  next edge that has either eflag or tflag.
 *
 *	bmesh_Edge pointer.
 */

BMEdge *bmesh_disk_next_edgeflag(BMEdge *e, BMVert *v, int eflag, int tflag)
{
	
	BMNode *diskbase;
	BMEdge *curedge;
	int len, ok;
	
	if(eflag && tflag) return NULL;
	
	ok = bmesh_vert_in_edge(e,v);
	if(ok){
		diskbase = bmesh_disk_getpointer(e, v);
		len = bmesh_cycle_length(diskbase);
		curedge = bmesh_disk_nextedge(e,v);
		while(curedge != e){
			if(eflag){
				if(curedge->head.eflag1 == eflag) return curedge;
			}
			curedge = bmesh_disk_nextedge(curedge, v);
		}
	}
	return NULL;
}

/**
 *			bmesh_disk_count_edgeflag
 *
 *	Counts number of edges in this verts disk cycle which have 
 *	either eflag or tflag (but not both!)
 *
 *  Returns -
 *	Integer.
 */

int bmesh_disk_count_edgeflag(BMVert *v, int eflag, int tflag)
{
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len=0, count=0;
	
	if(v->edge){
		if(eflag && tflag) return 0; /*tflag and eflag are reserved for different functions!*/
		diskbase = bmesh_disk_getpointer(v->edge, v);
		len = bmesh_cycle_length(diskbase);
		
		for(i = 0, curedge=v->edge; i<len; i++){
			if(eflag){
				if(curedge->head.eflag1 == eflag) count++;
			}
			curedge = bmesh_disk_nextedge(curedge, v);
		}
	}
	return count;
}


int bmesh_disk_hasedge(BMVert *v, BMEdge *e){
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len=0;
	
	if(v->edge){
		diskbase = bmesh_disk_getpointer(v->edge,v);
		len = bmesh_cycle_length(diskbase);
		
		for(i = 0, curedge=v->edge; i<len; i++){
			if(curedge == e) return 1;
			else curedge=bmesh_disk_nextedge(curedge, v);
		}
	}
	return 0;
}

BMEdge *bmesh_disk_existedge(BMVert *v1, BMVert *v2){
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len=0;
	
	if(v1->edge){
		diskbase = bmesh_disk_getpointer(v1->edge,v1);
		len = bmesh_cycle_length(diskbase);
		
		for(i=0,curedge=v1->edge;i<len;i++,curedge = bmesh_disk_nextedge(curedge,v1)){
			if(bmesh_verts_in_edge(v1,v2,curedge)) return curedge;
		}
	}
	
	return NULL;
}

/*end disk cycle routines*/

BMLoop *bmesh_radial_nextloop(BMLoop *l){
	return (BMLoop*)(l->radial.next->data);
}

void bmesh_radial_append(BMEdge *e, BMLoop *l){
	if(e->loop == NULL) e->loop = l;
	bmesh_cycle_append(&(e->loop->radial), &(l->radial));
}

void bmesh_radial_remove_loop(BMLoop *l, BMEdge *e)
{
	BMLoop *newbase;
	int len;
	
	/*deal with edge->loop pointer*/
	len = bmesh_cycle_length(&(e->loop->radial));
	if(len == 1) newbase = NULL;
	else if(e->loop == l) newbase = e->loop->radial.next->data;
	else newbase = e->loop;
	
	/*remove and rebase*/
	bmesh_cycle_remove(&(e->loop->radial), &(l->radial));
	e->loop = newbase;
}

int bmesh_radial_find_face(BMEdge *e,BMFace *f)
{
		
	BMLoop *curloop;
	int i, len;
	
	len = bmesh_cycle_length(&(e->loop->radial));
	for(i = 0, curloop = e->loop; i < len; i++, curloop = curloop->radial.next->data){
		if(curloop->f == f) return 1;
	}
	return 0;
}


/*
 * BME RADIAL COUNT FACE VERT
 *
 * Returns the number of times a vertex appears
 * in a radial cycle
 *
*/

int bmesh_radial_count_facevert(BMLoop *l, BMVert *v)
{
	BMLoop *curloop;
	int count = 0;
	curloop = l;
	do{
		if(curloop->v == v) count++;
		curloop = bmesh_radial_nextloop(curloop);
	}while(curloop != l);
	return count;
}

/*
 * BME DISK COUNT FACE VERT
 *
 * Counts the number of loop users
 * for this vertex. Note that this is
 * equivalent to counting the number of
 * faces incident upon this vertex
 *
*/

int bmesh_disk_count_facevert(BMVert *v)
{
	BMEdge *curedge;
	int count = 0;

	/*is there an edge on this vert at all?*/
	if(!v->edge)
		return count;

	/*first, loop around edges*/
	curedge = v->edge;
	do{
		if(curedge->loop) count += bmesh_radial_count_facevert(curedge->loop, v); 
		curedge = bmesh_disk_nextedge(curedge, v);
	}while(curedge != v->edge);

	return count;
}

/*
 * BME RADIAL FIND FIRST FACE VERT
 *
 * Finds the first loop of v around radial
 * cycle
 *
*/
BMLoop *bmesh_radial_find_first_facevert(BMLoop *l, BMVert *v)
{
	BMLoop *curloop;
	curloop = l;
	do{
		if(curloop->v == v) return curloop;
		curloop = bmesh_radial_nextloop(curloop);
	}while(curloop != l);
	return NULL;
}

BMLoop *bmesh_radial_find_next_facevert(BMLoop *l, BMVert *v)
{
	BMLoop *curloop;
	curloop = bmesh_radial_nextloop(l);
	do{
		if(curloop->v == v) return curloop;
		curloop = bmesh_radial_nextloop(curloop);
	}while(curloop !=l);
	return l;
}


/*
 * BME FIND FIRST FACE EDGE
 *
 * Finds the first edge in a vertices
 * Disk cycle that has one of this
 * vert's loops attached
 * to it.
 *
 *
*/

BMEdge *bmesh_disk_find_first_faceedge(BMEdge *e, BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = e;
	do{
		if(searchedge->loop && bmesh_radial_count_facevert(searchedge->loop,v)) return searchedge;
		searchedge = bmesh_disk_nextedge(searchedge,v);
	}while(searchedge != e);
	
	return NULL;
}

BMEdge *bmesh_disk_find_next_faceedge(BMEdge *e, BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = bmesh_disk_nextedge(e,v);
	do{
		if(searchedge->loop && bmesh_radial_count_facevert(searchedge->loop,v)) return searchedge;
		searchedge = bmesh_disk_nextedge(searchedge,v);
	}while(searchedge !=e);
	return e;
}





struct BMLoop *bmesh_loop_find_loop(struct BMFace *f, struct BMVert *v) {
	BMLoop *l;
	int i, len;
	
	len = bmesh_cycle_length(f->loopbase);
	for (i = 0, l=f->loopbase; i < len; i++, l=((BMLoop*)(l->head.next)) ) {
		if (l->v == v) return l;
	}
	return NULL;
}
