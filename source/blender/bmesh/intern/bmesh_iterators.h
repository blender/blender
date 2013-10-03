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

#include "BLI_compiler_attrs.h"
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

#define BM_ITER_MESH(ele, iter, bm, itype) \
	for (ele = BM_iter_new(iter, bm, itype, NULL); ele; ele = BM_iter_step(iter))

#define BM_ITER_MESH_INDEX(ele, iter, bm, itype, indexvar) \
	for (ele = BM_iter_new(iter, bm, itype, NULL), indexvar = 0; ele; ele = BM_iter_step(iter), (indexvar)++)

/* a version of BM_ITER_MESH which keeps the next item in storage
 * so we can delete the current item, see bug [#36923] */
#ifdef DEBUG
#  define BM_ITER_MESH_MUTABLE(ele, ele_next, iter, bm, itype) \
	for (ele = BM_iter_new(iter, bm, itype, NULL); \
	ele ? ((void)((iter)->count = BM_iter_mesh_count(bm, itype)), \
	       (void)(ele_next = BM_iter_step(iter)), 1) : 0; \
	ele = ele_next)
#else
#  define BM_ITER_MESH_MUTABLE(ele, ele_next, iter, bm, itype) \
	for (ele = BM_iter_new(iter, bm, itype, NULL); ele ? ((ele_next = BM_iter_step(iter)), 1) : 0; ele = ele_next)
#endif


#define BM_ITER_ELEM(ele, iter, data, itype) \
	for (ele = BM_iter_new(iter, NULL, itype, data); ele; ele = BM_iter_step(iter))

#define BM_ITER_ELEM_INDEX(ele, iter, data, itype, indexvar) \
	for (ele = BM_iter_new(iter, NULL, itype, data), indexvar = 0; ele; ele = BM_iter_step(iter), (indexvar)++)

/* iterator type structs */
struct BMIter__vert_of_mesh {
	BMesh *bm;
	BLI_mempool_iter pooliter;
};
struct BMIter__edge_of_mesh {
	BMesh *bm;
	BLI_mempool_iter pooliter;
};
struct BMIter__face_of_mesh {
	BMesh *bm;
	BLI_mempool_iter pooliter;
};
struct BMIter__edge_of_vert {
	BMVert *vdata;
	BMEdge *e_first, *e_next;
};
struct BMIter__face_of_vert {
	BMVert *vdata;
	BMLoop *l_first, *l_next;
	BMEdge *e_first, *e_next;
};
struct BMIter__loop_of_vert {
	BMVert *vdata;
	BMLoop *l_first, *l_next;
	BMEdge *e_first, *e_next;
};
struct BMIter__loop_of_edge {
	BMEdge *edata;
	BMLoop *l_first, *l_next;
};
struct BMIter__loop_of_loop {
	BMLoop *ldata;
	BMLoop *l_first, *l_next;
};
struct BMIter__face_of_edge {
	BMEdge *edata;
	BMLoop *l_first, *l_next;
};
struct BMIter__vert_of_edge {
	BMEdge *edata;
};
struct BMIter__vert_of_face {
	BMFace *pdata;
	BMLoop *l_first, *l_next;
};
struct BMIter__edge_of_face {
	BMFace *pdata;
	BMLoop *l_first, *l_next;
};
struct BMIter__loop_of_face {
	BMFace *pdata;
	BMLoop *l_first, *l_next;
};

typedef void  (*BMIter__begin_cb) (void *);
typedef void *(*BMIter__step_cb) (void *);

/* Iterator Structure */
/* note: some of these vars are not used,
 * so they have been commented to save stack space since this struct is used all over */
typedef struct BMIter {
	/* keep union first */
	union {
		struct BMIter__vert_of_mesh vert_of_mesh;
		struct BMIter__edge_of_mesh edge_of_mesh;
		struct BMIter__face_of_mesh face_of_mesh;

		struct BMIter__edge_of_vert edge_of_vert;
		struct BMIter__face_of_vert face_of_vert;
		struct BMIter__loop_of_vert loop_of_vert;
		struct BMIter__loop_of_edge loop_of_edge;
		struct BMIter__loop_of_loop loop_of_loop;
		struct BMIter__face_of_edge face_of_edge;
		struct BMIter__vert_of_edge vert_of_edge;
		struct BMIter__vert_of_face vert_of_face;
		struct BMIter__edge_of_face edge_of_face;
		struct BMIter__loop_of_face loop_of_face;
	} data;

	BMIter__begin_cb begin;
	BMIter__step_cb step;

	int count;  /* note, only some iterators set this, don't rely on it */
	char itype;
} BMIter;

int     BM_iter_mesh_count(BMesh *bm, const char itype);
void   *BM_iter_at_index(BMesh *bm, const char itype, void *data, int index) ATTR_WARN_UNUSED_RESULT;
int     BM_iter_as_array(BMesh *bm, const char itype, void *data, void **array, const int len);
void   *BM_iter_as_arrayN(BMesh *bm, const char itype, void *data, int *r_len,
                          void **stack_array, int stack_array_size) ATTR_WARN_UNUSED_RESULT;
int     BMO_iter_as_array(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const char restrictmask,
                          void **array, const int len);
void    *BMO_iter_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const char restrictmask,
                            int *r_len,
                            /* optional args to avoid an alloc (normally stack array) */
                            void **stack_array, int stack_array_size);

int     BM_iter_elem_count_flag(const char itype, void *data, const char hflag, const bool value);
int     BMO_iter_elem_count_flag(BMesh *bm, const char itype, void *data, const short oflag, const bool value);
int     BM_iter_mesh_count_flag(const char itype, BMesh *bm, const char hflag, const bool value);

/* private for bmesh_iterators_inline.c */

#define BMITER_CB_DEF(name) \
	struct BMIter__##name; \
	void  bmiter__##name##_begin(struct BMIter__##name *iter); \
	void *bmiter__##name##_step(struct BMIter__##name *iter)

BMITER_CB_DEF(vert_of_mesh);
BMITER_CB_DEF(edge_of_mesh);
BMITER_CB_DEF(face_of_mesh);
BMITER_CB_DEF(edge_of_vert);
BMITER_CB_DEF(face_of_vert);
BMITER_CB_DEF(loop_of_vert);
BMITER_CB_DEF(loop_of_edge);
BMITER_CB_DEF(loop_of_loop);
BMITER_CB_DEF(face_of_edge);
BMITER_CB_DEF(vert_of_edge);
BMITER_CB_DEF(vert_of_face);
BMITER_CB_DEF(edge_of_face);
BMITER_CB_DEF(loop_of_face);

#undef BMITER_CB_DEF

#include "intern/bmesh_iterators_inline.h"

#endif /* __BMESH_ITERATORS_H__ */
