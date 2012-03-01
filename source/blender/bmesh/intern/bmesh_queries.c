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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_queries.c
 *  \ingroup bmesh
 *
 * This file contains functions for answering common
 * Topological and geometric queries about a mesh, such
 * as, "What is the angle between these two faces?" or,
 * "How many faces are incident upon this vertex?" Tool
 * authors should use the functions in this file instead
 * of inspecting the mesh structure directly.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_private.h"

#define BM_OVERLAP (1 << 13)

/**
 * Return the amount of element of type 'type' in a given bmesh.
 */
int BM_mesh_elem_count(BMesh *bm, const char htype)
{
	if (htype == BM_VERT) return bm->totvert;
	else if (htype == BM_EDGE) return bm->totedge;
	else if (htype == BM_FACE) return bm->totface;

	return 0;
}

/**
 * Returns whether or not a given vertex is
 * is part of a given edge.
 */
int BM_vert_in_edge(BMEdge *e, BMVert *v)
{
	return bmesh_vert_in_edge(e, v);
}

/**
 * \brief BMESH OTHER EDGE IN FACE SHARING A VERTEX
 *
 * Finds the other loop that shares 'v' with 'e's loop in 'f'.
 */
BMLoop *BM_face_other_loop(BMEdge *e, BMFace *f, BMVert *v)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	
	do {
		if (l_iter->e == e) {
			break;
		}
	} while ((l_iter = l_iter->next) != l_first);
	
	return l_iter->v == v ? l_iter->prev : l_iter->next;
}

/**
 * Returns TRUE if the vertex is used in a given face.
 */

int BM_vert_in_face(BMFace *f, BMVert *v)
{
	BMLoop *l_iter, *l_first;

#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
	for (lst = f->loops.first; lst; lst = lst->next)
#endif
	{
#ifdef USE_BMESH_HOLES
		l_iter = l_first = lst->first;
#else
		l_iter = l_first = f->l_first;
#endif
		do {
			if (l_iter->v == v) {
				return TRUE;
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	return FALSE;
}

/**
 * Compares the number of vertices in an array
 * that appear in a given face
 */
int BM_verts_in_face(BMesh *bm, BMFace *f, BMVert **varr, int len)
{
	BMLoop *l_iter, *l_first;

#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
#endif

	int i, count = 0;
	
	for (i = 0; i < len; i++) {
		BMO_elem_flag_enable(bm, varr[i], BM_OVERLAP);
	}

#ifdef USE_BMESH_HOLES
	for (lst = f->loops.first; lst; lst = lst->next)
#endif
	{

#ifdef USE_BMESH_HOLES
		l_iter = l_first = lst->first;
#else
		l_iter = l_first = f->l_first;
#endif

		do {
			if (BMO_elem_flag_test(bm, l_iter->v, BM_OVERLAP)) {
				count++;
			}

		} while ((l_iter = l_iter->next) != l_first);
	}

	for (i = 0; i < len; i++) BMO_elem_flag_disable(bm, varr[i], BM_OVERLAP);

	return count;
}

/**
 * Returns whether or not a given edge is is part of a given face.
 */
int BM_edge_in_face(BMFace *f, BMEdge *e)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);

	do {
		if (l_iter->e == e) {
			return TRUE;
		}
	} while ((l_iter = l_iter->next) != l_first);

	return FALSE;
}

/**
 * Returns whether or not two vertices are in
 * a given edge
 */
int BM_verts_in_edge(BMVert *v1, BMVert *v2, BMEdge *e)
{
	return bmesh_verts_in_edge(v1, v2, e);
}

/**
 * Given a edge and one of its vertices, returns
 * the other vertex.
 */
BMVert *BM_edge_other_vert(BMEdge *e, BMVert *v)
{
	return bmesh_edge_other_vert_get(e, v);
}

/**
 *	Returns the number of edges around this vertex.
 */
int BM_vert_edge_count(BMVert *v)
{
	return bmesh_disk_count(v);
}

/**
 *	Returns the number of faces around this edge
 */
int BM_edge_face_count(BMEdge *e)
{
	int count = 0;

	if (e->l) {
		BMLoop *l_iter;
		BMLoop *l_first;

		l_iter = l_first = e->l;

		do {
			count++;
		} while ((l_iter = l_iter->radial_next) != l_first);
	}

	return count;
}

/**
 *	Returns the number of faces around this vert
 */
int BM_vert_face_count(BMVert *v)
{
	int count = 0;
	BMLoop *l;
	BMIter iter;

	BM_ITER(l, &iter, NULL, BM_LOOPS_OF_VERT, v) {
		count++;
	}

	return count;
#if 0 //this code isn't working
	BMEdge *curedge = NULL;

	if (v->e) {
		curedge = v->e;
		do {
			if (curedge->l) count += BM_edge_face_count(curedge);
			curedge = bmesh_disk_edge_next(curedge, v);
		} while (curedge != v->e);
	}
	return count;
#endif
}

/**
 * Tests whether or not the vertex is part of a wire edge.
 * (ie: has no faces attached to it)
 */
int BM_vert_is_wire(BMesh *UNUSED(bm), BMVert *v)
{
	BMEdge *curedge;

	if (v->e == NULL) {
		return FALSE;
	}
	
	curedge = v->e;
	do {
		if (curedge->l) {
			return FALSE;
		}

		curedge = bmesh_disk_edge_next(curedge, v);
	} while (curedge != v->e);

	return TRUE;
}

/**
 * Tests whether or not the edge is part of a wire.
 * (ie: has no faces attached to it)
 */
int BM_edge_is_wire(BMesh *UNUSED(bm), BMEdge *e)
{
	return (e->l) ? FALSE : TRUE;
}

/**
 * A vertex is non-manifold if it meets the following conditions:
 * 1: Loose - (has no edges/faces incident upon it)
 * 2: Joins two distinct regions - (two pyramids joined at the tip)
 * 3: Is part of a non-manifold edge (edge with more than 2 faces)
 * 4: Is part of a wire edge
 */
int BM_vert_is_manifold(BMesh *UNUSED(bm), BMVert *v)
{
	BMEdge *e, *oe;
	BMLoop *l;
	int len, count, flag;

	if (v->e == NULL) {
		/* loose vert */
		return FALSE;
	}

	/* count edges while looking for non-manifold edges */
	oe = v->e;
	for (len = 0, e = v->e; e != oe || (e == oe && len == 0); len++, e = bmesh_disk_edge_next(e, v)) {
		if (e->l == NULL) {
			/* loose edge */
			return FALSE;
		}

		if (bmesh_radial_length(e->l) > 2) {
			/* edge shared by more than two faces */
			return FALSE;
		}
	}

	count = 1;
	flag = 1;
	e = NULL;
	oe = v->e;
	l = oe->l;
	while (e != oe) {
		l = (l->v == v) ? l->prev : l->next;
		e = l->e;
		count++; /* count the edges */

		if (flag && l->radial_next == l) {
			/* we've hit the edge of an open mesh, reset once */
			flag = 0;
			count = 1;
			oe = e;
			e = NULL;
			l = oe->l;
		}
		else if (l->radial_next == l) {
			/* break the loop */
			e = oe;
		}
		else {
			l = l->radial_next;
		}
	}

	if (count < len) {
		/* vert shared by multiple regions */
		return FALSE;
	}

	return TRUE;
}

/**
 * Tests whether or not this edge is manifold.
 * A manifold edge either has 1 or 2 faces attached to it.
 */

#if 1 /* fast path for checking manifold */
int BM_edge_is_manifold(BMesh *UNUSED(bm), BMEdge *e)
{
	const BMLoop *l = e->l;
	return (l && ((l->radial_next == l) ||              /* 1 face user  */
	              (l->radial_next->radial_next == l))); /* 2 face users */
}
#else
int BM_edge_is_manifold(BMesh *UNUSED(bm), BMEdge *e)
{
	int count = BM_edge_face_count(e);
	if (count == 2 || count == 1) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}
#endif

/**
 * Tests whether or not an edge is on the boundary
 * of a shell (has one face associated with it)
 */

#if 1 /* fast path for checking boundry */
int BM_edge_is_boundary(BMEdge *e)
{
	const BMLoop *l = e->l;
	return (l && (l->radial_next == l));
}
#else
int BM_edge_is_boundary(BMEdge *e)
{
	int count = BM_edge_face_count(e);
	if (count == 1) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}
#endif

/**
 *  Counts the number of edges two faces share (if any)
 */
int BM_face_share_edge_count(BMFace *f1, BMFace *f2)
{
	BMLoop *l_iter;
	BMLoop *l_first;
	int count = 0;
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f1);
	do {
		if (bmesh_radial_face_find(l_iter->e, f2)) {
			count++;
		}
	} while ((l_iter = l_iter->next) != l_first);

	return count;
}

/**
 *	Test if e1 shares any faces with e2
 */
int BM_edge_share_face_count(BMEdge *e1, BMEdge *e2)
{
	BMLoop *l;
	BMFace *f;

	if (e1->l && e2->l) {
		l = e1->l;
		do {
			f = l->f;
			if (bmesh_radial_face_find(e2, f)) {
				return TRUE;
			}
			l = l->radial_next;
		} while (l != e1->l);
	}
	return FALSE;
}

/**
 *	Tests to see if e1 shares a vertex with e2
 */
int BM_edge_share_vert_count(BMEdge *e1, BMEdge *e2)
{
	return (e1->v1 == e2->v1 ||
	        e1->v1 == e2->v2 ||
	        e1->v2 == e2->v1 ||
	        e1->v2 == e2->v2);
}

/**
 *	Return the shared vertex between the two edges or NULL
 */
BMVert *BM_edge_share_vert(BMEdge *e1, BMEdge *e2)
{
	if (BM_vert_in_edge(e2, e1->v1)) {
		return e1->v1;
	}
	else if (BM_vert_in_edge(e2, e1->v2)) {
		return e1->v2;
	}
	else {
		return NULL;
	}
}

/**
 * Returns the verts of an edge as used in a face
 * if used in a face at all, otherwise just assign as used in the edge.
 *
 * Useful to get a determanistic winding order when calling
 * BM_face_create_ngon() on an arbitrary array of verts,
 * though be sure to pick an edge which has a face.
 */
void BM_edge_ordered_verts(BMEdge *edge, BMVert **r_v1, BMVert **r_v2)
{
	if ((edge->l == NULL) ||
	    (((edge->l->prev->v == edge->v1) && (edge->l->v == edge->v2)) ||
	     ((edge->l->v == edge->v1) && (edge->l->next->v == edge->v2)))
	    )
	{
		*r_v1 = edge->v1;
		*r_v2 = edge->v2;
	}
	else {
		*r_v1 = edge->v2;
		*r_v2 = edge->v1;
	}
}

/**
 * Calculates the angle between the previous and next loops
 * (angle at this loops face corner).
 *
 * \return angle in radians
 */
float BM_loop_face_angle(BMesh *UNUSED(bm), BMLoop *l)
{
	return angle_v3v3v3(l->prev->v->co,
	                    l->v->co,
	                    l->next->v->co);
}

/**
 * \brief BMESH EDGE/FACE ANGLE
 *
 *  Calculates the angle between two faces.
 *  Assumes the face normals are correct.
 *
 * \return angle in radians
 */
float BM_edge_face_angle(BMesh *UNUSED(bm), BMEdge *e)
{
	if (BM_edge_face_count(e) == 2) {
		BMLoop *l1 = e->l;
		BMLoop *l2 = e->l->radial_next;
		return angle_normalized_v3v3(l1->f->no, l2->f->no);
	}
	else {
		return DEG2RADF(90.0f);
	}
}

/**
 * \brief BMESH VERT/EDGE ANGLE
 *
 * Calculates the angle a verts 2 edges.
 *
 * \returns the angle in radians
 */
float BM_vert_edge_angle(BMesh *UNUSED(bm), BMVert *v)
{
	BMEdge *e1, *e2;

	/* saves BM_vert_edge_count(v) and and edge iterator,
	 * get the edges and count them both at once */

	if ((e1 = v->e) &&
		(e2 =  bmesh_disk_edge_next(e1, v)) &&
	    /* make sure we come full circle and only have 2 connected edges */
		(e1 == bmesh_disk_edge_next(e2, v)))
	{
		BMVert *v1 = BM_edge_other_vert(e1, v);
		BMVert *v2 = BM_edge_other_vert(e2, v);

		return M_PI - angle_v3v3v3(v1->co, v->co, v2->co);
	}
	else {
		return DEG2RADF(90.0f);
	}
}

/**
 * Returns the edge existing between v1 and v2, or NULL if there isn't one.
 *
 * \note multiple edges may exist between any two vertices, and therefore
 * this function only returns the first one found.
 */
BMEdge *BM_edge_exists(BMVert *v1, BMVert *v2)
{
	BMIter iter;
	BMEdge *e;

	BM_ITER(e, &iter, NULL, BM_EDGES_OF_VERT, v1) {
		if (e->v1 == v2 || e->v2 == v2)
			return e;
	}

	return NULL;
}

/**
 * Given a set of vertices \a varr, find out if
 * all those vertices overlap an existing face.
 *
 * \note Making a face here is valid but in some cases you wont want to
 * make a face thats part of another.
 *
 * \returns TRUE for overlap
 *
 */
int BM_face_exists_overlap(BMesh *bm, BMVert **varr, int len, BMFace **r_overlapface)
{
	BMIter viter;
	BMFace *f;
	int i, amount;

	for (i = 0; i < len; i++) {
		BM_ITER(f, &viter, bm, BM_FACES_OF_VERT, varr[i]) {
			amount = BM_verts_in_face(bm, f, varr, len);
			if (amount >= len) {
				if (r_overlapface) {
					*r_overlapface = f;
				}
				return TRUE;
			}
		}
	}

	if (r_overlapface) {
		*r_overlapface = NULL;
	}

	return FALSE;
}

/**
 * Given a set of vertices (varr), find out if
 * there is a face with exactly those vertices
 * (and only those vertices).
 */
int BM_face_exists(BMesh *bm, BMVert **varr, int len, BMFace **r_existface)
{
	BMIter viter;
	BMFace *f;
	int i, amount;

	for (i = 0; i < len; i++) {
		BM_ITER(f, &viter, bm, BM_FACES_OF_VERT, varr[i]) {
			amount = BM_verts_in_face(bm, f, varr, len);
			if (amount == len && amount == f->len) {
				if (r_existface) {
					*r_existface = f;
				}
				return TRUE;
			}
		}
	}

	if (r_existface) {
		*r_existface = NULL;
	}
	return FALSE;
}


/**
 * Given a set of vertices and edges (\a varr, \a earr), find out if
 * all those vertices are filled in by existing faces that _only_ use those vertices.
 *
 * This is for use in cases where creating a face is possible but would result in
 * many overlapping faces.
 *
 * An example of how this is used: when 2 tri's are selected that share an edge,
 * pressing Fkey would make a new overlapping quad (without a check like this)
 *
 * \a earr and \a varr can be in any order, however they _must_ form a closed loop.
 */
int BM_face_exists_multi(BMesh *bm, BMVert **varr, BMEdge **earr, int len)
{
	BMFace *f;
	BMEdge *e;
	BMVert *v;
	int ok;
	int tot_tag;

	BMIter fiter;
	BMIter viter;

	int i;

	for (i = 0; i < len; i++) {
		/* save some time by looping over edge faces rather then vert faces
		 * will still loop over some faces twice but not as many */
		BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, earr[i]) {
			BM_elem_flag_disable(f, BM_ELEM_INTERNAL_TAG);
			BM_ITER(v, &viter, bm, BM_VERTS_OF_FACE, f) {
				BM_elem_flag_disable(v, BM_ELEM_INTERNAL_TAG);
			}
		}

		/* clear all edge tags */
		BM_ITER(e, &fiter, bm, BM_EDGES_OF_VERT, varr[i]) {
			BM_elem_flag_disable(e, BM_ELEM_INTERNAL_TAG);
		}
	}

	/* now tag all verts and edges in the boundry array as true so
	 * we can know if a face-vert is from our array */
	for (i = 0; i < len; i++) {
		BM_elem_flag_enable(varr[i], BM_ELEM_INTERNAL_TAG);
		BM_elem_flag_enable(earr[i], BM_ELEM_INTERNAL_TAG);
	}


	/* so! boundry is tagged, everything else cleared */


	/* 1) tag all faces connected to edges - if all their verts are boundry */
	tot_tag = 0;
	for (i = 0; i < len; i++) {
		BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, earr[i]) {
			if (!BM_elem_flag_test(f, BM_ELEM_INTERNAL_TAG)) {
				ok = TRUE;
				BM_ITER(v, &viter, bm, BM_VERTS_OF_FACE, f) {
					if (!BM_elem_flag_test(v, BM_ELEM_INTERNAL_TAG)) {
						ok = FALSE;
						break;
					}
				}

				if (ok) {
					/* we only use boundry verts */
					BM_elem_flag_enable(f, BM_ELEM_INTERNAL_TAG);
					tot_tag++;
				}
			}
			else {
				/* we already found! */
			}
		}
	}

	if (tot_tag == 0) {
		/* no faces use only boundry verts, quit early */
		return FALSE;
	}

	/* 2) loop over non-boundry edges that use boundry verts,
	 *    check each have 2 tagges faces connected (faces that only use 'varr' verts) */
	ok = TRUE;
	for (i = 0; i < len; i++) {
		BM_ITER(e, &fiter, bm, BM_EDGES_OF_VERT, varr[i]) {

			if (/* non-boundry edge */
			    BM_elem_flag_test(e, BM_ELEM_INTERNAL_TAG) == FALSE &&
			    /* ...using boundry verts */
			    BM_elem_flag_test(e->v1, BM_ELEM_INTERNAL_TAG) == TRUE &&
			    BM_elem_flag_test(e->v2, BM_ELEM_INTERNAL_TAG) == TRUE)
			{
				int tot_face_tag = 0;
				BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
					if (BM_elem_flag_test(f, BM_ELEM_INTERNAL_TAG)) {
						tot_face_tag++;
					}
				}

				if (tot_face_tag != 2) {
					ok = FALSE;
					break;
				}

			}
		}

		if (ok == FALSE) {
			break;
		}
	}

	return ok;
}

/* same as 'BM_face_exists_multi' but built vert array from edges */
int BM_face_exists_multi_edge(BMesh *bm, BMEdge **earr, int len)
{
	BMVert **varr;
	BLI_array_fixedstack_declare(varr, BM_NGON_STACK_SIZE, len, __func__);

	int ok;
	int i, i_next;

	/* first check if verts have edges, if not we can bail out early */
	ok = TRUE;
	for (i = len - 1, i_next = 0; i_next < len; (i = i_next++)) {
		if (!(varr[i] = BM_edge_share_vert(earr[i], earr[i_next]))) {
			ok = FALSE;
			break;
		}
	}

	if (ok == FALSE) {
		BMESH_ASSERT(0);
		BLI_array_fixedstack_free(varr);
		return FALSE;
	}

	ok = BM_face_exists_multi(bm, varr, earr, len);

	BLI_array_fixedstack_free(varr);

	return ok;
}
