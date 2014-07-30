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
 * The Original Code is Copyright (C) 2010 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/editmesh_bvh.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdopbvh.h"

#include "BKE_editmesh.h"

#include "BKE_editmesh_bvh.h"  /* own include */


struct BMBVHTree {
	BVHTree *tree;

	BMLoop *(*looptris)[3];
	int looptris_tot;

	BMesh *bm;

	const float (*cos_cage)[3];
	bool cos_cage_free;

	int flag;
};

BMBVHTree *BKE_bmbvh_new_from_editmesh(
        BMEditMesh *em, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free)
{
	return BKE_bmbvh_new(em->bm, em->looptris, em->tottri, flag, cos_cage, cos_cage_free);
}

BMBVHTree *BKE_bmbvh_new_ex(
        BMesh *bm, BMLoop *(*looptris)[3], int looptris_tot, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free,
        bool (*test_fn)(BMFace *, void *user_data), void *user_data)
{
	/* could become argument */
	const float epsilon = FLT_EPSILON * 2.0f;

	BMBVHTree *bmtree = MEM_callocN(sizeof(*bmtree), "BMBVHTree");
	float cos[3][3];
	int i;
	int tottri;

	/* avoid testing every tri */
	BMFace *f_test, *f_test_prev;
	bool test_fn_ret;

	/* BKE_editmesh_tessface_calc() must be called already */
	BLI_assert(looptris_tot != 0 || bm->totface == 0);

	if (cos_cage) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	bmtree->looptris = looptris;
	bmtree->looptris_tot = looptris_tot;
	bmtree->bm = bm;
	bmtree->cos_cage = cos_cage;
	bmtree->cos_cage_free = cos_cage_free;
	bmtree->flag = flag;

	if (test_fn) {
		/* callback must do... */
		BLI_assert(!(flag & (BMBVH_RESPECT_SELECT | BMBVH_RESPECT_HIDDEN)));

		f_test_prev = NULL;
		test_fn_ret = false;

		tottri = 0;
		for (i = 0; i < looptris_tot; i++) {
			f_test = looptris[i][0]->f;
			if (f_test != f_test_prev) {
				test_fn_ret = test_fn(f_test, user_data);
				f_test_prev = f_test;
			}

			if (test_fn_ret) {
				tottri++;
			}
		}
	}
	else {
		tottri = looptris_tot;
	}

	bmtree->tree = BLI_bvhtree_new(tottri, epsilon, 8, 8);

	f_test_prev = NULL;
	test_fn_ret = false;

	for (i = 0; i < looptris_tot; i++) {
		if (test_fn) {
			/* note, the arrays wont align now! take care */
			f_test = looptris[i][0]->f;
			if (f_test != f_test_prev) {
				test_fn_ret = test_fn(f_test, user_data);
				f_test_prev = f_test;
			}

			if (!test_fn_ret) {
				continue;
			}
		}

		if (cos_cage) {
			copy_v3_v3(cos[0], cos_cage[BM_elem_index_get(looptris[i][0]->v)]);
			copy_v3_v3(cos[1], cos_cage[BM_elem_index_get(looptris[i][1]->v)]);
			copy_v3_v3(cos[2], cos_cage[BM_elem_index_get(looptris[i][2]->v)]);
		}
		else {
			copy_v3_v3(cos[0], looptris[i][0]->v->co);
			copy_v3_v3(cos[1], looptris[i][1]->v->co);
			copy_v3_v3(cos[2], looptris[i][2]->v->co);
		}

		BLI_bvhtree_insert(bmtree->tree, i, (float *)cos, 3);
	}
	
	BLI_bvhtree_balance(bmtree->tree);
	
	return bmtree;
}

static bool bm_face_is_select(BMFace *f, void *UNUSED(user_data))
{
	return (BM_elem_flag_test(f, BM_ELEM_SELECT) != 0);
}

static bool bm_face_is_not_hidden(BMFace *f, void *UNUSED(user_data))
{
	return (BM_elem_flag_test(f, BM_ELEM_HIDDEN) == 0);
}

BMBVHTree *BKE_bmbvh_new(
        BMesh *bm, BMLoop *(*looptris)[3], int looptris_tot, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free)
{
	bool (*test_fn)(BMFace *, void *user_data);

	if (flag & BMBVH_RESPECT_SELECT) {
		test_fn = bm_face_is_select;
	}
	else if (flag & BMBVH_RESPECT_HIDDEN) {
		test_fn = bm_face_is_not_hidden;
	}
	else {
		test_fn = NULL;
	}

	flag &= ~(BMBVH_RESPECT_SELECT | BMBVH_RESPECT_HIDDEN);

	return BKE_bmbvh_new_ex(bm, looptris, looptris_tot, flag, cos_cage, cos_cage_free, test_fn, NULL);
}


void BKE_bmbvh_free(BMBVHTree *bmtree)
{
	BLI_bvhtree_free(bmtree->tree);
	
	if (bmtree->cos_cage && bmtree->cos_cage_free) {
		MEM_freeN((void *)bmtree->cos_cage);
	}
	
	MEM_freeN(bmtree);
}

BVHTree *BKE_bmbvh_tree_get(BMBVHTree *bmtree)
{
	return bmtree->tree;
}



/* -------------------------------------------------------------------- */
/* Utility BMesh cast/intersect functions */

/**
 * Return the coords from a triangle.
 */
static void bmbvh_tri_from_face(const float *cos[3],
                                const BMLoop **ltri, const float (*cos_cage)[3])
{
	if (cos_cage == NULL) {
		cos[0] = ltri[0]->v->co;
		cos[1] = ltri[1]->v->co;
		cos[2] = ltri[2]->v->co;
	}
	else {
		cos[0] = cos_cage[BM_elem_index_get(ltri[0]->v)];
		cos[1] = cos_cage[BM_elem_index_get(ltri[1]->v)];
		cos[2] = cos_cage[BM_elem_index_get(ltri[2]->v)];
	}
}


/* taken from bvhutils.c */

/* -------------------------------------------------------------------- */
/* BKE_bmbvh_ray_cast */

struct RayCastUserData {
	/* from the bmtree */
	const BMLoop *(*looptris)[3];
	const float (*cos_cage)[3];

	/* from the hit */
	float uv[2];
};

static void bmbvh_ray_cast_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	struct RayCastUserData *bmcb_data = userdata;
	const BMLoop **ltri = bmcb_data->looptris[index];
	float dist, uv[2];
	const float *tri_cos[3];
	bool isect;

	bmbvh_tri_from_face(tri_cos, ltri, bmcb_data->cos_cage);

	isect = (ray->radius > 0.0f ?
	         isect_ray_tri_epsilon_v3(ray->origin, ray->direction,
	                                  tri_cos[0], tri_cos[1], tri_cos[2], &dist, uv, ray->radius) :
	         isect_ray_tri_v3(ray->origin, ray->direction,
	                          tri_cos[0], tri_cos[1], tri_cos[2], &dist, uv));

	if (isect && dist < hit->dist) {
		hit->dist = dist;
		hit->index = index;

		copy_v3_v3(hit->no, ltri[0]->f->no);

		madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

		copy_v2_v2(bmcb_data->uv, uv);
	}
}

BMFace *BKE_bmbvh_ray_cast(BMBVHTree *bmtree, const float co[3], const float dir[3], const float radius,
                           float *r_dist, float r_hitout[3], float r_cagehit[3])
{
	BVHTreeRayHit hit;
	struct RayCastUserData bmcb_data;
	const float dist = r_dist ? *r_dist : FLT_MAX;

	if (bmtree->cos_cage) BLI_assert(!(bmtree->bm->elem_index_dirty & BM_VERT));

	hit.dist = dist;
	hit.index = -1;

	/* ok to leave 'uv' uninitialized */
	bmcb_data.looptris = (const BMLoop *(*)[3])bmtree->looptris;
	bmcb_data.cos_cage = (const float (*)[3])bmtree->cos_cage;
	
	BLI_bvhtree_ray_cast(bmtree->tree, co, dir, radius, &hit, bmbvh_ray_cast_cb, &bmcb_data);
	if (hit.index != -1 && hit.dist != dist) {
		if (r_hitout) {
			if (bmtree->flag & BMBVH_RETURN_ORIG) {
				BMLoop **ltri = bmtree->looptris[hit.index];
				interp_v3_v3v3v3_uv(r_hitout, ltri[0]->v->co, ltri[1]->v->co, ltri[2]->v->co, bmcb_data.uv);
			}
			else {
				copy_v3_v3(r_hitout, hit.co);
			}

			if (r_cagehit) {
				copy_v3_v3(r_cagehit, hit.co);
			}
		}

		if (r_dist) {
			*r_dist = hit.dist;
		}

		return bmtree->looptris[hit.index][0]->f;
	}

	return NULL;
}


/* -------------------------------------------------------------------- */
/* BKE_bmbvh_find_face_segment */

struct SegmentUserData {
	/* from the bmtree */
	const BMLoop *(*looptris)[3];
	const float (*cos_cage)[3];

	/* from the hit */
	float uv[2];
	const float *co_a, *co_b;
};

static void bmbvh_find_face_segment_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	struct SegmentUserData *bmcb_data = userdata;
	const BMLoop **ltri = bmcb_data->looptris[index];
	float dist, uv[2];
	const float *tri_cos[3];

	bmbvh_tri_from_face(tri_cos, ltri, bmcb_data->cos_cage);

	if (equals_v3v3(bmcb_data->co_a, tri_cos[0]) ||
	    equals_v3v3(bmcb_data->co_a, tri_cos[1]) ||
	    equals_v3v3(bmcb_data->co_a, tri_cos[2]) ||

	    equals_v3v3(bmcb_data->co_b, tri_cos[0]) ||
	    equals_v3v3(bmcb_data->co_b, tri_cos[1]) ||
	    equals_v3v3(bmcb_data->co_b, tri_cos[2]))
	{
		return;
	}

	if (isect_ray_tri_v3(ray->origin, ray->direction, tri_cos[0], tri_cos[1], tri_cos[2], &dist, uv) &&
	    (dist < hit->dist))
	{
		hit->dist = dist;
		hit->index = index;

		copy_v3_v3(hit->no, ltri[0]->f->no);

		madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

		copy_v2_v2(bmcb_data->uv, uv);
	}
}

BMFace *BKE_bmbvh_find_face_segment(BMBVHTree *bmtree, const float co_a[3], const float co_b[3],
                                    float *r_fac, float r_hitout[3], float r_cagehit[3])
{
	BVHTreeRayHit hit;
	struct SegmentUserData bmcb_data;
	const float dist = len_v3v3(co_a, co_b);
	float dir[3];

	if (bmtree->cos_cage) BLI_assert(!(bmtree->bm->elem_index_dirty & BM_VERT));

	sub_v3_v3v3(dir, co_b, co_a);

	hit.dist = dist;
	hit.index = -1;

	/* ok to leave 'uv' uninitialized */
	bmcb_data.looptris = (const BMLoop *(*)[3])bmtree->looptris;
	bmcb_data.cos_cage = (const float (*)[3])bmtree->cos_cage;
	bmcb_data.co_a = co_a;
	bmcb_data.co_b = co_b;

	BLI_bvhtree_ray_cast(bmtree->tree, co_a, dir, 0.0f, &hit, bmbvh_find_face_segment_cb, &bmcb_data);
	if (hit.index != -1 && hit.dist != dist) {
		/* duplicate of BKE_bmbvh_ray_cast() */
		if (r_hitout) {
			if (bmtree->flag & BMBVH_RETURN_ORIG) {
				BMLoop **ltri = bmtree->looptris[hit.index];
				interp_v3_v3v3v3_uv(r_hitout, ltri[0]->v->co, ltri[1]->v->co, ltri[2]->v->co, bmcb_data.uv);
			}
			else {
				copy_v3_v3(r_hitout, hit.co);
			}

			if (r_cagehit) {
				copy_v3_v3(r_cagehit, hit.co);
			}
		}
		/* end duplicate */

		if (r_fac) {
			*r_fac = hit.dist / dist;
		}

		return bmtree->looptris[hit.index][0]->f;
	}

	return NULL;
}


/* -------------------------------------------------------------------- */
/* BKE_bmbvh_find_vert_closest */

struct VertSearchUserData {
	/* from the bmtree */
	const BMLoop *(*looptris)[3];
	const float (*cos_cage)[3];

	/* from the hit */
	float dist_max_sq;
	int   index_tri;
};

static void bmbvh_find_vert_closest_cb(void *userdata, int index, const float co[3], BVHTreeNearest *hit)
{
	struct VertSearchUserData *bmcb_data = userdata;
	const BMLoop **ltri = bmcb_data->looptris[index];
	const float dist_max_sq = bmcb_data->dist_max_sq;
	int i;

	const float *tri_cos[3];

	bmbvh_tri_from_face(tri_cos, ltri, bmcb_data->cos_cage);

	for (i = 0; i < 3; i++) {
		const float dist_sq = len_squared_v3v3(co, tri_cos[i]);
		if (dist_sq < hit->dist_sq && dist_sq < dist_max_sq) {
			copy_v3_v3(hit->co, tri_cos[i]);
			/* XXX, normal ignores cage */
			copy_v3_v3(hit->no, ltri[i]->v->no);
			hit->dist_sq = dist_sq;
			hit->index = index;
			bmcb_data->index_tri = i;
		}
	}
}

BMVert *BKE_bmbvh_find_vert_closest(BMBVHTree *bmtree, const float co[3], const float dist_max)
{
	BVHTreeNearest hit;
	struct VertSearchUserData bmcb_data;
	const float dist_max_sq = dist_max * dist_max;

	if (bmtree->cos_cage) BLI_assert(!(bmtree->bm->elem_index_dirty & BM_VERT));

	hit.dist_sq = dist_max_sq;
	hit.index = -1;

	bmcb_data.looptris = (const BMLoop *(*)[3])bmtree->looptris;
	bmcb_data.cos_cage = (const float (*)[3])bmtree->cos_cage;
	bmcb_data.dist_max_sq = dist_max_sq;

	BLI_bvhtree_find_nearest(bmtree->tree, co, &hit, bmbvh_find_vert_closest_cb, &bmcb_data);
	if (hit.index != -1) {
		BMLoop **ltri = bmtree->looptris[hit.index];
		return ltri[bmcb_data.index_tri]->v;
	}

	return NULL;
}
