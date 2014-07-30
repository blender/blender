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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_subdivide_edgering.c
 *  \ingroup bmesh
 *
 * This operator is a special edge-ring subdivision tool
 * which gives special options for interpolation.
 *
 * \note Tagging and flags
 * Tagging here is quite prone to errors if not done carefully.
 *
 * - With the exception of EDGE_RIM & EDGE_RIM,
 *   all flags need to be cleared on function exit.
 * - verts use BM_ELEM_TAG, these need to be cleared before functions exit.
 *
 * \note Order of execution with 2+ rings is undefined,
 * so tage care
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_stackdefines.h"
#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_curve.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define VERT_SHARED		(1 << 0)

#define EDGE_RING		(1 << 0)
#define EDGE_RIM		(1 << 1)
#define EDGE_IN_STACK	(1 << 2)

#define FACE_OUT		(1 << 0)
#define FACE_SHARED		(1 << 1)
#define FACE_IN_STACK	(1 << 2)


/* -------------------------------------------------------------------- */
/* Specialized Utility Funcs */

#ifndef NDEBUG
static unsigned int bm_verts_tag_count(BMesh *bm)
{
	int count = 0;
	BMIter iter;
	BMVert *v;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
			count++;
		}
	}
	return count;
}
#endif

static float bezier_handle_calc_length_v3(const float co_a[3], const float no_a[3],
                                          const float co_b[3], const float no_b[3])
{
	const float dot = dot_v3v3(no_a, no_b);
	/* gives closest approx at a circle with 2 parallel handles */
	float fac = 1.333333f;
	float len;
	if (dot < 0.0f) {
		/* scale down to 0.666 if we point directly at each other rough but ok */
		/* TODO, current blend from dot may not be optimal but its also a detail */
		const float t = 1.0f + dot;
		fac = (fac * t) + (0.75f * (1.0f - t));
	}

#if 0
	len = len_v3v3(co_a, co_b);
#else
	/* 2d length projected on plane of normals */
	{
		float co_a_ofs[3];
		cross_v3_v3v3(co_a_ofs, no_a, no_b);
		if (len_squared_v3(co_a_ofs) > FLT_EPSILON) {
			add_v3_v3(co_a_ofs, co_a);
			closest_to_line_v3(co_a_ofs, co_b, co_a, co_a_ofs);
		}
		else {
			copy_v3_v3(co_a_ofs, co_a);
		}
		len = len_v3v3(co_a_ofs, co_b);
	}
#endif

	return (len * 0.5f) * fac;
}

static void bm_edgeloop_vert_tag(struct BMEdgeLoopStore *el_store, const bool tag)
{
	LinkData *node = BM_edgeloop_verts_get(el_store)->first;
	do {
		BM_elem_flag_set((BMVert *)node->data, BM_ELEM_TAG, tag);
	} while ((node = node->next));
}

static void bmo_edgeloop_vert_tag(BMesh *bm, struct BMEdgeLoopStore *el_store, const short oflag, const bool tag)
{
	LinkData *node = BM_edgeloop_verts_get(el_store)->first;
	do {
		BMO_elem_flag_set(bm, (BMVert *)node->data, oflag, tag);
	} while ((node = node->next));
}

static bool bmo_face_is_vert_tag_all(BMesh *bm, BMFace *f, short oflag)
{
	BMLoop *l_iter, *l_first;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		if (!BMO_elem_flag_test(bm, l_iter->v, oflag)) {
			return false;
		}
	} while ((l_iter = l_iter->next) != l_first);
	return true;
}

static bool bm_vert_is_tag_edge_connect(BMesh *bm, BMVert *v)
{
	BMIter eiter;
	BMEdge *e;

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (BMO_elem_flag_test(bm, e, EDGE_RING)) {
			BMVert *v_other = BM_edge_other_vert(e, v);
			if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
				return true;
			}
		}
	}
	return false;
}

/* for now we need full overlap,
 * supporting partial overlap could be done but gets complicated
 * when trimming endpoints is not enough to ensure consistency.
 */
static bool bm_edgeloop_check_overlap_all(
        BMesh *bm,
        struct BMEdgeLoopStore *el_store_a,
        struct BMEdgeLoopStore *el_store_b)
{
	bool has_overlap = true;
	LinkData *node;

	ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
	ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);

	bm_edgeloop_vert_tag(el_store_a, false);
	bm_edgeloop_vert_tag(el_store_b, true);

	for (node = lb_a->first; node; node = node->next) {
		if (bm_vert_is_tag_edge_connect(bm, node->data) == false) {
			has_overlap = false;
			goto finally;
		}
	}

	bm_edgeloop_vert_tag(el_store_a, true);
	bm_edgeloop_vert_tag(el_store_b, false);

	for (node = lb_b->first; node; node = node->next) {
		if (bm_vert_is_tag_edge_connect(bm, node->data) == false) {
			has_overlap = false;
			goto finally;
		}
	}

finally:
	bm_edgeloop_vert_tag(el_store_a, false);
	bm_edgeloop_vert_tag(el_store_b, false);
	return has_overlap;

}

/* -------------------------------------------------------------------- */
/* Edge Loop Pairs */
/* key (ordered loop pointers) */
static GSet *bm_edgering_pair_calc(BMesh *bm, ListBase *eloops_rim)
{
	/**
	 * Method for for finding pairs:
	 *
	 * - first create (vert -> eloop) mapping.
	 * - loop over all eloops.
	 *   - take first vertex of the eloop (any vertex will do)
	 *     - loop over all edges of the vertex.
	 *       - use the edge-verts and (vert -> eloop) map
	 *         to create a pair of eloop pointers, add these to a hash.
	 *
	 * \note, each loop pair will be found twice.
	 * could sort and optimize this but not really so important.
	 */

	GSet *eloop_pair_gs = BLI_gset_pair_new(__func__);
	GHash *vert_eloop_gh = BLI_ghash_ptr_new(__func__);

	struct BMEdgeLoopStore *el_store;

	/* create vert -> eloop map */
	for (el_store = eloops_rim->first; el_store; el_store = BM_EDGELOOP_NEXT(el_store)) {
		LinkData *node = BM_edgeloop_verts_get(el_store)->first;
		do {
			BLI_ghash_insert(vert_eloop_gh, node->data, el_store);
		} while ((node = node->next));
	}


	/* collect eloop pairs */
	for (el_store = eloops_rim->first; el_store; el_store = BM_EDGELOOP_NEXT(el_store)) {
		BMIter eiter;
		BMEdge *e;

		BMVert *v = ((LinkData *)BM_edgeloop_verts_get(el_store)->first)->data;

		BM_ITER_ELEM (e, &eiter, (BMVert *)v, BM_EDGES_OF_VERT) {
			if (BMO_elem_flag_test(bm, e, EDGE_RING)) {
				struct BMEdgeLoopStore *el_store_other;
				BMVert *v_other = BM_edge_other_vert(e, v);
				GHashPair pair_test;

				el_store_other = BLI_ghash_lookup(vert_eloop_gh, v_other);

				/* in rare cases we cant find a match */
				if (el_store_other) {
					pair_test.first = el_store;
					pair_test.second = el_store_other;

					if (pair_test.first > pair_test.second)
						SWAP(const void *, pair_test.first, pair_test.second);

					if (!BLI_gset_haskey(eloop_pair_gs, &pair_test)) {
						GHashPair *pair = BLI_ghashutil_pairalloc(pair_test.first, pair_test.second);
						BLI_gset_insert(eloop_pair_gs, pair);
					}

				}
			}
		}
	}

	BLI_ghash_free(vert_eloop_gh, NULL, NULL);

	if (BLI_gset_size(eloop_pair_gs) == 0) {
		BLI_gset_free(eloop_pair_gs, NULL);
		eloop_pair_gs = NULL;
	}

	return eloop_pair_gs;
}


/* -------------------------------------------------------------------- */
/* Subdivide an edge 'n' times and return an open edgeloop */

static void bm_edge_subdiv_as_loop(BMesh *bm, ListBase *eloops, BMEdge *e, BMVert *v_a, const int cuts)
{
	struct BMEdgeLoopStore *eloop;
	BMVert **v_arr = BLI_array_alloca(v_arr, cuts + 2);
	BMVert *v_b;
	BLI_assert(BM_vert_in_edge(e, v_a));

	v_b = BM_edge_other_vert(e, v_a);

	BM_edge_split_n(bm, e, cuts, &v_arr[1]);
	if (v_a == e->v1) {
		v_arr[0]        = v_a;
		v_arr[cuts + 1] = v_b;
	}
	else {
		v_arr[0]        = v_b;
		v_arr[cuts + 1] = v_a;
	}

	eloop = BM_edgeloop_from_verts(v_arr, cuts + 2, false);

	if (v_a == e->v1) {
		BM_edgeloop_flip(bm, eloop);
	}

	BLI_addtail(eloops, eloop);
}


/* -------------------------------------------------------------------- */
/* LoopPair Cache (struct and util funcs) */


/**
 * Use for finding spline handle direction from surrounding faces.
 *
 * Resulting normal will _always_ point towards 'FACE_SHARED'
 *
 * This function must be called after all loops have been created,
 * but before any mesh modifications.
 *
 * \return true on success
 */
static void bm_vert_calc_surface_tangent(BMesh *bm, BMVert *v, float r_no[3])
{
	BMIter eiter;
	BMEdge *e;

	/* get outer normal, fallback to inner (if this vertex is on a boundary) */
	bool found_outer = false, found_inner = false, found_outer_tag = false;

	float no_outer[3] = {0.0f}, no_inner[3] = {0.0f};

	/* first find rim edges, typically we will only add 2 normals */
	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (UNLIKELY(BM_edge_is_wire(e))) {
			/* pass - this may confuse things */
		}
		else if (BMO_elem_flag_test(bm, e, EDGE_RIM)) {
			BMIter liter;
			BMLoop *l;
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				/* use unmarked (surrounding) faces to create surface tangent */
				float no[3];
				// BM_face_normal_update(l->f);
				BM_edge_calc_face_tangent(e, l, no);
				if (BMO_elem_flag_test(bm, l->f, FACE_SHARED)) {
					add_v3_v3(no_inner, no);
					found_inner = true;
				}
				else {
					add_v3_v3(no_outer, no);
					found_outer = true;

					/* other side is used too, blend midway */
					if (BMO_elem_flag_test(bm, l->f, FACE_OUT)) {
						found_outer_tag = true;
					}
				}
			}
		}
	}

	/* detect if this vertex is in-between 2 loops (when blending multiple),
	 * if so - take both inner and outer into account */

	if (found_inner && found_outer_tag) {
		/* blend between the 2 */
		negate_v3(no_outer);
		normalize_v3(no_outer);
		normalize_v3(no_inner);
		add_v3_v3v3(r_no, no_outer, no_inner);
		normalize_v3(r_no);
	}
	else if (found_outer) {
		negate_v3(no_outer);
		normalize_v3_v3(r_no, no_outer);
	}
	else {
		/* we always have inner geometry */
		BLI_assert(found_inner == true);
		normalize_v3_v3(r_no, no_inner);
	}
}

/**
 * Tag faces connected to an edge loop as FACE_SHARED
 * if all vertices are VERT_SHARED.
 */
static void bm_faces_share_tag_flush(BMesh *bm, BMEdge **e_arr, const unsigned int e_arr_len)
{
	unsigned int i;

	for (i = 0; i < e_arr_len; i++) {
		BMEdge *e = e_arr[i];
		BMLoop *l_iter, *l_first;

		l_iter = l_first = e->l;
		do {
			if (!BMO_elem_flag_test(bm, l_iter->f, FACE_SHARED)) {
				if (bmo_face_is_vert_tag_all(bm, l_iter->f, VERT_SHARED)) {
					BMO_elem_flag_enable(bm, l_iter->f, FACE_SHARED);
				}
			}
		} while ((l_iter = l_iter->radial_next) != l_first);
	}
}

/**
 * Un-Tag faces connected to an edge loop, clearing FACE_SHARED
 */
static void bm_faces_share_tag_clear(BMesh *bm, BMEdge **e_arr_iter, const unsigned int e_arr_len_iter)
{
	unsigned int i;

	for (i = 0; i < e_arr_len_iter; i++) {
		BMEdge *e = e_arr_iter[i];
		BMLoop *l_iter, *l_first;

		l_iter = l_first = e->l;
		do {
			BMO_elem_flag_disable(bm, l_iter->f, FACE_SHARED);
		} while ((l_iter = l_iter->radial_next) != l_first);
	}
}

/**
 * Store data for each loop pair,
 * needed so we don't get feedback loop reading/writing the mesh data.
 *
 * currently only used to store vert-spline-handles,
 * but may be extended for other uses.
 */
typedef struct LoopPairStore {
	/* handle array for splines */
	float (*nors_a)[3];
	float (*nors_b)[3];

	/* since we don't have reliable index values into the array,
	 * store a map (BMVert -> index) */
	GHash *nors_gh_a;
	GHash *nors_gh_b;
} LoopPairStore;

static LoopPairStore *bm_edgering_pair_store_create(
        BMesh *bm,
        struct BMEdgeLoopStore *el_store_a,
        struct BMEdgeLoopStore *el_store_b,
        const int interp_mode)
{
	LoopPairStore *lpair = MEM_mallocN(sizeof(*lpair), __func__);

	if (interp_mode == SUBD_RING_INTERP_SURF) {
		const unsigned int len_a = BM_edgeloop_length_get(el_store_a);
		const unsigned int len_b = BM_edgeloop_length_get(el_store_b);
		const unsigned int e_arr_a_len = len_a - (BM_edgeloop_is_closed(el_store_a) ? 0 : 1);
		const unsigned int e_arr_b_len = len_b - (BM_edgeloop_is_closed(el_store_b) ? 0 : 1);
		BMEdge **e_arr_a = BLI_array_alloca(e_arr_a, e_arr_a_len);
		BMEdge **e_arr_b = BLI_array_alloca(e_arr_b, e_arr_b_len);
		unsigned int i;

		struct BMEdgeLoopStore *el_store_pair[2] = {el_store_a, el_store_b};
		unsigned int side_index;
		float (*nors_pair[2])[3];
		GHash *nors_gh_pair[2];

		BM_edgeloop_edges_get(el_store_a, e_arr_a);
		BM_edgeloop_edges_get(el_store_b, e_arr_b);

		lpair->nors_a = MEM_mallocN(sizeof(*lpair->nors_a) * len_a, __func__);
		lpair->nors_b = MEM_mallocN(sizeof(*lpair->nors_b) * len_b, __func__);

		nors_pair[0] = lpair->nors_a;
		nors_pair[1] = lpair->nors_b;

		lpair->nors_gh_a = BLI_ghash_ptr_new(__func__);
		lpair->nors_gh_b = BLI_ghash_ptr_new(__func__);

		nors_gh_pair[0] = lpair->nors_gh_a;
		nors_gh_pair[1] = lpair->nors_gh_b;

		/* now calculate nor */

		/* all other verts must _not_ be tagged */
		bmo_edgeloop_vert_tag(bm, el_store_a, VERT_SHARED, true);
		bmo_edgeloop_vert_tag(bm, el_store_b, VERT_SHARED, true);

		/* tag all faces that are in-between both loops */
		bm_faces_share_tag_flush(bm, e_arr_a, e_arr_a_len);
		bm_faces_share_tag_flush(bm, e_arr_b, e_arr_b_len);

		/* now we have all data we need, calculate vertex spline nor! */
		for (side_index = 0; side_index < 2; side_index++) {
			/* iter vars */
			struct BMEdgeLoopStore *el_store = el_store_pair[side_index];
			ListBase *lb = BM_edgeloop_verts_get(el_store);
			GHash *nors_gh_iter = nors_gh_pair[side_index];
			float (*nor)[3] = nors_pair[side_index];

			LinkData *v_iter;

			for (v_iter = lb->first, i = 0; v_iter; v_iter = v_iter->next, i++) {
				BMVert *v = v_iter->data;
				bm_vert_calc_surface_tangent(bm, v, nor[i]);
				BLI_ghash_insert(nors_gh_iter, v, SET_UINT_IN_POINTER(i));
			}
		}

		/* cleanup verts share */
		bmo_edgeloop_vert_tag(bm, el_store_a, VERT_SHARED, false);
		bmo_edgeloop_vert_tag(bm, el_store_b, VERT_SHARED, false);

		/* cleanup faces share */
		bm_faces_share_tag_clear(bm, e_arr_a, e_arr_a_len);
		bm_faces_share_tag_clear(bm, e_arr_b, e_arr_b_len);
	}
	return lpair;
}

static void bm_edgering_pair_store_free(
        LoopPairStore *lpair,
        const int interp_mode)
{
	if (interp_mode == SUBD_RING_INTERP_SURF) {
		MEM_freeN(lpair->nors_a);
		MEM_freeN(lpair->nors_b);

		BLI_ghash_free(lpair->nors_gh_a, NULL, NULL);
		BLI_ghash_free(lpair->nors_gh_b, NULL, NULL);
	}
	MEM_freeN(lpair);
}


/* -------------------------------------------------------------------- */
/* Interpolation Function */

static void bm_edgering_pair_interpolate(BMesh *bm, LoopPairStore *lpair,
                                         struct BMEdgeLoopStore *el_store_a,
                                         struct BMEdgeLoopStore *el_store_b,
                                         ListBase *eloops_ring,
                                         const int interp_mode, const int cuts, const float smooth,
                                         const float *falloff_cache)
{
	const int resolu = cuts + 2;
	const int dims = 3;
	bool is_a_no_valid, is_b_no_valid;
	int i;

	float el_store_a_co[3], el_store_b_co[3];
	float el_store_a_no[3], el_store_b_no[3];

	struct BMEdgeLoopStore *el_store_ring;

	float (*coord_array_main)[3] = NULL;

	BM_edgeloop_calc_center(bm, el_store_a);
	BM_edgeloop_calc_center(bm, el_store_b);

	is_a_no_valid = BM_edgeloop_calc_normal(bm, el_store_a);
	is_b_no_valid = BM_edgeloop_calc_normal(bm, el_store_b);

	copy_v3_v3(el_store_a_co, BM_edgeloop_center_get(el_store_a));
	copy_v3_v3(el_store_b_co, BM_edgeloop_center_get(el_store_b));

	/* correct normals need to be flipped to face each other
	 * we know both normals point in the same direction so one will need flipping */
	{
		float el_dir[3];
		float no[3];
		sub_v3_v3v3(el_dir, el_store_a_co, el_store_b_co);
		normalize_v3_v3(no, el_dir);

		if (is_a_no_valid == false) {
			is_a_no_valid = BM_edgeloop_calc_normal_aligned(bm, el_store_a, no);
		}
		if (is_b_no_valid == false) {
			is_b_no_valid = BM_edgeloop_calc_normal_aligned(bm, el_store_b, no);
		}
		(void)is_a_no_valid, (void)is_b_no_valid;

		copy_v3_v3(el_store_a_no, BM_edgeloop_normal_get(el_store_a));
		copy_v3_v3(el_store_b_no, BM_edgeloop_normal_get(el_store_b));

		if (dot_v3v3(el_store_a_no, el_dir) > 0.0f) {
			negate_v3(el_store_a_no);
		}
		if (dot_v3v3(el_store_b_no, el_dir) < 0.0f) {
			negate_v3(el_store_b_no);
		}

	}
	/* now normals are correct, don't touch! */


	/* calculate the center spline, multiple  */
	if ((interp_mode == SUBD_RING_INTERP_PATH) || falloff_cache) {
		float handle_a[3], handle_b[3];
		float handle_len;

		handle_len = bezier_handle_calc_length_v3(el_store_a_co, el_store_a_no,
		                                          el_store_b_co, el_store_b_no) * smooth;

		mul_v3_v3fl(handle_a, el_store_a_no, handle_len);
		mul_v3_v3fl(handle_b, el_store_b_no, handle_len);

		add_v3_v3(handle_a, el_store_a_co);
		add_v3_v3(handle_b, el_store_b_co);

		coord_array_main = MEM_mallocN(dims * (resolu) * sizeof(float), __func__);

		for (i = 0; i < dims; i++) {
			BKE_curve_forward_diff_bezier(el_store_a_co[i], handle_a[i], handle_b[i], el_store_b_co[i],
			                              ((float *)coord_array_main) + i, resolu - 1, sizeof(float) * dims);
		}
	}

	switch (interp_mode) {
		case SUBD_RING_INTERP_LINEAR:
		{
			if (falloff_cache) {
				float (*coord_array)[3] = MEM_mallocN(dims * (resolu) * sizeof(float), __func__);
				for (i = 0; i < resolu; i++) {
					interp_v3_v3v3(coord_array[i], el_store_a_co, el_store_b_co, (float)i / (float)(resolu - 1));
				}

				for (el_store_ring = eloops_ring->first;
				     el_store_ring;
				     el_store_ring = BM_EDGELOOP_NEXT(el_store_ring))
				{
					ListBase *lb_ring = BM_edgeloop_verts_get(el_store_ring);
					LinkData *v_iter;

					for (v_iter = lb_ring->first, i = 0; v_iter; v_iter = v_iter->next, i++) {
						if (i > 0 && i < resolu - 1) {
							/* shape */
							if (falloff_cache) {
								interp_v3_v3v3(((BMVert *)v_iter->data)->co,
								               coord_array[i], ((BMVert *)v_iter->data)->co, falloff_cache[i]);
							}
						}
					}
				}

				MEM_freeN(coord_array);

			}

			break;
		}
		case SUBD_RING_INTERP_PATH:
		{
			float (*direction_array)[3] = MEM_mallocN(dims * (resolu) * sizeof(float), __func__);
			float (*quat_array)[4] = MEM_mallocN(resolu * sizeof(*quat_array), __func__);
			float (*tri_array)[3][3] = MEM_mallocN(resolu * sizeof(*tri_array), __func__);
			float (*tri_sta)[3], (*tri_end)[3], (*tri_tmp)[3];

			/* very similar to make_bevel_list_3D_minimum_twist */

			/* calculate normals */
			copy_v3_v3(direction_array[0],            el_store_a_no);
			negate_v3_v3(direction_array[resolu - 1], el_store_b_no);
			for (i = 1; i < resolu - 1; i++) {
				bisect_v3_v3v3v3(direction_array[i],
				                 coord_array_main[i - 1], coord_array_main[i], coord_array_main[i + 1]);
			}

			vec_to_quat(quat_array[0], direction_array[0], 5, 1);
			normalize_qt(quat_array[0]);

			for (i = 1; i < resolu; i++) {
				float angle = angle_normalized_v3v3(direction_array[i - 1], direction_array[i]);
				// BLI_assert(angle < DEG2RADF(90.0f));
				if (angle > 0.0f) { /* otherwise we can keep as is */
					float cross_tmp[3];
					float q[4];
					cross_v3_v3v3(cross_tmp, direction_array[i - 1], direction_array[i]);
					axis_angle_to_quat(q, cross_tmp, angle);
					mul_qt_qtqt(quat_array[i], q, quat_array[i - 1]);
					normalize_qt(quat_array[i]);
				}
				else {
					copy_qt_qt(quat_array[i], quat_array[i - 1]);
				}
			}

			/* init base tri */
			for (i = 0; i < resolu; i++) {
				int j;

				const float shape_size = falloff_cache ? falloff_cache[i] : 1.0f;

				tri_tmp = tri_array[i];

				/* create the triangle and transform */
				for (j = 0; j < 3; j++) {
					zero_v3(tri_tmp[j]);
					if      (j == 1) tri_tmp[j][0] = shape_size;
					else if (j == 2) tri_tmp[j][1] = shape_size;
					mul_qt_v3(quat_array[i], tri_tmp[j]);
					add_v3_v3(tri_tmp[j], coord_array_main[i]);
				}
			}

			tri_sta = tri_array[0];
			tri_end = tri_array[resolu - 1];

			for (el_store_ring = eloops_ring->first;
			     el_store_ring;
			     el_store_ring = BM_EDGELOOP_NEXT(el_store_ring))
			{
				ListBase *lb_ring = BM_edgeloop_verts_get(el_store_ring);
				LinkData *v_iter;

				BMVert *v_a = ((LinkData *)lb_ring->first)->data;
				BMVert *v_b = ((LinkData *)lb_ring->last)->data;

				/* skip first and last */
				for (v_iter = ((LinkData *)lb_ring->first)->next, i = 1;
				     v_iter != lb_ring->last;
				     v_iter = v_iter->next, i++)
				{
					float co_a[3], co_b[3];

					tri_tmp = tri_array[i];

					barycentric_transform(co_a, v_a->co, UNPACK3(tri_tmp), UNPACK3(tri_sta));
					barycentric_transform(co_b, v_b->co, UNPACK3(tri_tmp), UNPACK3(tri_end));

					interp_v3_v3v3(((BMVert *)v_iter->data)->co, co_a, co_b, (float)i / (float)(resolu - 1));
				}
			}

			MEM_freeN(direction_array);
			MEM_freeN(quat_array);
			MEM_freeN(tri_array);
			break;
		}
		case SUBD_RING_INTERP_SURF:
		{
			float (*coord_array)[3] = MEM_mallocN(dims * (resolu) * sizeof(float), __func__);

			/* calculate a bezier handle per edge ring */
			for (el_store_ring = eloops_ring->first;
			     el_store_ring;
			     el_store_ring = BM_EDGELOOP_NEXT(el_store_ring))
			{
				ListBase *lb_ring = BM_edgeloop_verts_get(el_store_ring);
				LinkData *v_iter;

				BMVert *v_a = ((LinkData *)lb_ring->first)->data;
				BMVert *v_b = ((LinkData *)lb_ring->last)->data;

				float co_a[3], no_a[3], handle_a[3], co_b[3], no_b[3], handle_b[3];
				float handle_len;

				copy_v3_v3(co_a, v_a->co);
				copy_v3_v3(co_b, v_b->co);

				/* don't calculate normals here else we get into feedback loop
				 * when subdividing 2+ connected edge rings */
#if 0
				bm_vert_calc_surface_tangent(bm, v_a, no_a);
				bm_vert_calc_surface_tangent(bm, v_b, no_b);
#else
				{
					const unsigned int index_a = GET_UINT_FROM_POINTER(BLI_ghash_lookup(lpair->nors_gh_a, v_a));
					const unsigned int index_b = GET_UINT_FROM_POINTER(BLI_ghash_lookup(lpair->nors_gh_b, v_b));

					BLI_assert(BLI_ghash_haskey(lpair->nors_gh_a, v_a));
					BLI_assert(BLI_ghash_haskey(lpair->nors_gh_b, v_b));

					copy_v3_v3(no_a, lpair->nors_a[index_a]);
					copy_v3_v3(no_b, lpair->nors_b[index_b]);
				}
#endif
				handle_len = bezier_handle_calc_length_v3(co_a, no_a, co_b, no_b) * smooth;

				mul_v3_v3fl(handle_a, no_a, handle_len);
				mul_v3_v3fl(handle_b, no_b, handle_len);

				add_v3_v3(handle_a, co_a);
				add_v3_v3(handle_b, co_b);

				for (i = 0; i < dims; i++) {
					BKE_curve_forward_diff_bezier(co_a[i], handle_a[i], handle_b[i], co_b[i],
					                              ((float *)coord_array) + i, resolu - 1, sizeof(float) * dims);
				}

				/* skip first and last */
				for (v_iter = ((LinkData *)lb_ring->first)->next, i = 1;
				     v_iter != lb_ring->last;
				     v_iter = v_iter->next, i++)
				{
					if (i > 0 && i < resolu - 1) {
						copy_v3_v3(((BMVert *)v_iter->data)->co, coord_array[i]);

						/* shape */
						if (falloff_cache) {
							interp_v3_v3v3(((BMVert *)v_iter->data)->co,
							               coord_array_main[i], ((BMVert *)v_iter->data)->co, falloff_cache[i]);
						}
					}
				}
			}

			MEM_freeN(coord_array);

			break;
		}
	}

	if (coord_array_main) {
		MEM_freeN(coord_array_main);
	}
}

/**
 * Cuts up an ngon into many slices.
 */
static void bm_face_slice(BMesh *bm, BMLoop *l, const int cuts)
{
	/* TODO, interpolate edge data */
	BMLoop *l_new = l;
	int i;

	for (i = 0; i < cuts; i++) {
		/* no chance of double */
		BM_face_split(bm, l_new->f, l_new->prev, l_new->next->next, &l_new, NULL, false);
		if (l_new->f->len < l_new->radial_next->f->len) {
			l_new = l_new->radial_next;
		}
		BMO_elem_flag_enable(bm, l_new->f, FACE_OUT);
		BMO_elem_flag_enable(bm, l_new->radial_next->f, FACE_OUT);
	}
}

static bool bm_edgering_pair_order_is_flipped(BMesh *UNUSED(bm),
                                              struct BMEdgeLoopStore *el_store_a,
                                              struct BMEdgeLoopStore *el_store_b )
{
	ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
	ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);

	LinkData *v_iter_a_first = lb_a->first;
	LinkData *v_iter_b_first = lb_b->first;

	LinkData *v_iter_a_step = v_iter_a_first;
	LinkData *v_iter_b_step = v_iter_b_first;

	/* we _must_ have same starting edge shared */
	BLI_assert(BM_edge_exists(v_iter_a_first->data, v_iter_b_first->data));

	/* step around any fan-faces on both sides */
	do {
		v_iter_a_step = v_iter_a_step->next;
	} while (v_iter_a_step &&
	         ((BM_edge_exists(v_iter_a_step->data, v_iter_b_first->data)) ||
	          (BM_edge_exists(v_iter_a_step->data, v_iter_b_first->next->data))));
	do {
		v_iter_b_step = v_iter_b_step->next;
	} while (v_iter_b_step &&
	         ((BM_edge_exists(v_iter_b_step->data, v_iter_a_first->data)) ||
	          (BM_edge_exists(v_iter_b_step->data, v_iter_a_first->next->data))));

	v_iter_a_step = v_iter_a_step ? v_iter_a_step->prev : lb_a->last;
	v_iter_b_step = v_iter_b_step ? v_iter_b_step->prev : lb_b->last;

	return !(BM_edge_exists(v_iter_a_step->data, v_iter_b_step->data) ||
	         BM_edge_exists(v_iter_a_first->next->data, v_iter_b_step->data) ||
	         BM_edge_exists(v_iter_b_first->next->data, v_iter_a_step->data));
}

/**
 * Takes 2 edge loops that share edges,
 * sort their verts and rotates the list so the lined up.
 */
static void bm_edgering_pair_order(BMesh *bm,
                                   struct BMEdgeLoopStore *el_store_a,
                                   struct BMEdgeLoopStore *el_store_b)
{
	ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
	ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);

	LinkData *node;

	bm_edgeloop_vert_tag(el_store_a, false);
	bm_edgeloop_vert_tag(el_store_b, true);

	/* before going much further, get ourselves in order
	 * - align loops (not strictly necessary but handy)
	 * - ensure winding is set for both loops */
	if (BM_edgeloop_is_closed(el_store_a) && BM_edgeloop_is_closed(el_store_b)) {
		BMIter eiter;
		BMEdge *e;
		BMVert *v_other;

		node = lb_a->first;

		BM_ITER_ELEM (e, &eiter, (BMVert *)node->data, BM_EDGES_OF_VERT) {
			if (BMO_elem_flag_test(bm, e, EDGE_RING)) {
				v_other = BM_edge_other_vert(e, (BMVert *)node->data);
				if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
					break;
				}
				else {
					v_other = NULL;
				}
			}
		}
		BLI_assert(v_other != NULL);

		for (node = lb_b->first; node; node = node->next) {
			if (node->data == v_other) {
				break;
			}
		}
		BLI_assert(node != NULL);

		BLI_listbase_rotate_first(lb_b, node);

		/* now check we are winding the same way */
		if (bm_edgering_pair_order_is_flipped(bm, el_store_a, el_store_b)) {
			BM_edgeloop_flip(bm, el_store_b);
			/* re-ensure the first node */
			BLI_listbase_rotate_first(lb_b, node);
		}

		/* sanity checks that we are aligned & winding now */
		BLI_assert(bm_edgering_pair_order_is_flipped(bm, el_store_a, el_store_b) == false);
	}
	else {
		/* if we dont share and edge - flip */
		BMEdge *e = BM_edge_exists(((LinkData *)lb_a->first)->data,
		                           ((LinkData *)lb_b->first)->data);
		if (e == NULL || !BMO_elem_flag_test(bm, e, EDGE_RING)) {
			BM_edgeloop_flip(bm, el_store_b);
		}
	}

	/* for cases with multiple loops */
	bm_edgeloop_vert_tag(el_store_b, false);
}


/**
 * Take 2 edge loops, do a subdivision on connecting edges.
 *
 * \note loops are _not_ aligned.
 */
static void bm_edgering_pair_subdiv(BMesh *bm,
                                    struct BMEdgeLoopStore *el_store_a,
                                    struct BMEdgeLoopStore *el_store_b,
                                    ListBase *eloops_ring,
                                    const int cuts)
{
	ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
	// ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);
	const int stack_max = max_ii(BM_edgeloop_length_get(el_store_a),
	                             BM_edgeloop_length_get(el_store_b)) * 2;
	BMEdge **edges_ring_arr = BLI_array_alloca(edges_ring_arr, stack_max);
	BMFace **faces_ring_arr = BLI_array_alloca(faces_ring_arr, stack_max);
	STACK_DECLARE(edges_ring_arr);
	STACK_DECLARE(faces_ring_arr);
	struct BMEdgeLoopStore *el_store_ring;
	LinkData *node;
	BMEdge *e;
	BMFace *f;

	STACK_INIT(edges_ring_arr, stack_max);
	STACK_INIT(faces_ring_arr, stack_max);

	bm_edgeloop_vert_tag(el_store_a, false);
	bm_edgeloop_vert_tag(el_store_b, true);

	for (node = lb_a->first; node; node = node->next) {
		BMIter eiter;

		BM_ITER_ELEM (e, &eiter, (BMVert *)node->data, BM_EDGES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, e, EDGE_IN_STACK)) {
				BMVert *v_other = BM_edge_other_vert(e, (BMVert *)node->data);
				if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
					BMIter fiter;

					BMO_elem_flag_enable(bm, e, EDGE_IN_STACK);
					STACK_PUSH(edges_ring_arr, e);

					/* add faces to the stack */
					BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
						if (BMO_elem_flag_test(bm, f, FACE_OUT)) {
							if (!BMO_elem_flag_test(bm, f, FACE_IN_STACK)) {
								BMO_elem_flag_enable(bm, f, FACE_IN_STACK);
								STACK_PUSH(faces_ring_arr, f);
							}
						}
					}
				}
			}
		}
	}

	while ((e = STACK_POP(edges_ring_arr))) {
		/* found opposite edge */
		BMVert *v_other;

		BMO_elem_flag_disable(bm, e, EDGE_IN_STACK);

		/* unrelated to subdiv, but if we _don't_ clear flag, multiple rings fail */
		BMO_elem_flag_disable(bm, e, EDGE_RING);

		v_other = BM_elem_flag_test(e->v1, BM_ELEM_TAG) ? e->v1 : e->v2;
		bm_edge_subdiv_as_loop(bm, eloops_ring, e, v_other, cuts);
	}

	while ((f = STACK_POP(faces_ring_arr))) {
		BMLoop *l_iter, *l_first;

		BMO_elem_flag_disable(bm, f, FACE_IN_STACK);

		/* Check each edge of the face */
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (BMO_elem_flag_test(bm, l_iter->e, EDGE_RIM)) {
				bm_face_slice(bm, l_iter, cuts);
				break;
			}
		} while ((l_iter = l_iter->next) != l_first);
	}


	/* clear tags so subdiv verts don't get tagged too  */
	for (el_store_ring = eloops_ring->first;
	     el_store_ring;
	     el_store_ring = BM_EDGELOOP_NEXT(el_store_ring))
	{
		bm_edgeloop_vert_tag(el_store_ring, false);
	}

	/* cleanup after */
	bm_edgeloop_vert_tag(el_store_b, false);
}

static void bm_edgering_pair_ringsubd(BMesh *bm, LoopPairStore *lpair,
                                      struct BMEdgeLoopStore *el_store_a,
                                      struct BMEdgeLoopStore *el_store_b,
                                      const int interp_mode, const int cuts, const float smooth,
                                      const float *falloff_cache)
{
	ListBase eloops_ring = {NULL};
	bm_edgering_pair_order(bm, el_store_a, el_store_b);
	bm_edgering_pair_subdiv(bm, el_store_a, el_store_b, &eloops_ring, cuts);
	bm_edgering_pair_interpolate(bm, lpair, el_store_a, el_store_b, &eloops_ring,
	                             interp_mode, cuts, smooth, falloff_cache);
	BM_mesh_edgeloops_free(&eloops_ring);
}

static bool bm_edge_rim_test_cb(BMEdge *e, void *bm_v)
{
	BMesh *bm = bm_v;
	return BMO_elem_flag_test_bool(bm, e, EDGE_RIM);
}


/* keep this operator fast, its used in a modifier */
void bmo_subdivide_edgering_exec(BMesh *bm, BMOperator *op)
{
	ListBase eloops_rim = {NULL};
	BMOIter siter;
	BMEdge *e;
	int count;
	bool changed = false;

	const int cuts = BMO_slot_int_get(op->slots_in, "cuts");
	const int interp_mode = BMO_slot_int_get(op->slots_in, "interp_mode");
	const float smooth = BMO_slot_float_get(op->slots_in, "smooth");
	const int resolu = cuts + 2;

	/* optional 'shape' */
	const int profile_shape = BMO_slot_int_get(op->slots_in, "profile_shape");
	const float profile_shape_factor = BMO_slot_float_get(op->slots_in, "profile_shape_factor");
	float *falloff_cache = (profile_shape_factor != 0.0f) ? BLI_array_alloca(falloff_cache, cuts + 2) : NULL;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_RING);

	BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

	/* -------------------------------------------------------------------- */
	/* flag outer edges (loops defined as edges on the bounds of the edge ring) */

	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		BMIter fiter;
		BMFace *f;

		BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
			if (!BMO_elem_flag_test(bm, f, FACE_OUT)) {
				BMIter liter;
				BMLoop *l;
				bool ok = false;

				/* check at least 2 edges in the face are rings */
				BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
					if (BMO_elem_flag_test(bm, l->e, EDGE_RING) && e != l->e) {
						ok = true;
						break;
					}
				}

				if (ok) {
					BMO_elem_flag_enable(bm, f, FACE_OUT);

					BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
						if (!BMO_elem_flag_test(bm, l->e, EDGE_RING)) {
							BMO_elem_flag_enable(bm, l->e, EDGE_RIM);
						}
					}
				}
			}
		}
	}


	/* -------------------------------------------------------------------- */
	/* Cache falloff for each step (symmetrical) */

	if (falloff_cache) {
		int i;
		for (i = 0; i < resolu; i++) {
			float shape_size = 1.0f;
			float fac = (float)i / (float)(resolu - 1);
			fac = fabsf(1.0f - 2.0f * fabsf(0.5f - fac));
			fac = bmesh_subd_falloff_calc(profile_shape, fac);
			shape_size += fac * profile_shape_factor;

			falloff_cache[i] = shape_size;
		}
	}


	/* -------------------------------------------------------------------- */
	/* Execute subdivision on all ring pairs */

	count = BM_mesh_edgeloops_find(bm, &eloops_rim, bm_edge_rim_test_cb, (void *)bm);

	if (count < 2) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "No edge rings found");
		goto cleanup;
	}
	else if (count == 2) {
		/* this case could be removed,
		 * but simple to avoid 'bm_edgering_pair_calc' in this case since theres only one. */
		struct BMEdgeLoopStore *el_store_a = eloops_rim.first;
		struct BMEdgeLoopStore *el_store_b = eloops_rim.last;
		LoopPairStore *lpair;

		if (bm_edgeloop_check_overlap_all(bm, el_store_a, el_store_b)) {
			lpair = bm_edgering_pair_store_create(bm, el_store_a, el_store_b, interp_mode);
		}
		else {
			lpair = NULL;
		}

		if (lpair) {
			bm_edgering_pair_ringsubd(bm, lpair, el_store_a, el_store_b,
			                          interp_mode, cuts, smooth, falloff_cache);
			bm_edgering_pair_store_free(lpair, interp_mode);
			changed = true;
		}
		else {
			BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
			                "Edge-ring pair isn't connected");
			goto cleanup;
		}
	}
	else {
		GSetIterator gs_iter;
		int i;

		GSet *eloop_pairs_gs = bm_edgering_pair_calc(bm, &eloops_rim);
		LoopPairStore **lpair_arr;

		if (eloop_pairs_gs == NULL) {
			BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
			                "Edge-rings are not connected");
			goto cleanup;
		}

		lpair_arr = BLI_array_alloca(lpair_arr, BLI_gset_size(eloop_pairs_gs));

		/* first cache pairs */
		GSET_ITER_INDEX (gs_iter, eloop_pairs_gs, i) {
			GHashPair *eloop_pair = BLI_gsetIterator_getKey(&gs_iter);
			struct BMEdgeLoopStore *el_store_a = (void *)eloop_pair->first;
			struct BMEdgeLoopStore *el_store_b = (void *)eloop_pair->second;
			LoopPairStore *lpair;

			if (bm_edgeloop_check_overlap_all(bm, el_store_a, el_store_b)) {
				lpair = bm_edgering_pair_store_create(bm, el_store_a, el_store_b, interp_mode);
			}
			else {
				lpair = NULL;
			}
			lpair_arr[i] = lpair;

			BLI_assert(bm_verts_tag_count(bm) == 0);
		}

		GSET_ITER_INDEX (gs_iter, eloop_pairs_gs, i) {
			GHashPair *eloop_pair = BLI_gsetIterator_getKey(&gs_iter);
			struct BMEdgeLoopStore *el_store_a = (void *)eloop_pair->first;
			struct BMEdgeLoopStore *el_store_b = (void *)eloop_pair->second;
			LoopPairStore *lpair = lpair_arr[i];

			if (lpair) {
				bm_edgering_pair_ringsubd(bm, lpair, el_store_a, el_store_b,
				                          interp_mode, cuts, smooth, falloff_cache);
				bm_edgering_pair_store_free(lpair, interp_mode);
				changed = true;
			}

			BLI_assert(bm_verts_tag_count(bm) == 0);
		}
		BLI_gset_free(eloop_pairs_gs, MEM_freeN);
	}

cleanup:
	BM_mesh_edgeloops_free(&eloops_rim);

	/* flag output */
	if (changed) {
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
	}
}
