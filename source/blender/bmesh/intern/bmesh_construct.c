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

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#define SELECT 1


/**
 * Fill in a vertex array from an edge array.
 *
 * \returns false if any verts aren't found.
 */
bool BM_verts_from_edges(BMVert **vert_arr, BMEdge **edge_arr, const int len)
{
	int i, i_prev = len - 1;
	for (i = 0; i < len; i++) {
		vert_arr[i] = BM_edge_share_vert(edge_arr[i_prev], edge_arr[i]);
		if (vert_arr[i] == NULL) {
			return false;
		}
		i_prev = i;
	}
	return true;
}

/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 *
 * \returns false if any edges aren't found.
 */
bool BM_edges_from_verts(BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
	int i, i_prev = len - 1;
	for (i = 0; i < len; i++) {
		edge_arr[i_prev] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
		if (edge_arr[i_prev] == NULL) {
			return false;
		}
		i_prev = i;
	}
	return true;
}

/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 * Creating edges as-needed.
 */
void BM_edges_from_verts_ensure(BMesh *bm, BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
	int i, i_prev = len - 1;
	for (i = 0; i < len; i++) {
		edge_arr[i_prev] = BM_edge_create(bm, vert_arr[i_prev], vert_arr[i], NULL, BM_CREATE_NO_DOUBLE);
		i_prev = i;
	}
}

/* prototypes */
static void bm_loop_attrs_copy(
        BMesh *source_mesh, BMesh *target_mesh,
        const BMLoop *source_loop, BMLoop *target_loop);

/**
 * \brief Make Quad/Triangle
 *
 * Creates a new quad or triangle from a list of 3 or 4 vertices.
 * If \a no_double is true, then a check is done to see if a face
 * with these vertices already exists and returns it instead.
 *
 * If a pointer to an example face is provided, it's custom data
 * and properties will be copied to the new face.
 *
 * \note The winding of the face is determined by the order
 * of the vertices in the vertex array.
 */

BMFace *BM_face_create_quad_tri(
        BMesh *bm,
        BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
        const BMFace *f_example, const eBMCreateFlag create_flag)
{
	BMVert *vtar[4] = {v1, v2, v3, v4};
	return BM_face_create_verts(bm, vtar, v4 ? 4 : 3, f_example, create_flag, true);
}

/**
 * \brief copies face loop data from shared adjacent faces.
 *
 * \param filter_fn  A function that filters the source loops before copying (don't always want to copy all)
 *
 * \note when a matching edge is found, both loops of that edge are copied
 * this is done since the face may not be completely surrounded by faces,
 * this way: a quad with 2 connected quads on either side will still get all 4 loops updated
 */
void BM_face_copy_shared(
        BMesh *bm, BMFace *f,
        BMLoopFilterFunc filter_fn, void *user_data)
{
	BMLoop *l_first;
	BMLoop *l_iter;

#ifdef DEBUG
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BLI_assert(BM_ELEM_API_FLAG_TEST(l_iter, _FLAG_OVERLAP) == 0);
	} while ((l_iter = l_iter->next) != l_first);
#endif

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BMLoop *l_other = l_iter->radial_next;

		if (l_other && l_other != l_iter) {
			BMLoop *l_src[2];
			BMLoop *l_dst[2] = {l_iter, l_iter->next};
			uint j;

			if (l_other->v == l_iter->v) {
				l_src[0] = l_other;
				l_src[1] = l_other->next;
			}
			else {
				l_src[0] = l_other->next;
				l_src[1] = l_other;
			}

			for (j = 0; j < 2; j++) {
				BLI_assert(l_dst[j]->v == l_src[j]->v);
				if (BM_ELEM_API_FLAG_TEST(l_dst[j], _FLAG_OVERLAP) == 0) {
					if ((filter_fn == NULL) || filter_fn(l_src[j], user_data)) {
						bm_loop_attrs_copy(bm, bm, l_src[j], l_dst[j]);
						BM_ELEM_API_FLAG_ENABLE(l_dst[j], _FLAG_OVERLAP);
					}
				}
			}
		}
	} while ((l_iter = l_iter->next) != l_first);


	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BM_ELEM_API_FLAG_DISABLE(l_iter, _FLAG_OVERLAP);
	} while ((l_iter = l_iter->next) != l_first);
}

/**
 * Given an array of edges,
 * order them using the winding defined by \a v1 & \a v2
 * into \a edges_sort & \a verts_sort.
 *
 * All arrays must be \a len long.
 */
static bool bm_edges_sort_winding(
        BMVert *v1, BMVert *v2,
        BMEdge **edges, const int len,
        BMEdge **edges_sort, BMVert **verts_sort)
{
	BMEdge *e_iter, *e_first;
	BMVert *v_iter;
	int i;

	/* all flags _must_ be cleared on exit! */
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_ENABLE(edges[i], _FLAG_MF);
		BM_ELEM_API_FLAG_ENABLE(edges[i]->v1, _FLAG_MV);
		BM_ELEM_API_FLAG_ENABLE(edges[i]->v2, _FLAG_MV);
	}

	/* find first edge */
	i = 0;
	v_iter = v1;
	e_iter = e_first = v1->e;
	do {
		if (BM_ELEM_API_FLAG_TEST(e_iter, _FLAG_MF) &&
		    (BM_edge_other_vert(e_iter, v_iter) == v2))
		{
			i = 1;
			break;
		}
	} while ((e_iter = bmesh_disk_edge_next(e_iter, v_iter)) != e_first);
	if (i == 0) {
		goto error;
	}

	i = 0;
	do {
		/* entering loop will always succeed */
		if (BM_ELEM_API_FLAG_TEST(e_iter, _FLAG_MF)) {
			if (UNLIKELY(BM_ELEM_API_FLAG_TEST(v_iter, _FLAG_MV) == false)) {
				/* vert is in loop multiple times */
				goto error;
			}

			BM_ELEM_API_FLAG_DISABLE(e_iter, _FLAG_MF);
			edges_sort[i] = e_iter;

			BM_ELEM_API_FLAG_DISABLE(v_iter, _FLAG_MV);
			verts_sort[i] = v_iter;

			i += 1;

			/* walk onto the next vertex */
			v_iter = BM_edge_other_vert(e_iter, v_iter);
			if (i == len) {
				if (UNLIKELY(v_iter != verts_sort[0])) {
					goto error;
				}
				break;
			}

			e_first = e_iter;
		}
	} while ((e_iter = bmesh_disk_edge_next(e_iter, v_iter)) != e_first);

	if (i == len) {
		return true;
	}

error:
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edges[i], _FLAG_MF);
		BM_ELEM_API_FLAG_DISABLE(edges[i]->v1, _FLAG_MV);
		BM_ELEM_API_FLAG_DISABLE(edges[i]->v2, _FLAG_MV);
	}

	return false;
}

/**
 * \brief Make NGon
 *
 * Makes an ngon from an unordered list of edges.
 * Verts \a v1 and \a v2 define the winding of the new face.
 *
 * \a edges are not required to be ordered, simply to to form
 * a single closed loop as a whole.
 *
 * \note While this function will work fine when the edges
 * are already sorted, if the edges are always going to be sorted,
 * #BM_face_create should be considered over this function as it
 * avoids some unnecessary work.
 */
BMFace *BM_face_create_ngon(
        BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, const int len,
        const BMFace *f_example, const eBMCreateFlag create_flag)
{
	BMEdge **edges_sort = BLI_array_alloca(edges_sort, len);
	BMVert **verts_sort = BLI_array_alloca(verts_sort, len);

	BLI_assert(len && v1 && v2 && edges && bm);

	if (bm_edges_sort_winding(v1, v2, edges, len, edges_sort, verts_sort)) {
		return BM_face_create(bm, verts_sort, edges_sort, len, f_example, create_flag);
	}

	return NULL;
}

/**
 * Create an ngon from an array of sorted verts
 *
 * Special features this has over other functions.
 * - Optionally calculate winding based on surrounding edges.
 * - Optionally create edges between vertices.
 * - Uses verts so no need to find edges (handy when you only have verts)
 */
BMFace *BM_face_create_ngon_verts(
        BMesh *bm, BMVert **vert_arr, const int len,
        const BMFace *f_example, const eBMCreateFlag create_flag,
        const bool calc_winding, const bool create_edges)
{
	BMEdge **edge_arr = BLI_array_alloca(edge_arr, len);
	uint winding[2] = {0, 0};
	int i, i_prev = len - 1;
	BMVert *v_winding[2] = {vert_arr[i_prev], vert_arr[0]};

	BLI_assert(len > 2);

	for (i = 0; i < len; i++) {
		if (create_edges) {
			edge_arr[i] = BM_edge_create(bm, vert_arr[i_prev], vert_arr[i], NULL, BM_CREATE_NO_DOUBLE);
		}
		else {
			edge_arr[i] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
			if (edge_arr[i] == NULL) {
				return NULL;
			}
		}

		if (calc_winding) {
			/* the edge may exist already and be attached to a face
			 * in this case we can find the best winding to use for the new face */
			if (edge_arr[i]->l) {
				BMVert *test_v1, *test_v2;
				/* we want to use the reverse winding to the existing order */
				BM_edge_ordered_verts(edge_arr[i], &test_v2, &test_v1);
				winding[(vert_arr[i_prev] == test_v2)]++;
				BLI_assert(vert_arr[i_prev] == test_v2 || vert_arr[i_prev] == test_v1);
			}
		}

		i_prev = i;
	}

	/* --- */

	if (calc_winding) {
		if (winding[0] < winding[1]) {
			winding[0] = 1;
			winding[1] = 0;
		}
		else {
			winding[0] = 0;
			winding[1] = 1;
		}
	}
	else {
		winding[0] = 0;
		winding[1] = 1;
	}

	/* --- */

	/* create the face */
	return BM_face_create_ngon(
	        bm,
	        v_winding[winding[0]],
	        v_winding[winding[1]],
	        edge_arr, len,
	        f_example, create_flag);
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
void BM_verts_sort_radial_plane(BMVert **vert_arr, int len)
{
	struct SortIntByFloat *vang = BLI_array_alloca(vang, len);
	BMVert **vert_arr_map = BLI_array_alloca(vert_arr_map, len);

	float totv_inv = 1.0f / (float)len;
	int i = 0;

	float cent[3], nor[3];

	const float *far = NULL, *far_cross = NULL;

	float far_vec[3];
	float far_cross_vec[3];
	float sign_vec[3]; /* work out if we are pos/neg angle */

	float far_dist_sq, far_dist_max_sq;
	float far_cross_dist, far_cross_best = 0.0f;

	/* get the center point and collect vector array since we loop over these a lot */
	zero_v3(cent);
	for (i = 0; i < len; i++) {
		madd_v3_v3fl(cent, vert_arr[i]->co, totv_inv);
	}


	/* find the far point from cent */
	far_dist_max_sq = 0.0f;
	for (i = 0; i < len; i++) {
		far_dist_sq = len_squared_v3v3(vert_arr[i]->co, cent);
		if (far_dist_sq > far_dist_max_sq || far == NULL) {
			far = vert_arr[i]->co;
			far_dist_max_sq = far_dist_sq;
		}
	}

	sub_v3_v3v3(far_vec, far, cent);
	// far_dist = len_v3(far_vec); /* real dist */ /* UNUSED */

	/* --- */

	/* find a point 90deg about to compare with */
	far_cross_best = 0.0f;
	for (i = 0; i < len; i++) {

		if (far == vert_arr[i]->co) {
			continue;
		}

		sub_v3_v3v3(far_cross_vec, vert_arr[i]->co, cent);
		far_cross_dist = normalize_v3(far_cross_vec);

		/* more of a weight then a distance */
		far_cross_dist = (/* first we want to have a value close to zero mapped to 1 */
		                  1.0f - fabsf(dot_v3v3(far_vec, far_cross_vec)) *

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
	for (i = 0; i < len; i++) {
		vang[i].sort_value = angle_signed_on_axis_v3v3v3_v3(far, cent, vert_arr[i]->co, nor);
		vang[i].data = i;
		vert_arr_map[i] = vert_arr[i];
	}

	/* sort by angle and magic! - we have our ngon */
	qsort(vang, len, sizeof(*vang), BLI_sortutil_cmp_float);

	/* --- */

	for (i = 0; i < len; i++) {
		vert_arr[i] = vert_arr_map[vang[i].data];
	}
}

/*************************************************************/


static void bm_vert_attrs_copy(
        BMesh *source_mesh, BMesh *target_mesh,
        const BMVert *source_vertex, BMVert *target_vertex)
{
	if ((source_mesh == target_mesh) && (source_vertex == target_vertex)) {
		BLI_assert(!"BMVert: source and targer match");
		return;
	}
	copy_v3_v3(target_vertex->no, source_vertex->no);
	CustomData_bmesh_free_block_data(&target_mesh->vdata, target_vertex->head.data);
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata,
	                           source_vertex->head.data, &target_vertex->head.data);
}

static void bm_edge_attrs_copy(
        BMesh *source_mesh, BMesh *target_mesh,
        const BMEdge *source_edge, BMEdge *target_edge)
{
	if ((source_mesh == target_mesh) && (source_edge == target_edge)) {
		BLI_assert(!"BMEdge: source and targer match");
		return;
	}
	CustomData_bmesh_free_block_data(&target_mesh->edata, target_edge->head.data);
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata,
	                           source_edge->head.data, &target_edge->head.data);
}

static void bm_loop_attrs_copy(
        BMesh *source_mesh, BMesh *target_mesh,
        const BMLoop *source_loop, BMLoop *target_loop)
{
	if ((source_mesh == target_mesh) && (source_loop == target_loop)) {
		BLI_assert(!"BMLoop: source and targer match");
		return;
	}
	CustomData_bmesh_free_block_data(&target_mesh->ldata, target_loop->head.data);
	CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata,
	                           source_loop->head.data, &target_loop->head.data);
}

static void bm_face_attrs_copy(
        BMesh *source_mesh, BMesh *target_mesh,
        const BMFace *source_face, BMFace *target_face)
{
	if ((source_mesh == target_mesh) && (source_face == target_face)) {
		BLI_assert(!"BMFace: source and targer match");
		return;
	}
	copy_v3_v3(target_face->no, source_face->no);
	CustomData_bmesh_free_block_data(&target_mesh->pdata, target_face->head.data);
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata,
	                           source_face->head.data, &target_face->head.data);
	target_face->mat_nr = source_face->mat_nr;
}

/* BMESH_TODO: Special handling for hide flags? */
/* BMESH_TODO: swap src/dst args, everywhere else in bmesh does other way round */

/**
 * Copies attributes, e.g. customdata, header flags, etc, from one element
 * to another of the same type.
 */
void BM_elem_attrs_copy_ex(
        BMesh *bm_src, BMesh *bm_dst, const void *ele_src_v, void *ele_dst_v,
        const char hflag_mask)
{
	const BMHeader *ele_src = ele_src_v;
	BMHeader *ele_dst = ele_dst_v;

	BLI_assert(ele_src->htype == ele_dst->htype);
	BLI_assert(ele_src != ele_dst);

	if ((hflag_mask & BM_ELEM_SELECT) == 0) {
		/* First we copy select */
		if (BM_elem_flag_test((BMElem *)ele_src, BM_ELEM_SELECT)) {
			BM_elem_select_set(bm_dst, (BMElem *)ele_dst, true);
		}
	}

	/* Now we copy flags */
	if (hflag_mask == 0) {
		ele_dst->hflag = ele_src->hflag;
	}
	else if (hflag_mask == 0xff) {
		/* pass */
	}
	else {
		ele_dst->hflag = ((ele_dst->hflag & hflag_mask) | (ele_src->hflag & ~hflag_mask));
	}

	/* Copy specific attributes */
	switch (ele_dst->htype) {
		case BM_VERT:
			bm_vert_attrs_copy(bm_src, bm_dst, (const BMVert *)ele_src, (BMVert *)ele_dst);
			break;
		case BM_EDGE:
			bm_edge_attrs_copy(bm_src, bm_dst, (const BMEdge *)ele_src, (BMEdge *)ele_dst);
			break;
		case BM_LOOP:
			bm_loop_attrs_copy(bm_src, bm_dst, (const BMLoop *)ele_src, (BMLoop *)ele_dst);
			break;
		case BM_FACE:
			bm_face_attrs_copy(bm_src, bm_dst, (const BMFace *)ele_src, (BMFace *)ele_dst);
			break;
		default:
			BLI_assert(0);
			break;
	}
}

void BM_elem_attrs_copy(BMesh *bm_src, BMesh *bm_dst, const void *ele_src, void *ele_dst)
{
	/* BMESH_TODO, default 'use_flags' to false */
	BM_elem_attrs_copy_ex(bm_src, bm_dst, ele_src, ele_dst, BM_ELEM_SELECT);
}

void BM_elem_select_copy(BMesh *bm_dst, void *ele_dst_v, const void *ele_src_v)
{
	BMHeader *ele_dst = ele_dst_v;
	const BMHeader *ele_src = ele_src_v;

	BLI_assert(ele_src->htype == ele_dst->htype);

	if ((ele_src->hflag & BM_ELEM_SELECT) != (ele_dst->hflag & BM_ELEM_SELECT)) {
		BM_elem_select_set(bm_dst, (BMElem *)ele_dst, (ele_src->hflag & BM_ELEM_SELECT) != 0);
	}
}

/* helper function for 'BM_mesh_copy' */
static BMFace *bm_mesh_copy_new_face(
        BMesh *bm_new, BMesh *bm_old,
        BMVert **vtable, BMEdge **etable,
        BMFace *f)
{
	BMLoop **loops = BLI_array_alloca(loops, f->len);
	BMVert **verts = BLI_array_alloca(verts, f->len);
	BMEdge **edges = BLI_array_alloca(edges, f->len);

	BMFace *f_new;
	BMLoop *l_iter, *l_first;
	int j;

	j = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		loops[j] = l_iter;
		verts[j] = vtable[BM_elem_index_get(l_iter->v)];
		edges[j] = etable[BM_elem_index_get(l_iter->e)];
		j++;
	} while ((l_iter = l_iter->next) != l_first);

	f_new = BM_face_create(bm_new, verts, edges, f->len, NULL, BM_CREATE_SKIP_CD);

	if (UNLIKELY(f_new == NULL)) {
		return NULL;
	}

	/* use totface in case adding some faces fails */
	BM_elem_index_set(f_new, (bm_new->totface - 1)); /* set_inline */

	BM_elem_attrs_copy_ex(bm_old, bm_new, f, f_new, 0xff);
	f_new->head.hflag = f->head.hflag;  /* low level! don't do this for normal api use */

	j = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
	do {
		BM_elem_attrs_copy(bm_old, bm_new, loops[j], l_iter);
		j++;
	} while ((l_iter = l_iter->next) != l_first);

	return f_new;
}

void BM_mesh_copy_init_customdata(BMesh *bm_dst, BMesh *bm_src, const BMAllocTemplate *allocsize)
{
	if (allocsize == NULL) {
		allocsize = &bm_mesh_allocsize_default;
	}

	CustomData_copy(&bm_src->vdata, &bm_dst->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_src->edata, &bm_dst->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_src->ldata, &bm_dst->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_src->pdata, &bm_dst->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm_dst->vdata, allocsize->totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm_dst->edata, allocsize->totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm_dst->ldata, allocsize->totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm_dst->pdata, allocsize->totface, BM_FACE);
}


BMesh *BM_mesh_copy(BMesh *bm_old)
{
	BMesh *bm_new;
	BMVert *v, *v_new, **vtable = NULL;
	BMEdge *e, *e_new, **etable = NULL;
	BMFace *f, *f_new, **ftable = NULL;
	BMElem **eletable;
	BMEditSelection *ese;
	BMIter iter;
	int i;
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_BM(bm_old);

	/* allocate a bmesh */
	bm_new = BM_mesh_create(
	        &allocsize,
	        &((struct BMeshCreateParams){.use_toolflags = bm_old->use_toolflags,}));

	BM_mesh_copy_init_customdata(bm_new, bm_old, &allocsize);

	vtable = MEM_mallocN(sizeof(BMVert *) * bm_old->totvert, "BM_mesh_copy vtable");
	etable = MEM_mallocN(sizeof(BMEdge *) * bm_old->totedge, "BM_mesh_copy etable");
	ftable = MEM_mallocN(sizeof(BMFace *) * bm_old->totface, "BM_mesh_copy ftable");

	BM_ITER_MESH_INDEX (v, &iter, bm_old, BM_VERTS_OF_MESH, i) {
		/* copy between meshes so cant use 'example' argument */
		v_new = BM_vert_create(bm_new, v->co, NULL, BM_CREATE_SKIP_CD);
		BM_elem_attrs_copy_ex(bm_old, bm_new, v, v_new, 0xff);
		v_new->head.hflag = v->head.hflag;  /* low level! don't do this for normal api use */
		vtable[i] = v_new;
		BM_elem_index_set(v, i); /* set_inline */
		BM_elem_index_set(v_new, i); /* set_inline */
	}
	bm_old->elem_index_dirty &= ~BM_VERT;
	bm_new->elem_index_dirty &= ~BM_VERT;

	/* safety check */
	BLI_assert(i == bm_old->totvert);
	
	BM_ITER_MESH_INDEX (e, &iter, bm_old, BM_EDGES_OF_MESH, i) {
		e_new = BM_edge_create(bm_new,
		                       vtable[BM_elem_index_get(e->v1)],
		                       vtable[BM_elem_index_get(e->v2)],
		                       e, BM_CREATE_SKIP_CD);

		BM_elem_attrs_copy_ex(bm_old, bm_new, e, e_new, 0xff);
		e_new->head.hflag = e->head.hflag;  /* low level! don't do this for normal api use */
		etable[i] = e_new;
		BM_elem_index_set(e, i); /* set_inline */
		BM_elem_index_set(e_new, i); /* set_inline */
	}
	bm_old->elem_index_dirty &= ~BM_EDGE;
	bm_new->elem_index_dirty &= ~BM_EDGE;

	/* safety check */
	BLI_assert(i == bm_old->totedge);
	
	BM_ITER_MESH_INDEX (f, &iter, bm_old, BM_FACES_OF_MESH, i) {
		BM_elem_index_set(f, i); /* set_inline */

		f_new = bm_mesh_copy_new_face(bm_new, bm_old, vtable, etable, f);

		ftable[i] = f_new;

		if (f == bm_old->act_face) bm_new->act_face = f_new;
	}
	bm_old->elem_index_dirty &= ~BM_FACE;
	bm_new->elem_index_dirty &= ~BM_FACE;


	/* low level! don't do this for normal api use */
	bm_new->totvertsel = bm_old->totvertsel;
	bm_new->totedgesel = bm_old->totedgesel;
	bm_new->totfacesel = bm_old->totfacesel;

	/* safety check */
	BLI_assert(i == bm_old->totface);

	/* copy over edit selection history */
	for (ese = bm_old->selected.first; ese; ese = ese->next) {
		BMElem *ele = NULL;

		switch (ese->htype) {
			case BM_VERT:
				eletable = (BMElem **)vtable;
				break;
			case BM_EDGE:
				eletable = (BMElem **)etable;
				break;
			case BM_FACE:
				eletable = (BMElem **)ftable;
				break;
			default:
				eletable = NULL;
				break;
		}

		if (eletable) {
			ele = eletable[BM_elem_index_get(ese->ele)];
			if (ele) {
				BM_select_history_store(bm_new, ele);
			}
		}
	}

	MEM_freeN(etable);
	MEM_freeN(vtable);
	MEM_freeN(ftable);

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
	         ((meflag & ME_EDGEDRAW)   ? BM_ELEM_DRAW   : 0) |
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

	return ( ((hflag & BM_ELEM_SELECT)       ? SELECT       : 0) |
	         ((hflag & BM_ELEM_SEAM)         ? ME_SEAM      : 0) |
	         ((hflag & BM_ELEM_DRAW)         ? ME_EDGEDRAW  : 0) |
	         ((hflag & BM_ELEM_SMOOTH) == 0  ? ME_SHARP     : 0) |
	         ((hflag & BM_ELEM_HIDDEN)       ? ME_HIDE      : 0) |
	         ((BM_edge_is_wire(eed))         ? ME_LOOSEEDGE : 0) | /* not typical */
	         ME_EDGERENDER
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
