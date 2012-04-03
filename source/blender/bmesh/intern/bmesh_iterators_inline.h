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

/** \file blender/bmesh/intern/bmesh_iterators_inline.h
 *  \ingroup bmesh
 *
 * BMesh inline iterator functions.
 */

#ifndef __BMESH_ITERATORS_INLINE_H__
#define __BMESH_ITERATORS_INLINE_H__

/* inline here optimizes out the switch statement when called with
 * constant values (which is very common), nicer for loop-in-loop situations */

/**
 * \brief Iterator Step
 *
 * Calls an iterators step fucntion to return the next element.
 */
BLI_INLINE void *BM_iter_step(BMIter *iter)
{
	return iter->step(iter);
}


/**
 * \brief Iterator Init
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type.
 */
BLI_INLINE int BM_iter_init(BMIter *iter, BMesh *bm, const char itype, void *data)
{
	/* int argtype; */
	iter->itype = itype;
	iter->bm = bm;

	/* inlining optimizes out this switch when called with the defined type */
	switch ((BMIterType)itype) {
		case BM_VERTS_OF_MESH:
			iter->begin = bmiter__vert_of_mesh_begin;
			iter->step =  bmiter__vert_of_mesh_step;
			break;
		case BM_EDGES_OF_MESH:
			iter->begin = bmiter__edge_of_mesh_begin;
			iter->step =  bmiter__edge_of_mesh_step;
			break;
		case BM_FACES_OF_MESH:
			iter->begin = bmiter__face_of_mesh_begin;
			iter->step =  bmiter__face_of_mesh_step;
			break;
		case BM_EDGES_OF_VERT:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__edge_of_vert_begin;
			iter->step =  bmiter__edge_of_vert_step;
			iter->vdata = data;
			break;
		case BM_FACES_OF_VERT:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__face_of_vert_begin;
			iter->step =  bmiter__face_of_vert_step;
			iter->vdata = data;
			break;
		case BM_LOOPS_OF_VERT:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__loop_of_vert_begin;
			iter->step =  bmiter__loop_of_vert_step;
			iter->vdata = data;
			break;
		case BM_VERTS_OF_EDGE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__vert_of_edge_begin;
			iter->step =  bmiter__vert_of_edge_step;
			iter->edata = data;
			break;
		case BM_FACES_OF_EDGE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__face_of_edge_begin;
			iter->step =  bmiter__face_of_edge_step;
			iter->edata = data;
			break;
		case BM_VERTS_OF_FACE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__vert_of_face_begin;
			iter->step =  bmiter__vert_of_face_step;
			iter->pdata = data;
			break;
		case BM_EDGES_OF_FACE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__edge_of_face_begin;
			iter->step =  bmiter__edge_of_face_step;
			iter->pdata = data;
			break;
		case BM_LOOPS_OF_FACE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__loop_of_face_begin;
			iter->step =  bmiter__loop_of_face_step;
			iter->pdata = data;
			break;
		case BM_LOOPS_OF_LOOP:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__loops_of_loop_begin;
			iter->step =  bmiter__loops_of_loop_step;
			iter->ldata = data;
			break;
		case BM_LOOPS_OF_EDGE:
			if (UNLIKELY(!data)) {
				return FALSE;
			}

			iter->begin = bmiter__loops_of_edge_begin;
			iter->step =  bmiter__loops_of_edge_step;
			iter->edata = data;
			break;
		default:
			break;
	}

	iter->begin(iter);
	return TRUE;
}

/**
 * \brief Iterator New
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type and then calls BMeshIter_step()
 * to return the first element of the iterator.
 *
 */
BLI_INLINE void *BM_iter_new(BMIter *iter, BMesh *bm, const char itype, void *data)
{
	if (LIKELY(BM_iter_init(iter, bm, itype, data))) {
		return BM_iter_step(iter);
	}
	else {
		return NULL;
	}
}

#endif /* __BMESH_ITERATORS_INLINE_H__ */
