/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/bmesh/intern/bmesh_structure.c
 *  \ingroup bmesh
 *
 * Low level routines for manipulating the BM structure.
 */

#include "bmesh.h"
#include "bmesh_private.h"

/**
 *	MISC utility functions.
 *
 */

int bmesh_vert_in_edge(BMEdge *e, BMVert *v)
{
	if (e->v1 == v || e->v2 == v) return TRUE;
	return FALSE;
}
int bmesh_verts_in_edge(BMVert *v1, BMVert *v2, BMEdge *e)
{
	if (e->v1 == v1 && e->v2 == v2) return TRUE;
	else if (e->v1 == v2 && e->v2 == v1) return TRUE;
	return FALSE;
}

BMVert *bmesh_edge_getothervert(BMEdge *e, BMVert *v) {
	if (e->v1 == v) {
		return e->v2;
	}
	else if (e->v2 == v) {
		return e->v1;
	}
	return NULL;
}

int bmesh_edge_swapverts(BMEdge *e, BMVert *orig, BMVert *newv)
{
	if (e->v1 == orig) {
		e->v1 = newv;
		e->v1_disk_link.next = e->v1_disk_link.prev = NULL;
		return TRUE;
	}
	else if (e->v2 == orig) {
		e->v2 = newv;
		e->v2_disk_link.next = e->v2_disk_link.prev = NULL;
		return TRUE;
	}
	return FALSE;
}

/**
 *	BMESH CYCLES
 * (this is somewhat outdate, though bits of its API are still used) - joeedh
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
 *	   Base: edge->l->radial structure.
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
 *     Base: polygon->lbase.
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
int bmesh_disk_append_edge(struct BMEdge *e, struct BMVert *v)
{
	if (!v->e) {
		BMDiskLink *dl1 = BM_EDGE_DISK_LINK_GET(e, v);

		v->e = e;
		dl1->next = dl1->prev = e;
	}
	else {
		BMDiskLink *dl1, *dl2, *dl3;

		dl1 = BM_EDGE_DISK_LINK_GET(e, v);
		dl2 = BM_EDGE_DISK_LINK_GET(v->e, v);
		dl3 = dl2->prev ? BM_EDGE_DISK_LINK_GET(dl2->prev, v) : NULL;

		dl1->next = v->e;
		dl1->prev = dl2->prev;

		dl2->prev = e;
		if (dl3)
			dl3->next = e;
	}

	return TRUE;
}

void bmesh_disk_remove_edge(struct BMEdge *e, struct BMVert *v)
{
	BMDiskLink *dl1, *dl2;

	dl1 = BM_EDGE_DISK_LINK_GET(e, v);
	if (dl1->prev) {
		dl2 = BM_EDGE_DISK_LINK_GET(dl1->prev, v);
		dl2->next = dl1->next;
	}

	if (dl1->next) {
		dl2 = BM_EDGE_DISK_LINK_GET(dl1->next, v);
		dl2->prev = dl1->prev;
	}

	if (v->e == e)
		v->e = (e != (BMEdge *)dl1->next) ? (BMEdge *)dl1->next : NULL;

	dl1->next = dl1->prev = NULL;
}

struct BMEdge *bmesh_disk_nextedge(struct BMEdge *e, struct BMVert *v)
{
	if (v == e->v1)
		return e->v1_disk_link.next;
	if (v == e->v2)
		return e->v2_disk_link.next;
	return NULL;
}

static BMEdge *bmesh_disk_prevedge(BMEdge *e, BMVert *v)
{
	if (v == e->v1)
		return e->v1_disk_link.prev;
	if (v == e->v2)
		return e->v2_disk_link.prev;
	return NULL;
}

BMEdge *bmesh_disk_existedge(BMVert *v1, BMVert *v2)
{
	BMEdge *curedge, *startedge;
	
	if (v1->e) {
		startedge = v1->e;
		curedge = startedge;
		do {
			if (bmesh_verts_in_edge(v1, v2, curedge)) {
				return curedge;
			}

			curedge = bmesh_disk_nextedge(curedge, v1);
		} while (curedge != startedge);
	}
	
	return NULL;
}

int bmesh_disk_count(struct BMVert *v)
{
	BMEdge *e = v->e;
	int i = 0;

	if (!e) {
		return 0;
	}

	do {
		if (!e) {
			return 0;
		}

		e =  bmesh_disk_nextedge(e, v);

		if (i >= (1 << 20)) {
			printf("bmesh error: infinite loop in disk cycle!\n");
			return 0;
		}

		i++;
	} while (e != v->e);

	return i;
}

int bmesh_disk_validate(int len, BMEdge *e, BMVert *v)
{
	BMEdge *e2;

	if (!BM_vert_in_edge(e, v))
		return FALSE;
	if (bmesh_disk_count(v) != len || len == 0)
		return FALSE;

	e2 = e;
	do {
		if (len != 1 && bmesh_disk_prevedge(e2, v) == e2) {
			return FALSE;
		}

		e2 = bmesh_disk_nextedge(e2, v);
	} while (e2 != e);

	return TRUE;
}

/*
 * BME DISK COUNT FACE VERT
 *
 * Counts the number of loop users
 * for this vertex. Note that this is
 * equivalent to counting the number of
 * faces incident upon this vertex
 */

int bmesh_disk_count_facevert(BMVert *v)
{
	BMEdge *curedge;
	int count = 0;

	/* is there an edge on this vert at all */
	if (!v->e)
		return count;

	/* first, loop around edge */
	curedge = v->e;
	do {
		if (curedge->l) count += bmesh_radial_count_facevert(curedge->l, v);
		curedge = bmesh_disk_nextedge(curedge, v);
	} while (curedge != v->e);

	return count;
}

struct BMEdge *bmesh_disk_find_first_faceedge(struct BMEdge *e, struct BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = e;
	do {
		if (searchedge->l && bmesh_radial_count_facevert(searchedge->l, v)) {
			return searchedge;
		}

		searchedge = bmesh_disk_nextedge(searchedge, v);
	} while (searchedge != e);

	return NULL;
}

struct BMEdge *bmesh_disk_find_next_faceedge(struct BMEdge *e, struct BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = bmesh_disk_nextedge(e, v);
	do {
		if (searchedge->l && bmesh_radial_count_facevert(searchedge->l, v)) {
			return searchedge;
		}
		searchedge = bmesh_disk_nextedge(searchedge, v);
	} while (searchedge != e);
	return e;
}

/*****radial cycle functions, e.g. loops surrounding edges**** */
int bmesh_radial_validate(int radlen, BMLoop *l)
{
	BMLoop *l_iter = l;
	int i = 0;
	
	if (bmesh_radial_length(l) != radlen)
		return FALSE;

	do {
		if (!l_iter) {
			bmesh_error();
			return FALSE;
		}
		
		if (l_iter->e != l->e)
			return FALSE;
		if (l_iter->v != l->e->v1 && l_iter->v != l->e->v2)
			return FALSE;
		
		if (i > BM_LOOP_RADIAL_MAX) {
			bmesh_error();
			return FALSE;
		}
		
		i++;
	} while ((l_iter = bmesh_radial_nextloop(l_iter)) != l);

	return TRUE;
}

/*
 * BMESH RADIAL REMOVE LOOP
 *
 * Removes a loop from an radial cycle. If edge e is non-NULL
 * it should contain the radial cycle, and it will also get
 * updated (in the case that the edge's link into the radial
 * cycle was the loop which is being removed from the cycle).
 */
void bmesh_radial_remove_loop(BMLoop *l, BMEdge *e)
{
	/* if e is non-NULL, l must be in the radial cycle of e */
	if (e && e != l->e) {
		bmesh_error();
	}

	if (l->radial_next != l) {
		if (e && l == e->l)
			e->l = l->radial_next;

		l->radial_next->radial_prev = l->radial_prev;
		l->radial_prev->radial_next = l->radial_next;
	}
	else {
		if (e) {
			if (l == e->l) {
				e->l = NULL;
			}
			else {
				bmesh_error();
			}
		}
	}

	/* l is no longer in a radial cycle; empty the links
	 * to the cycle and the link back to an edge */
	l->radial_next = l->radial_prev = NULL;
	l->e = NULL;
}


/*
 * BME RADIAL FIND FIRST FACE VERT
 *
 * Finds the first loop of v around radial
 * cycle
 */
BMLoop *bmesh_radial_find_first_faceloop(BMLoop *l, BMVert *v)
{
	BMLoop *l_iter;
	l_iter = l;
	do {
		if (l_iter->v == v) {
			return l_iter;
		}
	} while ((l_iter = bmesh_radial_nextloop(l_iter)) != l);
	return NULL;
}

BMLoop *bmesh_radial_find_next_faceloop(BMLoop *l, BMVert *v)
{
	BMLoop *l_iter;
	l_iter = bmesh_radial_nextloop(l);
	do {
		if (l_iter->v == v) {
			return l_iter;
		}
	} while ((l_iter = bmesh_radial_nextloop(l_iter)) != l);
	return l;
}

BMLoop *bmesh_radial_nextloop(BMLoop *l)
{
	return l->radial_next;
}

int bmesh_radial_length(BMLoop *l)
{
	BMLoop *l2 = l;
	int i = 0;

	if (!l)
		return 0;

	do {
		if (!l2) {
			/* radial cycle is broken (not a circulat loop) */
			bmesh_error();
			return 0;
		}
		
		i++;
		l2 = l2->radial_next;
		if (i >= BM_LOOP_RADIAL_MAX) {
			bmesh_error();
			return -1;
		}
	} while (l2 != l);

	return i;
}

void bmesh_radial_append(BMEdge *e, BMLoop *l)
{
	if (e->l == NULL) {
		e->l = l;
		l->radial_next = l->radial_prev = l;
	}
	else {
		l->radial_prev = e->l;
		l->radial_next = e->l->radial_next;

		e->l->radial_next->radial_prev = l;
		e->l->radial_next = l;

		e->l = l;
	}

	if (l->e && l->e != e) {
		/* l is already in a radial cycle for a different edge */
		bmesh_error();
	}
	
	l->e = e;
}

int bmesh_radial_find_face(BMEdge *e, BMFace *f)
{
	BMLoop *curloop;
	int i, len;

	len = bmesh_radial_length(e->l);
	for (i = 0, curloop = e->l; i < len; i++, curloop = curloop->radial_next) {
		if (curloop->f == f)
			return TRUE;
	}
	return FALSE;
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
	BMLoop *l_iter;
	int count = 0;
	l_iter = l;
	do {
		if (l_iter->v == v) {
			count++;
		}
	} while ((l_iter = bmesh_radial_nextloop(l_iter)) != l);

	return count;
}

/*****loop cycle functions, e.g. loops surrounding a face**** */
int bmesh_loop_validate(BMFace *f)
{
	int i;
	int len = f->len;
	BMLoop *l_iter, *l_first;

	l_first = BM_FACE_FIRST_LOOP(f);

	if (l_first == NULL) {
		return FALSE;
	}

	/* Validate that the face loop cycle is the length specified by f->len */
	for (i = 1, l_iter = l_first->next; i < len; i++, l_iter = l_iter->next) {
		if ( (l_iter->f != f) ||
		     (l_iter == l_first))
		{
			return FALSE;
		}
	}
	if (l_iter != l_first) {
		return FALSE;
	}

	/* Validate the loop->prev links also form a cycle of length f->len */
	for (i = 1, l_iter = l_first->prev; i < len; i++, l_iter = l_iter->prev) {
		if (l_iter == l_first) {
			return FALSE;
		}
	}
	if (l_iter != l_first) {
		return FALSE;
	}

	return TRUE;
}


#if 0
void bmesh_cycle_append(void *h, void *nt)
{
	BMNode *oldtail, *head, *newtail;
	
	head = (BMNode *)h;
	newtail = (BMNode *)nt;
	
	if (head->next == NULL) {
		head->next = newtail;
		head->prev = newtail;
		newtail->next = head;
		newtail->prev = head;
	}
	else {
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

int bmesh_cycle_length(BMEdge *e, BMVert *v)
{
	BMEdge *next, *prev, *cur;
	int len, vi = v == e->v1 ? 0 : 1;
	
	/* should skip 2 forward if v is 1, happily reduces to (v * 2) */
	prev = *(&e->v1_prev + vi * 2);
	
	cur = e;
	len = 1;
	while (cur != prev) {
		vi = cur->v1 == v ? 0 : 1;
		
		len++;
		cur = *(&cur->v1_next + vi * 2);
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
	
	head = (BMNode *)h;
	remnode = (BMNode *)remn;
	len = bmesh_cycle_length(h);
	
	if (len == 1 && head == remnode) {
		head->next = NULL;
		head->prev = NULL;
		return TRUE;
	}
	else {
		for (i = 0, curnode = head; i < len; curnode = curnode->next) {
			if (curnode == remnode) {
				remnode->prev->next = remnode->next;
				remnode->next->prev = remnode->prev;
				/* zero out remnode pointers, important */
				//remnode->next = NULL;
				//remnode->prev = NULL;
				return TRUE;

			}
		}
	}
	return FALSE;
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

int bmesh_cycle_validate(int len, void *h)
{
	int i;
	BMNode *curnode, *head;
	head = (BMNode *)h;
	
	/* forward validatio */
	for (i = 0, curnode = head; i < len; i++, curnode = curnode->next);
	if (curnode != head) {
		return FALSE;
	}

	/* reverse validatio */
	for (i = 0, curnode = head; i < len; i++, curnode = curnode->prev);
	if (curnode != head) {
		return FALSE;
	}

	return TRUE;
}

/* Begin Disk Cycle routine */

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
	if (bmesh_vert_in_edge(e, v)) {
		if (e->v1 == v) {
			return e->d1.next->data;
		}
		else if (e->v2 == v) {
			return e->d2.next->data;
		}
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
BMNode *bmesh_disk_getpointer(BMEdge *e, BMVert *v)
{
	/* returns pointer to the cycle node for the appropriate vertex in this dis */
	if (e->v1 == v) {
		return &(e->d1);
	}
	else if (e->v2 == v) {
		return &(e->d2);
	}
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

	/* check to make sure v is in */
	if (bmesh_vert_in_edge(e, v) == 0) {
		return FALSE;
	}
	
	/* check for loose vert firs */
	if (v->e == NULL) {
		v->e = e;
		base = tail = bmesh_disk_getpointer(e, v);
		bmesh_cycle_append(base, tail); /* circular reference is ok */
		return TRUE;
	}
	
	/* insert e at the end of disk cycle and make it the new v-> */
	base = bmesh_disk_getpointer(v->e, v);
	tail = bmesh_disk_getpointer(e, v);
	bmesh_cycle_append(base, tail);
	return TRUE;
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
	
	base = bmesh_disk_getpointer(v->e, v);
	remnode = bmesh_disk_getpointer(e, v);
	
	/* first deal with v->e pointer.. */
	len = bmesh_cycle_length(base);
	if (len == 1) newbase = NULL;
	else if (v->e == e) newbase = base->next-> data;
	else newbase = v->e;
	
	/* remove and rebas */
	bmesh_cycle_remove(base, remnode);
	v->e = newbase;
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
	
	if (eflag && tflag) {
		return NULL;
	}

	ok = bmesh_vert_in_edge(e, v);
	if (ok) {
		diskbase = bmesh_disk_getpointer(e, v);
		len = bmesh_cycle_length(diskbase);
		curedge = bmesh_disk_nextedge(e, v);
		while (curedge != e) {
			if (eflag) {
				if (curedge->head.eflag1 == eflag) {
					return curedge;
				}
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
	int i, len = 0, count = 0;
	
	if (v->e) {

		/* tflag and eflag are reserved for different functions */
		if (eflag && tflag) {
			return 0;
		}

		diskbase = bmesh_disk_getpointer(v->e, v);
		len = bmesh_cycle_length(diskbase);
		
		for (i = 0, curedge = v->e; i < len; i++) {
			if (eflag) {
				if (curedge->head.eflag1 == eflag) count++;
			}
			curedge = bmesh_disk_nextedge(curedge, v);
		}
	}
	return count;
}


int bmesh_disk_hasedge(BMVert *v, BMEdge *e)
{
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len = 0;
	
	if (v->e) {
		diskbase = bmesh_disk_getpointer(v->e, v);
		len = bmesh_cycle_length(diskbase);
		
		for (i = 0, curedge = v->e; i < len; i++) {
			if (curedge == e) {
				return TRUE;
			}
			else curedge = bmesh_disk_nextedge(curedge, v);
		}
	}
	return FALSE;
}

BMEdge *bmesh_disk_existedge(BMVert *v1, BMVert *v2)
{
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len = 0;
	
	if (v1->e) {
		diskbase = bmesh_disk_getpointer(v1->e, v1);
		len = bmesh_cycle_length(diskbase);
		
		for (i = 0, curedge = v1->e; i < len; i++, curedge = bmesh_disk_nextedge(curedge, v1)) {
			if (bmesh_verts_in_edge(v1, v2, curedge)) {
				return curedge;
			}
		}
	}
	
	return NULL;
}

/* end disk cycle routine */

void bmesh_radial_append(BMEdge *e, BMLoop *l)
{
	if (e->l == NULL) e->l = l;
	bmesh_cycle_append(&(e->l->radial), &(l->radial));
}

void bmesh_radial_remove_loop(BMLoop *l, BMEdge *e)
{
	BMLoop *newbase;
	int len;
	
	/* deal with edge->l pointe */
	len = bmesh_cycle_length(&(e->l->radial));
	if (len == 1) newbase = NULL;
	else if (e->l == l) newbase = e->l->radial_next;
	else newbase = e->l;
	
	/* remove and rebas */
	bmesh_cycle_remove(&(e->l->radial), &(l->radial));
	e->l = newbase;
}

int bmesh_radial_find_face(BMEdge *e, BMFace *f)
{

	BMLoop *curloop;
	int i, len;
	
	len = bmesh_cycle_length(&(e->l->radial));
	for (i = 0, curloop = e->l; i < len; i++, curloop = curloop->radial_next) {
		if (curloop->f == f) {
			return TRUE;
		}
	}
	return FALSE;
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
	do {
		if (curloop->v == v) count++;
		curloop = bmesh_radial_nextloop(curloop);
	} while (curloop != l);
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

	/* is there an edge on this vert at all */
	if (!v->e)
		return count;

	/* first, loop around edge */
	curedge = v->e;
	do {
		if (curedge->l) count += bmesh_radial_count_facevert(curedge->l, v);
		curedge = bmesh_disk_nextedge(curedge, v);
	} while (curedge != v->e);

	return count;
}

/*
 * BME RADIAL FIND FIRST FACE VERT
 *
 * Finds the first loop of v around radial
 * cycle
 *
 */
BMLoop *bmesh_radial_find_first_faceloop(BMLoop *l, BMVert *v)
{
	BMLoop *curloop;
	curloop = l;
	do {
		if (curloop->v == v) {
			return curloop;
		}

		curloop = bmesh_radial_nextloop(curloop);
	} while (curloop != l);
	return NULL;
}

BMLoop *bmesh_radial_find_next_faceloop(BMLoop *l, BMVert *v)
{
	BMLoop *curloop;
	curloop = bmesh_radial_nextloop(l);
	do {
		if (curloop->v == v) {
			return curloop;
		}

		curloop = bmesh_radial_nextloop(curloop);
	} while (curloop != l);
	return l;
}


/*
 * BME FIND FIRST FACE EDGE
 *
 * Finds the first edge in a vertices
 * Disk cycle that has one of this
 * vert's loops attached
 * to it.
 */

BMEdge *bmesh_disk_find_first_faceedge(BMEdge *e, BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = e;
	do {
		if (searchedge->l && bmesh_radial_count_facevert(searchedge->l, v)) {
			return searchedge;
		}
		searchedge = bmesh_disk_nextedge(searchedge, v);
	} while (searchedge != e);
	
	return NULL;
}

BMEdge *bmesh_disk_find_next_faceedge(BMEdge *e, BMVert *v)
{
	BMEdge *searchedge = NULL;
	searchedge = bmesh_disk_nextedge(e, v);
	do {
		if (searchedge->l && bmesh_radial_count_facevert(searchedge->l, v)) {
			return searchedge;
		}
		searchedge = bmesh_disk_nextedge(searchedge, v);
	} while (searchedge != e);
	return e;
}





struct BMLoop *bmesh_loop_find_loop(struct BMFace *f, struct BMVert *v)
{
	BMLoop *l;
	int i, len;
	
	len = bmesh_cycle_length(f->lbase);
	for (i = 0, l = f->loopbase; i < len; i++, l = l->next) {
		if (l->v == v) {
			return l;
		}
	}
	return NULL;
}

#endif
