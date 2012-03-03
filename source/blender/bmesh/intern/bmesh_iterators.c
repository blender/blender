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

/** \file blender/bmesh/intern/bmesh_iterators.c
 *  \ingroup bmesh
 *
 * Functions to abstract looping over bmesh data structures.
 *
 * See: bmesh_iterators_inlin.c too, some functions are here for speed reasons.
 */


#include "bmesh.h"
#include "bmesh_private.h"

const char bm_iter_itype_htype_map[BM_ITYPE_MAX] = {
	'\0',
	BM_VERT, /* BM_VERTS_OF_MESH */
	BM_EDGE, /* BM_EDGES_OF_MESH */
	BM_FACE, /* BM_FACES_OF_MESH */
	BM_EDGE, /* BM_EDGES_OF_VERT */
	BM_FACE, /* BM_FACES_OF_VERT */
	BM_LOOP, /* BM_LOOPS_OF_VERT */
	BM_VERT, /* BM_VERTS_OF_EDGE */
	BM_FACE, /* BM_FACES_OF_EDGE */
	BM_VERT, /* BM_VERTS_OF_FACE */
	BM_EDGE, /* BM_EDGES_OF_FACE */
	BM_LOOP, /* BM_LOOPS_OF_FACE */
	BM_LOOP, /* BM_ALL_LOOPS_OF_FACE */
	BM_LOOP, /* BM_LOOPS_OF_LOOP */
	BM_LOOP  /* BM_LOOPS_OF_EDGE */
};

/**
 * \note Use #BM_vert_at_index / #BM_edge_at_index / #BM_face_at_index for mesh arrays.
 */
void *BM_iter_at_index(BMesh *bm, const char itype, void *data, int index)
{
	BMIter iter;
	void *val;
	int i;

	/* sanity check */
	if (index < 0) {
		return NULL;
	}

	val = BM_iter_new(&iter, bm, itype, data);

	i = 0;
	while (i < index) {
		val = BM_iter_step(&iter);
		i++;
	}

	return val;
}


/**
 * \brief Iterator as Array
 *
 * Sometimes its convenient to get the iterator as an array
 * to avoid multiple calls to #BM_iter_at_index.
 */
int BM_iter_as_array(BMesh *bm, const char type, void *data, void **array, const int len)
{
	int i = 0;

	/* sanity check */
	if (len > 0) {

		BMIter iter;
		void *val;

		BM_ITER(val, &iter, bm, type, data) {
			array[i] = val;
			i++;
			if (i == len) {
				return len;
			}
		}
	}

	return i;
}


/**
 * \brief Init Iterator
 *
 * Clears the internal state of an iterator for begin() callbacks.
 */
static void init_iterator(BMIter *iter)
{
	iter->firstvert = iter->nextvert = NULL;
	iter->firstedge = iter->nextedge = NULL;
	iter->firstloop = iter->nextloop = NULL;
	iter->firstpoly = iter->nextpoly = NULL;
	iter->ldata = NULL;
}

/**
 * Notes on iterator implementation:
 *
 * Iterators keep track of the next element in a sequence.
 * When a step() callback is invoked the current value of 'next'
 * is stored to be returned later and the next variable is incremented.
 *
 * When the end of a sequence is reached, next should always equal NULL
 *
 * The 'bmiter__' prefix is used because these are used in
 * bmesh_iterators_inine.c but should otherwise be seen as
 * private.
 */

/*
 * VERT OF MESH CALLBACKS
 */

void bmiter__vert_of_mesh_begin(BMIter *iter)
{
	BLI_mempool_iternew(iter->bm->vpool, &iter->pooliter);
}

void  *bmiter__vert_of_mesh_step(BMIter *iter)
{
	return BLI_mempool_iterstep(&iter->pooliter);

}

void  bmiter__edge_of_mesh_begin(BMIter *iter)
{
	BLI_mempool_iternew(iter->bm->epool, &iter->pooliter);
}

void  *bmiter__edge_of_mesh_step(BMIter *iter)
{
	return BLI_mempool_iterstep(&iter->pooliter);

}

void  bmiter__face_of_mesh_begin(BMIter *iter)
{
	BLI_mempool_iternew(iter->bm->fpool, &iter->pooliter);
}

void  *bmiter__face_of_mesh_step(BMIter *iter)
{
	return BLI_mempool_iterstep(&iter->pooliter);

}

/*
 * EDGE OF VERT CALLBACKS
 */

void  bmiter__edge_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	if (iter->vdata->e) {
		iter->firstedge = iter->vdata->e;
		iter->nextedge = iter->vdata->e;
	}
}

void  *bmiter__edge_of_vert_step(BMIter *iter)
{
	BMEdge *current = iter->nextedge;

	if (iter->nextedge)
		iter->nextedge = bmesh_disk_edge_next(iter->nextedge, iter->vdata);
	
	if (iter->nextedge == iter->firstedge) iter->nextedge = NULL;

	return current;
}

/*
 * FACE OF VERT CALLBACKS
 */

void  bmiter__face_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->count = 0;
	if (iter->vdata->e)
		iter->count = bmesh_disk_facevert_count(iter->vdata);
	if (iter->count) {
		iter->firstedge = bmesh_disk_faceedge_find_first(iter->vdata->e, iter->vdata);
		iter->nextedge = iter->firstedge;
		iter->firstloop = bmesh_radial_faceloop_find_first(iter->firstedge->l, iter->vdata);
		iter->nextloop = iter->firstloop;
	}
}
void  *bmiter__face_of_vert_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->count && iter->nextloop) {
		iter->count--;
		iter->nextloop = bmesh_radial_faceloop_find_next(iter->nextloop, iter->vdata);
		if (iter->nextloop == iter->firstloop) {
			iter->nextedge = bmesh_disk_faceedge_find_next(iter->nextedge, iter->vdata);
			iter->firstloop = bmesh_radial_faceloop_find_first(iter->nextedge->l, iter->vdata);
			iter->nextloop = iter->firstloop;
		}
	}
	
	if (!iter->count) iter->nextloop = NULL;

	return current ? current->f : NULL;
}


/*
 * LOOP OF VERT CALLBACKS
 *
 */

void  bmiter__loop_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->count = 0;
	if (iter->vdata->e)
		iter->count = bmesh_disk_facevert_count(iter->vdata);
	if (iter->count) {
		iter->firstedge = bmesh_disk_faceedge_find_first(iter->vdata->e, iter->vdata);
		iter->nextedge = iter->firstedge;
		iter->firstloop = bmesh_radial_faceloop_find_first(iter->firstedge->l, iter->vdata);
		iter->nextloop = iter->firstloop;
	}
}
void  *bmiter__loop_of_vert_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->count) {
		iter->count--;
		iter->nextloop = bmesh_radial_faceloop_find_next(iter->nextloop, iter->vdata);
		if (iter->nextloop == iter->firstloop) {
			iter->nextedge = bmesh_disk_faceedge_find_next(iter->nextedge, iter->vdata);
			iter->firstloop = bmesh_radial_faceloop_find_first(iter->nextedge->l, iter->vdata);
			iter->nextloop = iter->firstloop;
		}
	}
	
	if (!iter->count) iter->nextloop = NULL;

	
	if (current) {
		return current;
	}

	return NULL;
}


void  bmiter__loops_of_edge_begin(BMIter *iter)
{
	BMLoop *l;

	l = iter->edata->l;

	/* note sure why this sets ldata ... */
	init_iterator(iter);
	
	iter->firstloop = iter->nextloop = l;
}

void  *bmiter__loops_of_edge_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->nextloop) {
		iter->nextloop = iter->nextloop->radial_next;
	}

	if (iter->nextloop == iter->firstloop) {
		iter->nextloop = NULL;
	}

	if (current) {
		return current;
	}

	return NULL;
}

void  bmiter__loops_of_loop_begin(BMIter *iter)
{
	BMLoop *l;

	l = iter->ldata;

	/* note sure why this sets ldata ... */
	init_iterator(iter);

	iter->firstloop = l;
	iter->nextloop = iter->firstloop->radial_next;
	
	if (iter->nextloop == iter->firstloop)
		iter->nextloop = NULL;
}

void  *bmiter__loops_of_loop_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;
	
	if (iter->nextloop) {
		iter->nextloop = iter->nextloop->radial_next;
	}

	if (iter->nextloop == iter->firstloop) {
		iter->nextloop = NULL;
	}

	if (current) {
		return current;
	}

	return NULL;
}

/*
 * FACE OF EDGE CALLBACKS
 */

void  bmiter__face_of_edge_begin(BMIter *iter)
{
	init_iterator(iter);
	
	if (iter->edata->l) {
		iter->firstloop = iter->edata->l;
		iter->nextloop = iter->edata->l;
	}
}

void  *bmiter__face_of_edge_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->nextloop) {
		iter->nextloop = iter->nextloop->radial_next;
	}

	if (iter->nextloop == iter->firstloop) iter->nextloop = NULL;

	return current ? current->f : NULL;
}

/*
 * VERTS OF EDGE CALLBACKS
 */

void  bmiter__vert_of_edge_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->count = 0;
}

void  *bmiter__vert_of_edge_step(BMIter *iter)
{
	iter->count++;
	switch (iter->count) {
		case 1:
			return iter->edata->v1;
		case 2:
			return iter->edata->v2;
		default:
			return NULL;
	}
}

/*
 * VERT OF FACE CALLBACKS
 */

void  bmiter__vert_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__vert_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->nextloop) iter->nextloop = iter->nextloop->next;
	if (iter->nextloop == iter->firstloop) iter->nextloop = NULL;

	return current ? current->v : NULL;
}

/*
 * EDGE OF FACE CALLBACKS
 */

void  bmiter__edge_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__edge_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->nextloop) iter->nextloop = iter->nextloop->next;
	if (iter->nextloop == iter->firstloop) iter->nextloop = NULL;
	
	return current ? current->e : NULL;
}

/*
 * LOOP OF FACE CALLBACKS
 */

void  bmiter__loop_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__loop_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if (iter->nextloop) iter->nextloop = iter->nextloop->next;
	if (iter->nextloop == iter->firstloop) iter->nextloop = NULL;

	return current;
}
