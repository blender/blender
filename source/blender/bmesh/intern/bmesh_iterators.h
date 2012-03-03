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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_ITERATORS_H__
#define __BMESH_ITERATORS_H__

/** \file blender/bmesh/intern/bmesh_iterators.h
 *  \ingroup bmesh
 */

/**
 * \brief BMesh Iterators
 *
 * The functions and structures in this file
 * provide a unified method for iterating over
 * the elements of a mesh and answering simple
 * adjacency queries. Tool authors should use
 * the iterators provided in this file instead
 * of inspecting the structure directly.
 *
 */

#include "BLI_mempool.h"

/* Defines for passing to BM_iter_new.
 *
 * "OF" can be substituted for "around"
 * so BM_VERTS_OF_FACE means "vertices
 * around a face."
 */

/* these iterator over all elements of a specific
 * type in the mesh.
 *
 * be sure to keep 'bm_iter_itype_htype_map' in sync with any changes
 */
typedef enum BMIterType {
	BM_VERTS_OF_MESH = 1,
	BM_EDGES_OF_MESH = 2,
	BM_FACES_OF_MESH = 3,
	/* these are topological iterators. */
	BM_EDGES_OF_VERT = 4,
	BM_FACES_OF_VERT = 5,
	BM_LOOPS_OF_VERT = 6,
	BM_VERTS_OF_EDGE = 7, /* just v1, v2: added so py can use generalized sequencer wrapper */
	BM_FACES_OF_EDGE = 8,
	BM_VERTS_OF_FACE = 9,
	BM_EDGES_OF_FACE = 10,
	BM_LOOPS_OF_FACE = 11,
	/* returns elements from all boundaries, and returns
	 * the first element at the end to flag that we're entering
	 * a different face hole boundary*/
	BM_ALL_LOOPS_OF_FACE = 12,
	/* iterate through loops around this loop, which are fetched
	 * from the other faces in the radial cycle surrounding the
	 * input loop's edge.*/
	BM_LOOPS_OF_LOOP = 13,
	BM_LOOPS_OF_EDGE = 14
} BMIterType;

#define BM_ITYPE_MAX 15

/* the iterator htype for each iterator */
extern const char bm_iter_itype_htype_map[BM_ITYPE_MAX];


#define BM_ITER(ele, iter, bm, itype, data)                                   \
	ele = BM_iter_new(iter, bm, itype, data);                                 \
	for ( ; ele; ele = BM_iter_step(iter))

#define BM_ITER_INDEX(ele, iter, bm, itype, data, indexvar)                   \
	ele = BM_iter_new(iter, bm, itype, data);                                 \
	for (indexvar = 0; ele; indexvar++, ele = BM_iter_step(iter))

/* Iterator Structure */
typedef struct BMIter {
	BLI_mempool_iter pooliter;

	BMVert *firstvert, *nextvert, *vdata;
	BMEdge *firstedge, *nextedge, *edata;
	BMLoop *firstloop, *nextloop, *ldata, *l;
	BMFace *firstpoly, *nextpoly, *pdata;
	BMesh *bm;
	void (*begin)(struct BMIter *iter);
	void *(*step)(struct BMIter *iter);
	union {
		void       *p;
		int         i;
		long        l;
		float       f;
	} filter;
	int count;
	char itype;
} BMIter;

void *BM_iter_at_index(BMesh *bm, const char itype, void *data, int index);
int   BM_iter_as_array(BMesh *bm, const char itype, void *data, void **array, const int len);

/* private for bmesh_iterators_inline.c */
void  bmiter__vert_of_mesh_begin(struct BMIter *iter);
void *bmiter__vert_of_mesh_step(struct BMIter *iter);
void  bmiter__edge_of_mesh_begin(struct BMIter *iter);
void *bmiter__edge_of_mesh_step(struct BMIter *iter);
void  bmiter__face_of_mesh_begin(struct BMIter *iter);
void *bmiter__face_of_mesh_step(struct BMIter *iter);
void  bmiter__edge_of_vert_begin(struct BMIter *iter);
void *bmiter__edge_of_vert_step(struct BMIter *iter);
void  bmiter__face_of_vert_begin(struct BMIter *iter);
void *bmiter__face_of_vert_step(struct BMIter *iter);
void  bmiter__loop_of_vert_begin(struct BMIter *iter);
void *bmiter__loop_of_vert_step(struct BMIter *iter);
void  bmiter__loops_of_edge_begin(struct BMIter *iter);
void *bmiter__loops_of_edge_step(struct BMIter *iter);
void  bmiter__loops_of_loop_begin(struct BMIter *iter);
void *bmiter__loops_of_loop_step(struct BMIter *iter);
void  bmiter__face_of_edge_begin(struct BMIter *iter);
void *bmiter__face_of_edge_step(struct BMIter *iter);
void  bmiter__vert_of_edge_begin(struct BMIter *iter);
void *bmiter__vert_of_edge_step(struct BMIter *iter);
void  bmiter__vert_of_face_begin(struct BMIter *iter);
void *bmiter__vert_of_face_step(struct BMIter *iter);
void  bmiter__edge_of_face_begin(struct BMIter *iter);
void *bmiter__edge_of_face_step(struct BMIter *iter);
void  bmiter__loop_of_face_begin(struct BMIter *iter);
void *bmiter__loop_of_face_step(struct BMIter *iter);

#include "intern/bmesh_iterators_inline.c"

#endif /* __BMESH_ITERATORS_H__ */
