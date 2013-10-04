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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

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
 * Utility function.
 */
int BM_iter_mesh_count(BMesh *bm, const char itype)
{
	int count;

	switch (itype) {
		case BM_VERTS_OF_MESH:
			count = bm->totvert;
			break;
		case BM_EDGES_OF_MESH:
			count = bm->totedge;
			break;
		case BM_FACES_OF_MESH:
			count = bm->totface;
			break;
		default:
			count = 0;
			BLI_assert(0);
			break;
	}

	return count;
}

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
int BM_iter_as_array(BMesh *bm, const char itype, void *data, void **array, const int len)
{
	int i = 0;

	/* sanity check */
	if (len > 0) {
		BMIter iter;
		void *ele;

		for (ele = BM_iter_new(&iter, bm, itype, data); ele; ele = BM_iter_step(&iter)) {
			array[i] = ele;
			i++;
			if (i == len) {
				return len;
			}
		}
	}

	return i;
}
/**
 * \brief Operator Iterator as Array
 *
 * Sometimes its convenient to get the iterator as an array.
 */
int BMO_iter_as_array(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const char restrictmask,
                      void **array, const int len)
{
	int i = 0;

	/* sanity check */
	if (len > 0) {
		BMOIter oiter;
		void *ele;

		for (ele = BMO_iter_new(&oiter, slot_args, slot_name, restrictmask); ele; ele = BMO_iter_step(&oiter)) {
			array[i] = ele;
			i++;
			if (i == len) {
				return len;
			}
		}
	}

	return i;
}


/**
 * \brief Iterator as Array
 *
 * Allocates a new array, has the advantage that you dont need to know the size ahead of time.
 *
 * Takes advantage of less common iterator usage to avoid counting twice,
 * which you might end up doing when #BM_iter_as_array is used.
 *
 * Caller needs to free the array.
 */
void *BM_iter_as_arrayN(BMesh *bm, const char itype, void *data, int *r_len,
                        /* optional args to avoid an alloc (normally stack array) */
                        void **stack_array, int stack_array_size)
{
	BMIter iter;

	BLI_assert(stack_array_size == 0 || (stack_array_size && stack_array));

	/* we can't rely on coun't being set */
	switch (itype) {
		case BM_VERTS_OF_MESH:
			iter.count = bm->totvert;
			break;
		case BM_EDGES_OF_MESH:
			iter.count = bm->totedge;
			break;
		case BM_FACES_OF_MESH:
			iter.count = bm->totface;
			break;
		default:
			break;
	}

	if (BM_iter_init(&iter, bm, itype, data) && iter.count > 0) {
		BMElem *ele;
		BMElem **array = iter.count > stack_array_size ?
		                 MEM_mallocN(sizeof(ele) * iter.count, __func__) :
		                 stack_array;
		int i = 0;

		*r_len = iter.count;  /* set before iterating */

		while ((ele = BM_iter_step(&iter))) {
			array[i++] = ele;
		}
		return array;
	}
	else {
		*r_len = 0;
		return NULL;
	}
}

void *BMO_iter_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const char restrictmask,
                         int *r_len,
                         /* optional args to avoid an alloc (normally stack array) */
                         void **stack_array, int stack_array_size)
{
	BMOIter iter;
	BMElem *ele;
	int count = BMO_slot_buffer_count(slot_args, slot_name);

	BLI_assert(stack_array_size == 0 || (stack_array_size && stack_array));

	if ((ele = BMO_iter_new(&iter, slot_args, slot_name, restrictmask)) && count > 0) {
		BMElem **array = count > stack_array_size ?
		                 MEM_mallocN(sizeof(ele) * count, __func__) :
		                 stack_array;
		int i = 0;

		do {
			array[i++] = ele;
		} while ((ele = BMO_iter_step(&iter)));
		BLI_assert(i <= count);

		if (i != count) {
			if ((void **)array != stack_array) {
				array = MEM_reallocN(array, sizeof(ele) * i);
			}
		}
		*r_len = i;
		return array;
	}
	else {
		*r_len = 0;
		return NULL;
	}
}

/**
 * \brief Elem Iter Flag Count
 *
 * Counts how many flagged / unflagged items are found in this element.
 */
int BM_iter_elem_count_flag(const char itype, void *data, const char hflag, const bool value)
{
	BMIter iter;
	BMElem *ele;
	int count = 0;

	BM_ITER_ELEM (ele, &iter, data, itype) {
		if (BM_elem_flag_test_bool(ele, hflag) == value) {
			count++;
		}
	}

	return count;
}

/**
 * \brief Elem Iter Tool Flag Count
 *
 * Counts how many flagged / unflagged items are found in this element.
 */
int BMO_iter_elem_count_flag(BMesh *bm, const char itype, void *data,
                             const short oflag, const bool value)
{
	BMIter iter;
	BMElemF *ele;
	int count = 0;

	/* loops have no header flags */
	BLI_assert(bm_iter_itype_htype_map[itype] != BM_LOOP);

	BM_ITER_ELEM (ele, &iter, data, itype) {
		if (BMO_elem_flag_test_bool(bm, ele, oflag) == value) {
			count++;
		}
	}
	return count;
}


/**
 * \brief Mesh Iter Flag Count
 *
 * Counts how many flagged / unflagged items are found in this mesh.
 */
int BM_iter_mesh_count_flag(const char itype, BMesh *bm, const char hflag, const bool value)
{
	BMIter iter;
	BMElem *ele;
	int count = 0;

	BM_ITER_MESH (ele, &iter, bm, itype) {
		if (BM_elem_flag_test_bool(ele, hflag) == value) {
			count++;
		}
	}

	return count;
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

/* see bug [#36923] for why we need this,
 * allow adding but not removing, this isnt _totally_ safe since
 * you could add/remove within the same loop, but catches common cases
 */
#ifdef DEBUG
#  define USE_IMMUTABLE_ASSERT
#endif

void bmiter__vert_of_mesh_begin(struct BMIter__vert_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	((BMIter *)iter)->count = iter->bm->totvert;
#endif
	BLI_mempool_iternew(iter->bm->vpool, &iter->pooliter);
}

void *bmiter__vert_of_mesh_step(struct BMIter__vert_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	BLI_assert(((BMIter *)iter)->count <= iter->bm->totvert);
#endif
	return BLI_mempool_iterstep(&iter->pooliter);
}

void bmiter__edge_of_mesh_begin(struct BMIter__edge_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	((BMIter *)iter)->count = iter->bm->totedge;
#endif
	BLI_mempool_iternew(iter->bm->epool, &iter->pooliter);
}

void  *bmiter__edge_of_mesh_step(struct BMIter__edge_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	BLI_assert(((BMIter *)iter)->count <= iter->bm->totedge);
#endif
	return BLI_mempool_iterstep(&iter->pooliter);
}

void  bmiter__face_of_mesh_begin(struct BMIter__face_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	((BMIter *)iter)->count = iter->bm->totface;
#endif
	BLI_mempool_iternew(iter->bm->fpool, &iter->pooliter);
}

void  *bmiter__face_of_mesh_step(struct BMIter__face_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
	BLI_assert(((BMIter *)iter)->count <= iter->bm->totface);
#endif
	return BLI_mempool_iterstep(&iter->pooliter);
}

#ifdef USE_IMMUTABLE_ASSERT
#  undef USE_IMMUTABLE_ASSERT
#endif

/*
 * EDGE OF VERT CALLBACKS
 */

void  bmiter__edge_of_vert_begin(struct BMIter__edge_of_vert *iter)
{
	if (iter->vdata->e) {
		iter->e_first = iter->vdata->e;
		iter->e_next = iter->vdata->e;
	}
	else {
		iter->e_first = NULL;
		iter->e_next = NULL;
	}
}

void  *bmiter__edge_of_vert_step(struct BMIter__edge_of_vert *iter)
{
	BMEdge *e_curr = iter->e_next;

	if (iter->e_next) {
		iter->e_next = bmesh_disk_edge_next(iter->e_next, iter->vdata);
		if (iter->e_next == iter->e_first) {
			iter->e_next = NULL;
		}
	}

	return e_curr;
}

/*
 * FACE OF VERT CALLBACKS
 */

void  bmiter__face_of_vert_begin(struct BMIter__face_of_vert *iter)
{
	((BMIter *)iter)->count = bmesh_disk_facevert_count(iter->vdata);
	if (((BMIter *)iter)->count) {
		iter->e_first = bmesh_disk_faceedge_find_first(iter->vdata->e, iter->vdata);
		iter->e_next = iter->e_first;
		iter->l_first = bmesh_radial_faceloop_find_first(iter->e_first->l, iter->vdata);
		iter->l_next = iter->l_first;
	}
	else {
		iter->l_first = iter->l_next = NULL;
		iter->e_first = iter->e_next = NULL;
	}
}
void  *bmiter__face_of_vert_step(struct BMIter__face_of_vert *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (((BMIter *)iter)->count && iter->l_next) {
		((BMIter *)iter)->count--;
		iter->l_next = bmesh_radial_faceloop_find_next(iter->l_next, iter->vdata);
		if (iter->l_next == iter->l_first) {
			iter->e_next = bmesh_disk_faceedge_find_next(iter->e_next, iter->vdata);
			iter->l_first = bmesh_radial_faceloop_find_first(iter->e_next->l, iter->vdata);
			iter->l_next = iter->l_first;
		}
	}
	
	if (!((BMIter *)iter)->count) {
		iter->l_next = NULL;
	}

	return l_curr ? l_curr->f : NULL;
}


/*
 * LOOP OF VERT CALLBACKS
 *
 */

void  bmiter__loop_of_vert_begin(struct BMIter__loop_of_vert *iter)
{
	((BMIter *)iter)->count = bmesh_disk_facevert_count(iter->vdata);
	if (((BMIter *)iter)->count) {
		iter->e_first = bmesh_disk_faceedge_find_first(iter->vdata->e, iter->vdata);
		iter->e_next = iter->e_first;
		iter->l_first = bmesh_radial_faceloop_find_first(iter->e_first->l, iter->vdata);
		iter->l_next = iter->l_first;
	}
	else {
		iter->l_first = iter->l_next = NULL;
		iter->e_first = iter->e_next = NULL;
	}
}
void  *bmiter__loop_of_vert_step(struct BMIter__loop_of_vert *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (((BMIter *)iter)->count) {
		((BMIter *)iter)->count--;
		iter->l_next = bmesh_radial_faceloop_find_next(iter->l_next, iter->vdata);
		if (iter->l_next == iter->l_first) {
			iter->e_next = bmesh_disk_faceedge_find_next(iter->e_next, iter->vdata);
			iter->l_first = bmesh_radial_faceloop_find_first(iter->e_next->l, iter->vdata);
			iter->l_next = iter->l_first;
		}
	}
	
	if (!((BMIter *)iter)->count) {
		iter->l_next = NULL;
	}

	/* NULL on finish */
	return l_curr;
}

/*
 * LOOP OF EDGE CALLBACKS
 */

void  bmiter__loop_of_edge_begin(struct BMIter__loop_of_edge *iter)
{
	iter->l_first = iter->l_next = iter->edata->l;
}

void  *bmiter__loop_of_edge_step(struct BMIter__loop_of_edge *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (iter->l_next) {
		iter->l_next = iter->l_next->radial_next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}

	/* NULL on finish */
	return l_curr;
}

/*
 * LOOP OF LOOP CALLBACKS
 */

void  bmiter__loop_of_loop_begin(struct BMIter__loop_of_loop *iter)
{
	iter->l_first = iter->ldata;
	iter->l_next = iter->l_first->radial_next;
	
	if (iter->l_next == iter->l_first)
		iter->l_next = NULL;
}

void  *bmiter__loop_of_loop_step(struct BMIter__loop_of_loop *iter)
{
	BMLoop *l_curr = iter->l_next;
	
	if (iter->l_next) {
		iter->l_next = iter->l_next->radial_next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}

	/* NULL on finish */
	return l_curr;
}

/*
 * FACE OF EDGE CALLBACKS
 */

void  bmiter__face_of_edge_begin(struct BMIter__face_of_edge *iter)
{
	iter->l_first = iter->l_next = iter->edata->l;
}

void  *bmiter__face_of_edge_step(struct BMIter__face_of_edge *iter)
{
	BMLoop *current = iter->l_next;

	if (iter->l_next) {
		iter->l_next = iter->l_next->radial_next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}

	return current ? current->f : NULL;
}

/*
 * VERTS OF EDGE CALLBACKS
 */

void  bmiter__vert_of_edge_begin(struct BMIter__vert_of_edge *iter)
{
	((BMIter *)iter)->count = 0;
}

void  *bmiter__vert_of_edge_step(struct BMIter__vert_of_edge *iter)
{
	switch (((BMIter *)iter)->count++) {
		case 0:
			return iter->edata->v1;
		case 1:
			return iter->edata->v2;
		default:
			return NULL;
	}
}

/*
 * VERT OF FACE CALLBACKS
 */

void  bmiter__vert_of_face_begin(struct BMIter__vert_of_face *iter)
{
	iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__vert_of_face_step(struct BMIter__vert_of_face *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (iter->l_next) {
		iter->l_next = iter->l_next->next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}

	return l_curr ? l_curr->v : NULL;
}

/*
 * EDGE OF FACE CALLBACKS
 */

void  bmiter__edge_of_face_begin(struct BMIter__edge_of_face *iter)
{
	iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__edge_of_face_step(struct BMIter__edge_of_face *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (iter->l_next) {
		iter->l_next = iter->l_next->next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}
	
	return l_curr ? l_curr->e : NULL;
}

/*
 * LOOP OF FACE CALLBACKS
 */

void  bmiter__loop_of_face_begin(struct BMIter__loop_of_face *iter)
{
	iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void  *bmiter__loop_of_face_step(struct BMIter__loop_of_face *iter)
{
	BMLoop *l_curr = iter->l_next;

	if (iter->l_next) {
		iter->l_next = iter->l_next->next;
		if (iter->l_next == iter->l_first) {
			iter->l_next = NULL;
		}
	}

	return l_curr;
}
