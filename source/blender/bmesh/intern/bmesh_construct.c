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

/** \file blender/bmesh/intern/bmesh_construct.c
 *  \ingroup bmesh
 *
 * BM construction functions.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#define SELECT 1

/* prototypes */
static void bm_loop_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMLoop *source_loop, BMLoop *target_loop);

/**
 * \brief Make Quad/Triangle
 *
 * Creates a new quad or triangle from a list of 3 or 4 vertices.
 * If \a nodouble is TRUE, then a check is done to see if a face
 * with these vertices already exists and returns it instead.
 *
 * If a pointer to an example face is provided, it's custom data
 * and properties will be copied to the new face.
 *
 * \note The winding of the face is determined by the order
 * of the vertices in the vertex array.
 */

BMFace *BM_face_create_quad_tri(BMesh *bm,
                                BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                const BMFace *example, const int nodouble)
{
	BMVert *vtar[4] = {v1, v2, v3, v4};
	return BM_face_create_quad_tri_v(bm, vtar, v4 ? 4 : 3, example, nodouble);
}

BMFace *BM_face_create_quad_tri_v(BMesh *bm, BMVert **verts, int len, const BMFace *example, const int nodouble)
{
	BMFace *f = NULL;
	int is_overlap = FALSE;

	/* sanity check - debug mode only */
	if (len == 3) {
		BLI_assert(verts[0] != verts[1]);
		BLI_assert(verts[0] != verts[2]);
		BLI_assert(verts[1] != verts[2]);
	}
	else if (len == 4) {
		BLI_assert(verts[0] != verts[1]);
		BLI_assert(verts[0] != verts[2]);
		BLI_assert(verts[0] != verts[3]);

		BLI_assert(verts[1] != verts[2]);
		BLI_assert(verts[1] != verts[3]);

		BLI_assert(verts[2] != verts[3]);
	}
	else {
		BLI_assert(0);
	}


	if (nodouble) {
		/* check if face exists or overlaps */
		is_overlap = BM_face_exists(bm, verts, len, &f);
	}

	/* make new face */
	if ((f == NULL) && (!is_overlap)) {
		BMEdge *edar[4] = {NULL};
		edar[0] = BM_edge_create(bm, verts[0], verts[1], NULL, TRUE);
		edar[1] = BM_edge_create(bm, verts[1], verts[2], NULL, TRUE);
		if (len == 4) {
			edar[2] = BM_edge_create(bm, verts[2], verts[3], NULL, TRUE);
			edar[3] = BM_edge_create(bm, verts[3], verts[0], NULL, TRUE);
		}
		else {
			edar[2] = BM_edge_create(bm, verts[2], verts[0], NULL, TRUE);
		}

		f = BM_face_create(bm, verts, edar, len, FALSE);

		if (example && f) {
			BM_elem_attrs_copy(bm, bm, example, f);
		}
	}

	return f;
}

/**
 * \brief copies face loop data from shared adjacent faces.
 * \note when a matching edge is found, both loops of that edge are copied
 * this is done since the face may not be completely surrounded by faces,
 * this way: a quad with 2 connected quads on either side will still get all 4 loops updated */
void BM_face_copy_shared(BMesh *bm, BMFace *f)
{
	BMLoop *l_first;
	BMLoop *l_iter;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BMLoop *l_other = l_iter->radial_next;

		if (l_other && l_other != l_iter) {
			if (l_other->v == l_iter->v) {
				bm_loop_attrs_copy(bm, bm, l_other, l_iter);
				bm_loop_attrs_copy(bm, bm, l_other->next, l_iter->next);
			}
			else {
				bm_loop_attrs_copy(bm, bm, l_other->next, l_iter);
				bm_loop_attrs_copy(bm, bm, l_other, l_iter->next);
			}
			/* since we copy both loops of the shared edge, step over the next loop here */
			if ((l_iter = l_iter->next) == l_first) {
				break;
			}
		}
	} while ((l_iter = l_iter->next) != l_first);
}

/**
 * \brief Make NGon
 *
 * Makes an ngon from an unordered list of edges. \a v1 and \a v2
 * must be the verts defining edges[0],
 * and define the winding of the new face.
 *
 * \a edges are not required to be ordered, simply to to form
 * a single closed loop as a whole.
 *
 * \note While this function will work fine when the edges
 * are already sorted, if the edges are always going to be sorted,
 * #BM_face_create should be considered over this function as it
 * avoids some unnecessary work.
 */
BMFace *BM_face_create_ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble)
{
	BMEdge **edges2 = NULL;
	BLI_array_staticdeclare(edges2, BM_NGON_STACK_SIZE);
	BMVert **verts = NULL;
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	BMFace *f = NULL;
	BMEdge *e;
	BMVert *v, *ev1, *ev2;
	int i, /* j, */ v1found, reverse;

	/* this code is hideous, yeek.  I'll have to think about ways of
	 *  cleaning it up.  basically, it now combines the old BM_face_create_ngon
	 *  _and_ the old bmesh_mf functions, so its kindof smashed together
	 * - joeedh */

	if (!len || !v1 || !v2 || !edges || !bm)
		return NULL;

	/* put edges in correct order */
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_ENABLE(edges[i], _FLAG_MF);
	}

	ev1 = edges[0]->v1;
	ev2 = edges[0]->v2;

	if (v1 == ev2) {
		/* Swapping here improves performance and consistency of face
		 * structure in the special case that the edges are already in
		 * the correct order and winding */
		SWAP(BMVert *, ev1, ev2);
	}

	BLI_array_append(verts, ev1);
	v = ev2;
	e = edges[0];
	do {
		BMEdge *e2 = e;

		BLI_array_append(verts, v);
		BLI_array_append(edges2, e);

		do {
			e2 = bmesh_disk_edge_next(e2, v);
			if (e2 != e && BM_ELEM_API_FLAG_TEST(e2, _FLAG_MF)) {
				v = BM_edge_other_vert(e2, v);
				break;
			}
		} while (e2 != e);

		if (e2 == e)
			goto err; /* the edges do not form a closed loop */

		e = e2;
	} while (e != edges[0]);

	if (BLI_array_count(edges2) != len) {
		goto err; /* we didn't use all edges in forming the boundary loop */
	}

	/* ok, edges are in correct order, now ensure they are going
	 * in the correct direction */
	v1found = reverse = FALSE;
	for (i = 0; i < len; i++) {
		if (BM_vert_in_edge(edges2[i], v1)) {
			/* see if v1 and v2 are in the same edge */
			if (BM_vert_in_edge(edges2[i], v2)) {
				/* if v1 is shared by the *next* edge, then the winding
				 * is incorrect */
				if (BM_vert_in_edge(edges2[(i + 1) % len], v1)) {
					reverse = TRUE;
					break;
				}
			}

			v1found = TRUE;
		}

		if ((v1found == FALSE) && BM_vert_in_edge(edges2[i], v2)) {
			reverse = TRUE;
			break;
		}
	}

	if (reverse) {
		for (i = 0; i < len / 2; i++) {
			v = verts[i];
			verts[i] = verts[len - i - 1];
			verts[len - i - 1] = v;
		}
	}

	for (i = 0; i < len; i++) {
		edges2[i] = BM_edge_exists(verts[i], verts[(i + 1) % len]);
		if (!edges2[i]) {
			goto err;
		}
	}

	f = BM_face_create(bm, verts, edges2, len, nodouble);

	/* clean up flags */
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edges2[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return f;

err:
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edges[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return NULL;
}

typedef struct AngleIndexPair {
	float angle;
	int index;
} AngleIndexPair;

static int angle_index_pair_cmp(const void *e1, const void *e2)
{
	const AngleIndexPair *p1 = e1, *p2 = e2;
	if      (p1->angle > p2->angle) return  1;
	else if (p1->angle < p2->angle) return -1;
	else return 0;
}

/**
 * Makes an NGon from an un-ordered set of verts
 *
 * assumes...
 * - that verts are only once in the list.
 * - that the verts have roughly planer bounds
 * - that the verts are roughly circular
 * there can be concave areas but overlapping folds from the center point will fail.
 *
 * a brief explanation of the method used
 * - find the center point
 * - find the normal of the vcloud
 * - order the verts around the face based on their angle to the normal vector at the center point.
 *
 * \note Since this is a vcloud there is no direction.
 */
BMFace *BM_face_create_ngon_vcloud(BMesh *bm, BMVert **vert_arr, int totv, int nodouble)
{
	BMFace *f;

	float totv_inv = 1.0f / (float)totv;
	int i = 0;

	float cent[3], nor[3];

	float *far = NULL, *far_cross = NULL;

	float far_vec[3];
	float far_cross_vec[3];
	float sign_vec[3]; /* work out if we are pos/neg angle */

	float far_dist, far_best;
	float far_cross_dist, far_cross_best = 0.0f;

	AngleIndexPair *vang;

	BMVert **vert_arr_map;
	BMEdge **edge_arr;
	int i_prev;

	unsigned int winding[2] = {0, 0};

	/* get the center point and collect vector array since we loop over these a lot */
	zero_v3(cent);
	for (i = 0; i < totv; i++) {
		madd_v3_v3fl(cent, vert_arr[i]->co, totv_inv);
	}


	/* find the far point from cent */
	far_best = 0.0f;
	for (i = 0; i < totv; i++) {
		far_dist = len_squared_v3v3(vert_arr[i]->co, cent);
		if (far_dist > far_best || far == NULL) {
			far = vert_arr[i]->co;
			far_best = far_dist;
		}
	}

	sub_v3_v3v3(far_vec, far, cent);
	far_dist = len_v3(far_vec); /* real dist */

	/* --- */

	/* find a point 90deg about to compare with */
	far_cross_best = 0.0f;
	for (i = 0; i < totv; i++) {

		if (far == vert_arr[i]->co) {
			continue;
		}

		sub_v3_v3v3(far_cross_vec, vert_arr[i]->co, cent);
		far_cross_dist = normalize_v3(far_cross_vec);

		/* more of a weight then a distance */
		far_cross_dist = (/* first we want to have a value close to zero mapped to 1 */
						  1.0 - fabsf(dot_v3v3(far_vec, far_cross_vec)) *

						  /* second  we multiply by the distance
						   * so points close to the center are not preferred */
						  far_cross_dist);

		if (far_cross_dist > far_cross_best || far_cross == NULL) {
			far_cross = vert_arr[i]->co;
			far_cross_best = far_cross_dist;
		}
	}

	sub_v3_v3v3(far_cross_vec, far_cross, cent);

	/* --- */

	/* now we have 2 vectors we can have a cross product */
	cross_v3_v3v3(nor, far_vec, far_cross_vec);
	normalize_v3(nor);
	cross_v3_v3v3(sign_vec, far_vec, nor); /* this vector should match 'far_cross_vec' closely */

	/* --- */

	/* now calculate every points angle around the normal (signed) */
	vang = MEM_mallocN(sizeof(AngleIndexPair) * totv, __func__);

	for (i = 0; i < totv; i++) {
		float co[3];
		float proj_vec[3];
		float angle;

		/* center relative vec */
		sub_v3_v3v3(co, vert_arr[i]->co, cent);

		/* align to plane */
		project_v3_v3v3(proj_vec, co, nor);
		sub_v3_v3(co, proj_vec);

		/* now 'co' is valid - we can compare its angle against the far vec */
		angle = angle_v3v3(far_vec, co);

		if (dot_v3v3(co, sign_vec) < 0.0f) {
			angle = -angle;
		}

		vang[i].angle = angle;
		vang[i].index = i;
	}

	/* sort by angle and magic! - we have our ngon */
	qsort(vang, totv, sizeof(AngleIndexPair), angle_index_pair_cmp);

	/* --- */

	/* create edges and find the winding (if faces are attached to any existing edges) */
	vert_arr_map = MEM_mallocN(sizeof(BMVert **) * totv, __func__);
	edge_arr = MEM_mallocN(sizeof(BMEdge **) * totv, __func__);

	for (i = 0; i < totv; i++) {
		vert_arr_map[i] = vert_arr[vang[i].index];
	}
	MEM_freeN(vang);

	i_prev = totv - 1;
	for (i = 0; i < totv; i++) {
		edge_arr[i] = BM_edge_create(bm, vert_arr_map[i_prev], vert_arr_map[i], NULL, TRUE);

		/* the edge may exist already and be attached to a face
		 * in this case we can find the best winding to use for the new face */
		if (edge_arr[i]->l) {
			BMVert *test_v1, *test_v2;
			/* we want to use the reverse winding to the existing order */
			BM_edge_ordered_verts(edge_arr[i], &test_v2, &test_v1);
			winding[(vert_arr_map[i_prev] == test_v2)]++;

		}

		i_prev = i;
	}

	/* --- */

	if (winding[0] < winding[1]) {
		winding[0] = 1;
		winding[1] = 0;
	}
	else {
		winding[0] = 0;
		winding[1] = 1;
	}

	/* --- */

	/* create the face */
	f = BM_face_create_ngon(bm, vert_arr_map[winding[0]], vert_arr_map[winding[1]], edge_arr, totv, nodouble);

	MEM_freeN(edge_arr);
	MEM_freeN(vert_arr_map);

	return f;
}

/**
 * Called by operators to remove elements that they have marked for
 * removal.
 */
void BMO_remove_tagged_faces(BMesh *bm, const short oflag)
{
	BMFace *f;
	BMIter iter;

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (BMO_elem_flag_test(bm, f, oflag)) {
			BM_face_kill(bm, f);
		}
	}
}

void BMO_remove_tagged_edges(BMesh *bm, const short oflag)
{
	BMEdge *e;
	BMIter iter;

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, oflag)) {
			BM_edge_kill(bm, e);
		}
	}
}

void BMO_remove_tagged_verts(BMesh *bm, const short oflag)
{
	BMVert *v;
	BMIter iter;

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, oflag)) {
			BM_vert_kill(bm, v);
		}
	}
}

/*************************************************************/
/* you need to make remove tagged verts/edges/faces
 * api functions that take a filter callback.....
 * and this new filter type will be for opstack flags.
 * This is because the BM_remove_taggedXXX functions bypass iterator API.
 *  - Ops don't care about 'UI' considerations like selection state, hide state, etc.
 *    If you want to work on unhidden selections for instance,
 *    copy output from a 'select context' operator to another operator....
 */

static void bmo_remove_tagged_context_verts(BMesh *bm, const short oflag)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter iter;
	BMIter itersub;

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, oflag)) {
			/* Visit edge */
			BM_ITER_ELEM (e, &itersub, v, BM_EDGES_OF_VERT) {
				BMO_elem_flag_enable(bm, e, oflag);
			}
			/* Visit face */
			BM_ITER_ELEM (f, &itersub, v, BM_FACES_OF_VERT) {
				BMO_elem_flag_enable(bm, f, oflag);
			}
		}
	}

	BMO_remove_tagged_faces(bm, oflag);
	BMO_remove_tagged_edges(bm, oflag);
	BMO_remove_tagged_verts(bm, oflag);
}

static void bmo_remove_tagged_context_edges(BMesh *bm, const short oflag)
{
	BMEdge *e;
	BMFace *f;

	BMIter iter;
	BMIter itersub;

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, oflag)) {
			BM_ITER_ELEM (f, &itersub, e, BM_FACES_OF_EDGE) {
				BMO_elem_flag_enable(bm, f, oflag);
			}
		}
	}
	BMO_remove_tagged_faces(bm, oflag);
	BMO_remove_tagged_edges(bm, oflag);
}

#define DEL_WIREVERT	(1 << 10)

/**
 * \warning oflag applies to different types in some contexts,
 * not just the type being removed.
 *
 * \warning take care, uses operator flag DEL_WIREVERT
 */
void BMO_remove_tagged_context(BMesh *bm, const short oflag, const int type)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter viter;
	BMIter eiter;
	BMIter fiter;

	switch (type) {
		case DEL_VERTS:
		{
			bmo_remove_tagged_context_verts(bm, oflag);

			break;
		}
		case DEL_EDGES:
		{
			/* flush down to vert */
			BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
				if (BMO_elem_flag_test(bm, e, oflag)) {
					BMO_elem_flag_enable(bm, e->v1, oflag);
					BMO_elem_flag_enable(bm, e->v2, oflag);
				}
			}
			bmo_remove_tagged_context_edges(bm, oflag);
			/* remove loose vertice */
			BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
				if (BMO_elem_flag_test(bm, v, oflag) && (!(v->e)))
					BMO_elem_flag_enable(bm, v, DEL_WIREVERT);
			}
			BMO_remove_tagged_verts(bm, DEL_WIREVERT);

			break;
		}
		case DEL_EDGESFACES:
		{
			bmo_remove_tagged_context_edges(bm, oflag);

			break;
		}
		case DEL_ONLYFACES:
		{
			BMO_remove_tagged_faces(bm, oflag);

			break;
		}
		case DEL_ONLYTAGGED:
		{
			BMO_remove_tagged_faces(bm, oflag);
			BMO_remove_tagged_edges(bm, oflag);
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
		case DEL_FACES:
		{
			/* go through and mark all edges and all verts of all faces for delet */
			BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
				if (BMO_elem_flag_test(bm, f, oflag)) {
					for (e = BM_iter_new(&eiter, bm, BM_EDGES_OF_FACE, f); e; e = BM_iter_step(&eiter))
						BMO_elem_flag_enable(bm, e, oflag);
					for (v = BM_iter_new(&viter, bm, BM_VERTS_OF_FACE, f); v; v = BM_iter_step(&viter))
						BMO_elem_flag_enable(bm, v, oflag);
				}
			}
			/* now go through and mark all remaining faces all edges for keeping */
			BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
				if (!BMO_elem_flag_test(bm, f, oflag)) {
					for (e = BM_iter_new(&eiter, bm, BM_EDGES_OF_FACE, f); e; e = BM_iter_step(&eiter)) {
						BMO_elem_flag_disable(bm, e, oflag);
					}
					for (v = BM_iter_new(&viter, bm, BM_VERTS_OF_FACE, f); v; v = BM_iter_step(&viter)) {
						BMO_elem_flag_disable(bm, v, oflag);
					}
				}
			}
			/* also mark all the vertices of remaining edges for keeping */
			BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
				if (!BMO_elem_flag_test(bm, e, oflag)) {
					BMO_elem_flag_disable(bm, e->v1, oflag);
					BMO_elem_flag_disable(bm, e->v2, oflag);
				}
			}
			/* now delete marked face */
			BMO_remove_tagged_faces(bm, oflag);
			/* delete marked edge */
			BMO_remove_tagged_edges(bm, oflag);
			/* remove loose vertice */
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
		case DEL_ALL:
		{
			/* does this option even belong in here? */
			BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
				BMO_elem_flag_enable(bm, f, oflag);
			}
			BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
				BMO_elem_flag_enable(bm, e, oflag);
			}
			BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
				BMO_elem_flag_enable(bm, v, oflag);
			}

			BMO_remove_tagged_faces(bm, oflag);
			BMO_remove_tagged_edges(bm, oflag);
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
	}
}
/*************************************************************/


static void bm_vert_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMVert *source_vertex, BMVert *target_vertex)
{
	if ((source_mesh == target_mesh) && (source_vertex == target_vertex)) {
		return;
	}
	copy_v3_v3(target_vertex->no, source_vertex->no);
	CustomData_bmesh_free_block(&target_mesh->vdata, &target_vertex->head.data);
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata,
	                           source_vertex->head.data, &target_vertex->head.data);
}

static void bm_edge_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMEdge *source_edge, BMEdge *target_edge)
{
	if ((source_mesh == target_mesh) && (source_edge == target_edge)) {
		return;
	}
	CustomData_bmesh_free_block(&target_mesh->edata, &target_edge->head.data);
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata,
	                           source_edge->head.data, &target_edge->head.data);
}

static void bm_loop_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMLoop *source_loop, BMLoop *target_loop)
{
	if ((source_mesh == target_mesh) && (source_loop == target_loop)) {
		return;
	}
	CustomData_bmesh_free_block(&target_mesh->ldata, &target_loop->head.data);
	CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata,
	                           source_loop->head.data, &target_loop->head.data);
}

static void bm_face_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMFace *source_face, BMFace *target_face)
{
	if ((source_mesh == target_mesh) && (source_face == target_face)) {
		return;
	}
	copy_v3_v3(target_face->no, source_face->no);
	CustomData_bmesh_free_block(&target_mesh->pdata, &target_face->head.data);
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata,
	                           source_face->head.data, &target_face->head.data);
	target_face->mat_nr = source_face->mat_nr;
}

/* BMESH_TODO: Special handling for hide flags? */

/**
 * Copies attributes, e.g. customdata, header flags, etc, from one element
 * to another of the same type.
 */
void BM_elem_attrs_copy(BMesh *source_mesh, BMesh *target_mesh, const void *source, void *target)
{
	const BMHeader *sheader = source;
	BMHeader *theader = target;

	BLI_assert(sheader->htype == theader->htype);

	if (sheader->htype != theader->htype)
		return;

	/* First we copy select */
	if (BM_elem_flag_test((BMElem *)sheader, BM_ELEM_SELECT)) {
		BM_elem_select_set(target_mesh, (BMElem *)target, TRUE);
	}
	
	/* Now we copy flags */
	theader->hflag = sheader->hflag;
	
	/* Copy specific attributes */
	switch (theader->htype) {
		case BM_VERT:
			bm_vert_attrs_copy(source_mesh, target_mesh, (const BMVert *)source, (BMVert *)target);
			break;
		case BM_EDGE:
			bm_edge_attrs_copy(source_mesh, target_mesh, (const BMEdge *)source, (BMEdge *)target);
			break;
		case BM_LOOP:
			bm_loop_attrs_copy(source_mesh, target_mesh, (const BMLoop *)source, (BMLoop *)target);
			break;
		case BM_FACE:
			bm_face_attrs_copy(source_mesh, target_mesh, (const BMFace *)source, (BMFace *)target);
			break;
		default:
			BLI_assert(0);
	}
}

BMesh *BM_mesh_copy(BMesh *bm_old)
{
	BMesh *bm_new;
	BMVert *v, *v2, **vtable = NULL;
	BMEdge *e, *e2, **edges = NULL, **etable = NULL;
	BLI_array_declare(edges);
	BMLoop *l, /* *l2, */ **loops = NULL;
	BLI_array_declare(loops);
	BMFace *f, *f2, **ftable = NULL;
	BMEditSelection *ese;
	BMIter iter, liter;
	int i, j;
	BMAllocTemplate allocsize = {bm_old->totvert,
	                             bm_old->totedge,
	                             bm_old->totloop,
	                             bm_old->totface};

	/* allocate a bmesh */
	bm_new = BM_mesh_create(&allocsize);

	CustomData_copy(&bm_old->vdata, &bm_new->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->edata, &bm_new->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->ldata, &bm_new->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->pdata, &bm_new->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm_new->vdata, allocsize.totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm_new->edata, allocsize.totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm_new->ldata, allocsize.totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm_new->pdata, allocsize.totface, BM_FACE);

	vtable = MEM_mallocN(sizeof(BMVert *) * bm_old->totvert, "BM_mesh_copy vtable");
	etable = MEM_mallocN(sizeof(BMEdge *) * bm_old->totedge, "BM_mesh_copy etable");
	ftable = MEM_mallocN(sizeof(BMFace *) * bm_old->totface, "BM_mesh_copy ftable");

	v = BM_iter_new(&iter, bm_old, BM_VERTS_OF_MESH, NULL);
	for (i = 0; v; v = BM_iter_step(&iter), i++) {
		v2 = BM_vert_create(bm_new, v->co, NULL); /* copy between meshes so cant use 'example' argument */
		BM_elem_attrs_copy(bm_old, bm_new, v, v2);
		vtable[i] = v2;
		BM_elem_index_set(v, i); /* set_inline */
		BM_elem_index_set(v2, i); /* set_inline */
	}
	bm_old->elem_index_dirty &= ~BM_VERT;
	bm_new->elem_index_dirty &= ~BM_VERT;

	/* safety check */
	BLI_assert(i == bm_old->totvert);
	
	e = BM_iter_new(&iter, bm_old, BM_EDGES_OF_MESH, NULL);
	for (i = 0; e; e = BM_iter_step(&iter), i++) {
		e2 = BM_edge_create(bm_new,
		                    vtable[BM_elem_index_get(e->v1)],
		                    vtable[BM_elem_index_get(e->v2)],
		                    e, FALSE);

		BM_elem_attrs_copy(bm_old, bm_new, e, e2);
		etable[i] = e2;
		BM_elem_index_set(e, i); /* set_inline */
		BM_elem_index_set(e2, i); /* set_inline */
	}
	bm_old->elem_index_dirty &= ~BM_EDGE;
	bm_new->elem_index_dirty &= ~BM_EDGE;

	/* safety check */
	BLI_assert(i == bm_old->totedge);
	
	f = BM_iter_new(&iter, bm_old, BM_FACES_OF_MESH, NULL);
	for (i = 0; f; f = BM_iter_step(&iter), i++) {
		BM_elem_index_set(f, i); /* set_inline */

		BLI_array_empty(loops);
		BLI_array_empty(edges);
		BLI_array_growitems(loops, f->len);
		BLI_array_growitems(edges, f->len);

		l = BM_iter_new(&liter, bm_old, BM_LOOPS_OF_FACE, f);
		for (j = 0; j < f->len; j++, l = BM_iter_step(&liter)) {
			loops[j] = l;
			edges[j] = etable[BM_elem_index_get(l->e)];
		}

		v = vtable[BM_elem_index_get(loops[0]->v)];
		v2 = vtable[BM_elem_index_get(loops[1]->v)];

		if (!bmesh_verts_in_edge(v, v2, edges[0])) {
			v = vtable[BM_elem_index_get(loops[BLI_array_count(loops) - 1]->v)];
			v2 = vtable[BM_elem_index_get(loops[0]->v)];
		}

		f2 = BM_face_create_ngon(bm_new, v, v2, edges, f->len, FALSE);
		if (!f2)
			continue;
		/* use totface in case adding some faces fails */
		BM_elem_index_set(f2, (bm_new->totface - 1)); /* set_inline */

		ftable[i] = f2;

		BM_elem_attrs_copy(bm_old, bm_new, f, f2);
		copy_v3_v3(f2->no, f->no);

		l = BM_iter_new(&liter, bm_new, BM_LOOPS_OF_FACE, f2);
		for (j = 0; j < f->len; j++, l = BM_iter_step(&liter)) {
			BM_elem_attrs_copy(bm_old, bm_new, loops[j], l);
		}

		if (f == bm_old->act_face) bm_new->act_face = f2;
	}
	bm_old->elem_index_dirty &= ~BM_FACE;
	bm_new->elem_index_dirty &= ~BM_FACE;

	/* safety check */
	BLI_assert(i == bm_old->totface);

	/* copy over edit selection history */
	for (ese = bm_old->selected.first; ese; ese = ese->next) {
		void *ele = NULL;

		if (ese->htype == BM_VERT)
			ele = vtable[BM_elem_index_get(ese->ele)];
		else if (ese->htype == BM_EDGE)
			ele = etable[BM_elem_index_get(ese->ele)];
		else if (ese->htype == BM_FACE) {
			ele = ftable[BM_elem_index_get(ese->ele)];
		}
		else {
			BLI_assert(0);
		}
		
		if (ele)
			BM_select_history_store(bm_new, ele);
	}

	MEM_freeN(etable);
	MEM_freeN(vtable);
	MEM_freeN(ftable);

	BLI_array_free(loops);
	BLI_array_free(edges);

	return bm_new;
}

/* ME -> BM */
char BM_vert_flag_from_mflag(const char  meflag)
{
	return ( ((meflag & SELECT)       ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_HIDE)      ? BM_ELEM_HIDDEN : 0)
	         );
}
char BM_edge_flag_from_mflag(const short meflag)
{
	return ( ((meflag & SELECT)        ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_SEAM)       ? BM_ELEM_SEAM   : 0) |
	         ((meflag & ME_SHARP) == 0 ? BM_ELEM_SMOOTH : 0) | /* invert */
	         ((meflag & ME_HIDE)       ? BM_ELEM_HIDDEN : 0)
	         );
}
char BM_face_flag_from_mflag(const char  meflag)
{
	return ( ((meflag & ME_FACE_SEL)  ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_SMOOTH)    ? BM_ELEM_SMOOTH : 0) |
	         ((meflag & ME_HIDE)      ? BM_ELEM_HIDDEN : 0)
	         );
}

/* BM -> ME */
char  BM_vert_flag_to_mflag(BMVert *eve)
{
	const char hflag = eve->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT)  ? SELECT  : 0) |
	         ((hflag & BM_ELEM_HIDDEN)  ? ME_HIDE : 0)
	         );
}
short BM_edge_flag_to_mflag(BMEdge *eed)
{
	const char hflag = eed->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT)       ? SELECT    : 0) |
	         ((hflag & BM_ELEM_SEAM)         ? ME_SEAM   : 0) |
	         ((hflag & BM_ELEM_SMOOTH) == 0  ? ME_SHARP  : 0) |
	         ((hflag & BM_ELEM_HIDDEN)       ? ME_HIDE   : 0) |
	         ((BM_edge_is_wire(eed)) ? ME_LOOSEEDGE : 0) | /* not typical */
	         (ME_EDGEDRAW | ME_EDGERENDER)
	         );
}
char  BM_face_flag_to_mflag(BMFace *efa)
{
	const char hflag = efa->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT) ? ME_FACE_SEL : 0) |
	         ((hflag & BM_ELEM_SMOOTH) ? ME_SMOOTH   : 0) |
	         ((hflag & BM_ELEM_HIDDEN) ? ME_HIDE     : 0)
	         );
}
