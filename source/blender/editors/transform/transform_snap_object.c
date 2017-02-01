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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_snap_object.c
 *  \ingroup edtransform
 */

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_memarena.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_anim.h"  /* for duplis */
#include "BKE_editmesh.h"
#include "BKE_main.h"
#include "BKE_tracking.h"

#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "ED_armature.h"

#include "transform.h"

enum eViewProj {
	VIEW_PROJ_NONE = -1,
	VIEW_PROJ_ORTHO = 0,
	VIEW_PROJ_PERSP = -1,
};

typedef struct SnapData {
	short snap_to;
	float mval[2];
	float ray_origin[3];
	float ray_start[3];
	float ray_dir[3];
	float pmat[4][4]; /* perspective matrix */
	float win_half[2];/* win x and y */
	enum eViewProj view_proj;
	float depth_range[2];
} SnapData;

typedef struct SnapObjectData {
	enum {
		SNAP_MESH = 1,
		SNAP_EDIT_MESH,
	} type;
} SnapObjectData;

typedef struct SnapObjectData_Mesh {
	SnapObjectData sd;
	BVHTreeFromMesh *bvh_trees[3];

} SnapObjectData_Mesh;

typedef struct SnapObjectData_EditMesh {
	SnapObjectData sd;
	BVHTreeFromEditMesh *bvh_trees[3];

} SnapObjectData_EditMesh;

struct SnapObjectContext {
	Main *bmain;
	Scene *scene;
	int flag;

	/* Optional: when performing screen-space projection.
	 * otherwise this doesn't take viewport into account. */
	bool use_v3d;
	struct {
		const struct View3D *v3d;
		const struct ARegion *ar;
	} v3d_data;


	/* Object -> SnapObjectData map */
	struct {
		GHash *object_map;
		MemArena *mem_arena;
	} cache;

	/* Filter data, returns true to check this value */
	struct {
		struct {
			bool (*test_vert_fn)(BMVert *, void *user_data);
			bool (*test_edge_fn)(BMEdge *, void *user_data);
			bool (*test_face_fn)(BMFace *, void *user_data);
			void *user_data;
		} edit_mesh;
	} callbacks;

};

static int dm_looptri_to_poly_index(DerivedMesh *dm, const MLoopTri *lt);


/* -------------------------------------------------------------------- */

/** \name Support for storing all depths, not just the first (raycast 'all')
 *
 * This uses a list of #SnapObjectHitDepth structs.
 *
 * \{ */

/* Store all ray-hits */
struct RayCastAll_Data {
	void *bvhdata;

	/* internal vars for adding depths */
	BVHTree_RayCastCallback raycast_callback;

	const float(*obmat)[4];
	const float(*timat)[3];

	float len_diff;
	float local_scale;

	Object *ob;
	unsigned int ob_uuid;

	/* output data */
	ListBase *hit_list;
	bool retval;
};

static struct SnapObjectHitDepth *hit_depth_create(
        const float depth, const float co[3], const float no[3], int index,
        Object *ob, const float obmat[4][4], unsigned int ob_uuid)
{
	struct SnapObjectHitDepth *hit = MEM_mallocN(sizeof(*hit), __func__);

	hit->depth = depth;
	copy_v3_v3(hit->co, co);
	copy_v3_v3(hit->no, no);
	hit->index = index;

	hit->ob = ob;
	copy_m4_m4(hit->obmat, (float(*)[4])obmat);
	hit->ob_uuid = ob_uuid;

	return hit;
}

static int hit_depth_cmp(const void *arg1, const void *arg2)
{
	const struct SnapObjectHitDepth *h1 = arg1;
	const struct SnapObjectHitDepth *h2 = arg2;
	int val = 0;

	if (h1->depth < h2->depth) {
		val = -1;
	}
	else if (h1->depth > h2->depth) {
		val = 1;
	}

	return val;
}

static void raycast_all_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	struct RayCastAll_Data *data = userdata;
	data->raycast_callback(data->bvhdata, index, ray, hit);
	if (hit->index != -1) {
		/* get all values in worldspace */
		float location[3], normal[3];
		float depth;

		/* worldspace location */
		mul_v3_m4v3(location, (float(*)[4])data->obmat, hit->co);
		depth = (hit->dist + data->len_diff) / data->local_scale;

		/* worldspace normal */
		copy_v3_v3(normal, hit->no);
		mul_m3_v3((float(*)[3])data->timat, normal);
		normalize_v3(normal);

		/* currently unused, and causes issues when looptri's haven't been calculated.
		 * since theres some overhead in ensuring this data is valid, it may need to be optional. */
#if 0
		if (data->dm) {
			hit->index = dm_looptri_to_poly_index(data->dm, &data->dm_looptri[hit->index]);
		}
#endif

		struct SnapObjectHitDepth *hit_item = hit_depth_create(
		        depth, location, normal, hit->index,
		        data->ob, data->obmat, data->ob_uuid);
		BLI_addtail(data->hit_list, hit_item);
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \Common utilities
 * \{ */

MINLINE float depth_get(const float co[3], const float ray_start[3], const float ray_dir[3])
{
	float dvec[3];
	sub_v3_v3v3(dvec, co, ray_start);
	return dot_v3v3(dvec, ray_dir);
}

/**
 * Struct that kepts basic information about a BVHTree build from a editmesh.
 */
typedef struct BVHTreeFromMeshType {
	void *userdata;
	char type;
} BVHTreeFromMeshType;

/**
 * Generates a struct with the immutable parameters that will be used on all objects.
 *
 * \param snap_to: Element to snap, Vertice, Edge or Face.
 * \param view_proj: ORTHO or PERSP.
 * Currently only works one at a time, but can eventually operate as flag.
 *
 * \param mval: Mouse coords.
 * (When NULL, ray-casting is handled without any projection matrix correction.)
 * \param ray_origin: ray_start before being moved toward the ray_normal at the distance from vew3d clip_min.
 * \param ray_start: ray_origin moved for the start clipping plane (clip_min).
 * \param ray_direction: Unit length direction of the ray.
 * \param depth_range: distances of clipe plane min and clip plane max;
 */
static void snap_data_set(
        SnapData *snapdata,
        const ARegion *ar, const unsigned short snap_to, const enum eViewProj view_proj,
        const float mval[2], const float ray_origin[3], const float ray_start[3],
        const float ray_direction[3], const float depth_range[2])
{
	if (ar) {
		copy_m4_m4(snapdata->pmat, ((RegionView3D *)ar->regiondata)->persmat);
		snapdata->win_half[0] = ar->winx / 2;
		snapdata->win_half[1] = ar->winy / 2;
	}
	if (mval) {
		copy_v2_v2(snapdata->mval, mval);
	}
	snapdata->snap_to = snap_to;
	copy_v3_v3(snapdata->ray_origin, ray_origin);
	copy_v3_v3(snapdata->ray_start, ray_start);
	copy_v3_v3(snapdata->ray_dir, ray_direction);
	snapdata->view_proj = view_proj;
	copy_v2_v2(snapdata->depth_range, depth_range);
}

static void copy_vert_no(const BVHTreeFromMeshType *meshdata, const int index, float r_no[3])
{
	switch (meshdata->type) {
		case SNAP_MESH:
		{
			BVHTreeFromMesh *data = meshdata->userdata;
			const MVert *vert = data->vert + index;
			normal_short_to_float_v3(r_no, vert->no);
			break;
		}
		case SNAP_EDIT_MESH:
		{
			BVHTreeFromEditMesh *data = meshdata->userdata;
			BMVert *eve = BM_vert_at_index(data->em->bm, index);
			copy_v3_v3(r_no, eve->no);
			break;
		}
	}
}

static void get_edge_verts(
        const BVHTreeFromMeshType *meshdata, const int index,
        const float *v_pair[2])
{
	switch (meshdata->type) {
		case SNAP_MESH:
		{
			BVHTreeFromMesh *data = meshdata->userdata;

			const MVert *vert = data->vert;
			const MEdge *edge = data->edge + index;

			v_pair[0] = vert[edge->v1].co;
			v_pair[1] = vert[edge->v2].co;
			break;
		}
		case SNAP_EDIT_MESH:
		{
			BVHTreeFromEditMesh *data = meshdata->userdata;
			BMEdge *eed = BM_edge_at_index(data->em->bm, index);

			v_pair[0] = eed->v1->co;
			v_pair[1] = eed->v2->co;
			break;
		}
	}
}

static bool test_projected_vert_dist(
        const float depth_range[2], const float mval[2], const float co[3],
        float pmat[4][4], const float win_half[2], const bool is_persp,
        float *dist_px_sq, float r_co[3])
{
	float depth;
	if (is_persp) {
		depth = mul_project_m4_v3_zfac(pmat, co);
		if (depth < depth_range[0] || depth > depth_range[1]) {
			return false;
		}
	}

	float co2d[2] = {
		(dot_m4_v3_row_x(pmat, co) + pmat[3][0]),
		(dot_m4_v3_row_y(pmat, co) + pmat[3][1]),
	};

	if (is_persp) {
		mul_v2_fl(co2d, 1 / depth);
	}

	co2d[0] += 1.0f;
	co2d[1] += 1.0f;
	co2d[0] *= win_half[0];
	co2d[1] *= win_half[1];

	const float dist_sq = len_squared_v2v2(mval, co2d);
	if (dist_sq < *dist_px_sq) {
		copy_v3_v3(r_co, co);
		*dist_px_sq = dist_sq;
		return true;
	}
	return false;
}

static bool test_projected_edge_dist(
        const float depth_range[2], const float mval[2],
        float pmat[4][4], const float win_half[2], const bool is_persp,
        const float ray_start[3], const float ray_dir[3],
        const float va[3], const float vb[3],
        float *dist_px_sq, float r_co[3])
{

	float tmp_co[3], depth;
	dist_squared_ray_to_seg_v3(ray_start, ray_dir, va, vb, tmp_co, &depth);
	return test_projected_vert_dist(depth_range, mval, tmp_co, pmat, win_half, is_persp, dist_px_sq, r_co);
}
typedef struct Nearest2dPrecalc {
	float ray_origin_local[3];
	float ray_direction_local[3];
	float ray_inv_dir[3];

	float ray_min_dist;
	float pmat[4][4]; /* perspective matrix multiplied by object matrix */
	bool is_persp;
	float win_half[2];

	float mval[2];
	bool sign[3];
} Nearest2dPrecalc;

/**
 * \param lpmat: Perspective matrix multiplied by object matrix
 */
static void dist_squared_to_projected_aabb_precalc(
        struct Nearest2dPrecalc *neasrest_precalc,
        float lpmat[4][4], bool is_persp, const float win_half[2],
        const float ray_min_dist, const float mval[2],
        const float ray_origin_local[3], const float ray_direction_local[3])
{
	copy_m4_m4(neasrest_precalc->pmat, lpmat);
	neasrest_precalc->is_persp = is_persp;
	copy_v2_v2(neasrest_precalc->win_half, win_half);
	neasrest_precalc->ray_min_dist = ray_min_dist;

	copy_v3_v3(neasrest_precalc->ray_origin_local, ray_origin_local);
	copy_v3_v3(neasrest_precalc->ray_direction_local, ray_direction_local);
	copy_v2_v2(neasrest_precalc->mval, mval);

	for (int i = 0; i < 3; i++) {
		neasrest_precalc->ray_inv_dir[i] =
		        (neasrest_precalc->ray_direction_local[i] != 0.0f) ?
		        (1.0f / neasrest_precalc->ray_direction_local[i]) : FLT_MAX;
		neasrest_precalc->sign[i] = (neasrest_precalc->ray_inv_dir[i] < 0.0f);
	}
}

static float dist_squared_to_projected_aabb(
        struct Nearest2dPrecalc *data,
        const float bbmin[3], const float bbmax[3],
        bool r_axis_closest[3])
{
	float local_bvmin[3], local_bvmax[3];
	if (data->sign[0]) {
		local_bvmin[0] = bbmax[0];
		local_bvmax[0] = bbmin[0];
	}
	else {
		local_bvmin[0] = bbmin[0];
		local_bvmax[0] = bbmax[0];
	}
	if (data->sign[1]) {
		local_bvmin[1] = bbmax[1];
		local_bvmax[1] = bbmin[1];
	}
	else {
		local_bvmin[1] = bbmin[1];
		local_bvmax[1] = bbmax[1];
	}
	if (data->sign[2]) {
		local_bvmin[2] = bbmax[2];
		local_bvmax[2] = bbmin[2];
	}
	else {
		local_bvmin[2] = bbmin[2];
		local_bvmax[2] = bbmax[2];
	}

	const float tmin[3] = {
		(local_bvmin[0] - data->ray_origin_local[0]) * data->ray_inv_dir[0],
		(local_bvmin[1] - data->ray_origin_local[1]) * data->ray_inv_dir[1],
		(local_bvmin[2] - data->ray_origin_local[2]) * data->ray_inv_dir[2],
	};
	const float tmax[3] = {
		(local_bvmax[0] - data->ray_origin_local[0]) * data->ray_inv_dir[0],
		(local_bvmax[1] - data->ray_origin_local[1]) * data->ray_inv_dir[1],
		(local_bvmax[2] - data->ray_origin_local[2]) * data->ray_inv_dir[2],
	};
	/* `va` and `vb` are the coordinates of the AABB edge closest to the ray */
	float va[3], vb[3];
	/* `rtmin` and `rtmax` are the minimum and maximum distances of the ray hits on the AABB */
	float rtmin, rtmax;
	int main_axis;

	if ((tmax[0] <= tmax[1]) && (tmax[0] <= tmax[2])) {
		rtmax = tmax[0];
		va[0] = vb[0] = local_bvmax[0];
		main_axis = 3;
		r_axis_closest[0] = data->sign[0];
	}
	else if ((tmax[1] <= tmax[0]) && (tmax[1] <= tmax[2])) {
		rtmax = tmax[1];
		va[1] = vb[1] = local_bvmax[1];
		main_axis = 2;
		r_axis_closest[1] = data->sign[1];
	}
	else {
		rtmax = tmax[2];
		va[2] = vb[2] = local_bvmax[2];
		main_axis = 1;
		r_axis_closest[2] = data->sign[2];
	}

	if ((tmin[0] >= tmin[1]) && (tmin[0] >= tmin[2])) {
		rtmin = tmin[0];
		va[0] = vb[0] = local_bvmin[0];
		main_axis -= 3;
		r_axis_closest[0] = !data->sign[0];
	}
	else if ((tmin[1] >= tmin[0]) && (tmin[1] >= tmin[2])) {
		rtmin = tmin[1];
		va[1] = vb[1] = local_bvmin[1];
		main_axis -= 1;
		r_axis_closest[1] = !data->sign[1];
	}
	else {
		rtmin = tmin[2];
		va[2] = vb[2] = local_bvmin[2];
		main_axis -= 2;
		r_axis_closest[2] = !data->sign[2];
	}
	if (main_axis < 0) {
		main_axis += 3;
	}

	/* if rtmin < rtmax, ray intersect `AABB` */
	if (rtmin <= rtmax) {
#define IGNORE_BEHIND_RAY
#ifdef IGNORE_BEHIND_RAY
		/* `if rtmax < depth_min`, the hit is behind us */
		if (rtmax < data->ray_min_dist) {
			/* Test if the entire AABB is behind us */
			float depth = depth_get(
			        local_bvmax, data->ray_origin_local, data->ray_direction_local);
			if (depth < (data->ray_min_dist)) {
				return FLT_MAX;
			}
		}
#endif
		const float proj = rtmin * data->ray_direction_local[main_axis];
		r_axis_closest[main_axis] = (proj - va[main_axis]) < (vb[main_axis] - proj);
		return 0.0f;
	}
#ifdef IGNORE_BEHIND_RAY
	/* `if rtmin < depth_min`, the hit is behing us */
	else if (rtmin < data->ray_min_dist) {
		/* Test if the entire AABB is behind us */
		float depth = depth_get(
		        local_bvmax, data->ray_origin_local, data->ray_direction_local);
		if (depth < (data->ray_min_dist)) {
			return FLT_MAX;
		}
	}
#endif
#undef IGNORE_BEHIND_RAY
	if (data->sign[main_axis]) {
		va[main_axis] = local_bvmax[main_axis];
		vb[main_axis] = local_bvmin[main_axis];
	}
	else {
		va[main_axis] = local_bvmin[main_axis];
		vb[main_axis] = local_bvmax[main_axis];
	}
	float scale = fabsf(local_bvmax[main_axis] - local_bvmin[main_axis]);

	float (*pmat)[4] = data->pmat;

	float va2d[2] = {
		(dot_m4_v3_row_x(pmat, va) + pmat[3][0]),
		(dot_m4_v3_row_y(pmat, va) + pmat[3][1]),
	};
	float vb2d[2] = {
		(va2d[0] + pmat[main_axis][0] * scale),
		(va2d[1] + pmat[main_axis][1] * scale),
	};

	if (data->is_persp) {
		float depth_a = mul_project_m4_v3_zfac(pmat, va);
		float depth_b = depth_a + pmat[main_axis][3] * scale;
		va2d[0] /= depth_a;
		va2d[1] /= depth_a;
		vb2d[0] /= depth_b;
		vb2d[1] /= depth_b;
	}

	va2d[0] += 1.0f;
	va2d[1] += 1.0f;
	vb2d[0] += 1.0f;
	vb2d[1] += 1.0f;

	va2d[0] *= data->win_half[0];
	va2d[1] *= data->win_half[1];
	vb2d[0] *= data->win_half[0];
	vb2d[1] *= data->win_half[1];

	//float dvec[2], edge[2], rdist;
	//sub_v2_v2v2(dvec, data->mval, va2d);
	//sub_v2_v2v2(edge, vb2d, va2d);
	float rdist;
	short dvec[2] = {data->mval[0] - va2d[0], data->mval[1] - va2d[1]};
	short edge[2] = {vb2d[0] - va2d[0], vb2d[1] - va2d[1]};
	float lambda = dvec[0] * edge[0] + dvec[1] * edge[1];
	if (lambda != 0.0f) {
		lambda /= edge[0] * edge[0] + edge[1] * edge[1];
		if (lambda <= 0.0f) {
			rdist = len_squared_v2v2(data->mval, va2d);
			r_axis_closest[main_axis] = true;
		}
		else if (lambda >= 1.0f) {
			rdist = len_squared_v2v2(data->mval, vb2d);
			r_axis_closest[main_axis] = false;
		}
		else {
			va2d[0] += edge[0] * lambda;
			va2d[1] += edge[1] * lambda;
			rdist = len_squared_v2v2(data->mval, va2d);
			r_axis_closest[main_axis] = lambda < 0.5f;
		}
	}
	else {
		rdist = len_squared_v2v2(data->mval, va2d);
	}
	return rdist;
}

static float dist_squared_to_projected_aabb_simple(
        float lpmat[4][4], const float win_half[2],
        const float ray_min_dist, const float mval[2],
        const float ray_origin_local[3], const float ray_direction_local[3],
        const float bbmin[3], const float bbmax[3])
{
	struct Nearest2dPrecalc data;
	dist_squared_to_projected_aabb_precalc(
	        &data, lpmat, true, win_half, ray_min_dist,
	        mval, ray_origin_local, ray_direction_local);

	bool dummy[3] = {true, true, true};
	return dist_squared_to_projected_aabb(&data, bbmin, bbmax, dummy);
}

static float dist_aabb_to_plane(
        const float bbmin[3], const float bbmax[3],
        const float plane_co[3], const float plane_no[3])
{
	const float local_bvmin[3] = {
		(plane_no[0] < 0) ? bbmax[0] : bbmin[0],
		(plane_no[1] < 0) ? bbmax[1] : bbmin[1],
		(plane_no[2] < 0) ? bbmax[2] : bbmin[2],
	};
	return depth_get(local_bvmin, plane_co, plane_no);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \Walk DFS
 * \{ */
typedef struct Nearest2dUserData {
	struct Nearest2dPrecalc data_precalc;

	float dist_px_sq;

	bool r_axis_closest[3];

	float depth_range[2];
	void *userdata;
	int index;
	float co[3];
	float no[3];
} Nearest2dUserData;


static bool cb_walk_parent_snap_project(const BVHTreeAxisRange *bounds, void *user_data)
{
	Nearest2dUserData *data = user_data;
	const float bbmin[3] = {bounds[0].min, bounds[1].min, bounds[2].min};
	const float bbmax[3] = {bounds[0].max, bounds[1].max, bounds[2].max};
	const float rdist = dist_squared_to_projected_aabb(
	        &data->data_precalc, bbmin, bbmax, data->r_axis_closest);
	return rdist < data->dist_px_sq;
}

static bool cb_walk_leaf_snap_vert(const BVHTreeAxisRange *bounds, int index, void *userdata)
{
	struct Nearest2dUserData *data = userdata;
	struct Nearest2dPrecalc *neasrest_precalc = &data->data_precalc;
	const float co[3] = {
		(bounds[0].min + bounds[0].max) / 2,
		(bounds[1].min + bounds[1].max) / 2,
		(bounds[2].min + bounds[2].max) / 2,
	};

	if (test_projected_vert_dist(
	        data->depth_range,
	        neasrest_precalc->mval, co,
	        neasrest_precalc->pmat,
	        neasrest_precalc->win_half,
	        neasrest_precalc->is_persp,
	        &data->dist_px_sq,
	        data->co))
	{
		copy_vert_no(data->userdata, index, data->no);
		data->index = index;
	}
	return true;
}

static bool cb_walk_leaf_snap_edge(const BVHTreeAxisRange *UNUSED(bounds), int index, void *userdata)
{
	struct Nearest2dUserData *data = userdata;
	struct Nearest2dPrecalc *neasrest_precalc = &data->data_precalc;

	const float *v_pair[2];
	get_edge_verts(data->userdata, index, v_pair);

	if (test_projected_edge_dist(
	        data->depth_range,
	        neasrest_precalc->mval,
	        neasrest_precalc->pmat,
	        neasrest_precalc->win_half,
	        neasrest_precalc->is_persp,
	        neasrest_precalc->ray_origin_local,
	        neasrest_precalc->ray_direction_local,
	        v_pair[0], v_pair[1],
	        &data->dist_px_sq,
	        data->co))
	{
		sub_v3_v3v3(data->no, v_pair[0], v_pair[1]);
		data->index = index;
	}
	return true;
}

static bool cb_nearest_walk_order(const BVHTreeAxisRange *UNUSED(bounds), char axis, void *userdata)
{
	const bool *r_axis_closest = ((struct Nearest2dUserData *)userdata)->r_axis_closest;
	return r_axis_closest[axis];
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Internal Object Snapping API
 * \{ */

static bool snapArmature(
        SnapData *snapdata,
        Object *ob, bArmature *arm, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no))
{
	bool retval = false;

	float ray_start_local[3], ray_normal_local[3]; /* Used only in the snap to edges */
	if (snapdata->snap_to == SCE_SNAP_MODE_EDGE) {
		float imat[4][4];
		invert_m4_m4(imat, obmat);

		copy_v3_v3(ray_start_local, snapdata->ray_origin);
		copy_v3_v3(ray_normal_local, snapdata->ray_dir);
		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);
	}
	else if (snapdata->snap_to != SCE_SNAP_MODE_VERTEX) { /* Currently only edge and vert */
		return retval;
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
	float lpmat[4][4], dist_px_sq;
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
	dist_px_sq = SQUARE(*dist_px);

	if (arm->edbo) {
		for (EditBone *eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_ROOTSEL | BONE_TIPSEL)) == 0) {
					switch (snapdata->snap_to) {
						case SCE_SNAP_MODE_VERTEX:
							retval |= test_projected_vert_dist(
							        snapdata->depth_range, snapdata->mval, eBone->head,
							        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
							        r_loc);
							retval |= test_projected_vert_dist(
							        snapdata->depth_range, snapdata->mval, eBone->tail,
							        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
							        r_loc);
							break;
						case SCE_SNAP_MODE_EDGE:
							retval |= test_projected_edge_dist(
							        snapdata->depth_range, snapdata->mval, lpmat,
							        snapdata->win_half, is_persp, ray_start_local, ray_normal_local,
							        eBone->head, eBone->tail,
							        &dist_px_sq, r_loc);
							break;
					}
				}
			}
		}
	}
	else if (ob->pose && ob->pose->chanbase.first) {
		for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			Bone *bone = pchan->bone;
			/* skip hidden bones */
			if (bone && !(bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
				const float *head_vec = pchan->pose_head;
				const float *tail_vec = pchan->pose_tail;

				switch (snapdata->snap_to) {
					case SCE_SNAP_MODE_VERTEX:
						retval |= test_projected_vert_dist(
						        snapdata->depth_range, snapdata->mval, head_vec,
						        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
						        r_loc);
						retval |= test_projected_vert_dist(
						        snapdata->depth_range, snapdata->mval, tail_vec,
						        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
						        r_loc);
						break;
					case SCE_SNAP_MODE_EDGE:
						retval |= test_projected_edge_dist(
						        snapdata->depth_range, snapdata->mval, lpmat,
						        snapdata->win_half, is_persp, ray_start_local, ray_normal_local,
						        head_vec, tail_vec,
						        &dist_px_sq, r_loc);
						break;
				}
			}
		}
	}
	if (retval) {
		*dist_px = sqrtf(dist_px_sq);
		mul_m4_v3(obmat, r_loc);
		*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);
		return true;
	}
	return false;
}

static bool snapCurve(
        SnapData *snapdata,
        Object *ob, Curve *cu, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no))
{
	bool retval = false;

	/* only vertex snapping mode (eg control points and handles) supported for now) */
	if (snapdata->snap_to != SCE_SNAP_MODE_VERTEX) {
		return retval;
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
	float lpmat[4][4], dist_px_sq;
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
	dist_px_sq = SQUARE(*dist_px);

	for (Nurb *nu = (ob->mode == OB_MODE_EDIT ? cu->editnurb->nurbs.first : cu->nurb.first); nu; nu = nu->next) {
		for (int u = 0; u < nu->pntsu; u++) {
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_VERTEX:
				{
					if (ob->mode == OB_MODE_EDIT) {
						if (nu->bezt) {
							/* don't snap to selected (moving) or hidden */
							if (nu->bezt[u].f2 & SELECT || nu->bezt[u].hide != 0) {
								break;
							}
							retval |= test_projected_vert_dist(
							        snapdata->depth_range, snapdata->mval, nu->bezt[u].vec[1],
							        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
							        r_loc);
							/* don't snap if handle is selected (moving), or if it is aligning to a moving handle */
							if (!(nu->bezt[u].f1 & SELECT) &&
							    !(nu->bezt[u].h1 & HD_ALIGN && nu->bezt[u].f3 & SELECT))
							{
								retval |= test_projected_vert_dist(
								        snapdata->depth_range, snapdata->mval, nu->bezt[u].vec[0],
								        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
								        r_loc);
							}
							if (!(nu->bezt[u].f3 & SELECT) &&
							    !(nu->bezt[u].h2 & HD_ALIGN && nu->bezt[u].f1 & SELECT))
							{
								retval |= test_projected_vert_dist(
								        snapdata->depth_range, snapdata->mval, nu->bezt[u].vec[2],
								        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
								        r_loc);
							}
						}
						else {
							/* don't snap to selected (moving) or hidden */
							if (nu->bp[u].f1 & SELECT || nu->bp[u].hide != 0) {
								break;
							}
							retval |= test_projected_vert_dist(
							        snapdata->depth_range, snapdata->mval, nu->bp[u].vec,
							        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
							        r_loc);
						}
					}
					else {
						/* curve is not visible outside editmode if nurb length less than two */
						if (nu->pntsu > 1) {
							if (nu->bezt) {
								retval |= test_projected_vert_dist(
								        snapdata->depth_range, snapdata->mval, nu->bezt[u].vec[1],
								        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
								        r_loc);
							}
							else {
								retval |= test_projected_vert_dist(
								        snapdata->depth_range, snapdata->mval, nu->bp[u].vec,
								        lpmat, snapdata->win_half, is_persp, &dist_px_sq,
								        r_loc);
							}
						}
					}
					break;
				}
				default:
					break;
			}
		}
	}
	if (retval) {
		*dist_px = sqrtf(dist_px_sq);
		mul_m4_v3(obmat, r_loc);
		*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);
		return true;
	}
	return false;
}

/* may extend later (for now just snaps to empty center) */
static bool snapEmpty(
        SnapData *snapdata,
        Object *ob, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no))
{
	bool retval = false;

	if (ob->transflag & OB_DUPLI) {
		return retval;
	}

	/* for now only vertex supported */
	switch (snapdata->snap_to) {
		case SCE_SNAP_MODE_VERTEX:
		{
			bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
			float dist_px_sq = SQUARE(*dist_px);
			float tmp_co[3];
			copy_v3_v3(tmp_co, obmat[3]);
			if (test_projected_vert_dist(
				        snapdata->depth_range, snapdata->mval, tmp_co,
				        snapdata->pmat, snapdata->win_half, is_persp, &dist_px_sq,
				        r_loc)) {
				*dist_px = sqrtf(dist_px_sq);
				*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);
				retval = true;
			}
			break;
		}
		default:
			break;
	}

	return retval;
}

static bool snapCamera(
        const SnapObjectContext *sctx, SnapData *snapdata,
        Object *object, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no))
{
	Scene *scene = sctx->scene;

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
	float dist_px_sq = SQUARE(*dist_px);

	float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
	bool retval = false;
	MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
	MovieTracking *tracking;

	if (clip == NULL) {
		return retval;
	}
	if (object->transflag & OB_DUPLI) {
		return retval;
	}

	tracking = &clip->tracking;

	BKE_tracking_get_camera_object_matrix(scene, object, orig_camera_mat);

	invert_m4_m4(orig_camera_imat, orig_camera_mat);
	invert_m4_m4(imat, obmat);

	switch (snapdata->snap_to) {
		case SCE_SNAP_MODE_VERTEX:
		{
			MovieTrackingObject *tracking_object;

			for (tracking_object = tracking->objects.first;
			     tracking_object;
			     tracking_object = tracking_object->next)
			{
				ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
				MovieTrackingTrack *track;
				float reconstructed_camera_mat[4][4],
				      reconstructed_camera_imat[4][4];
				float (*vertex_obmat)[4];

				if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
					BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object,
					                                                  CFRA, reconstructed_camera_mat);

					invert_m4_m4(reconstructed_camera_imat, reconstructed_camera_mat);
				}

				for (track = tracksbase->first; track; track = track->next) {
					float bundle_pos[3];

					if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
						continue;
					}

					copy_v3_v3(bundle_pos, track->bundle_pos);
					if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
						vertex_obmat = orig_camera_mat;
					}
					else {
						mul_m4_v3(reconstructed_camera_imat, bundle_pos);
						vertex_obmat = obmat;
					}

					mul_m4_v3(vertex_obmat, bundle_pos);
					retval |= test_projected_vert_dist(
					        snapdata->depth_range, snapdata->mval, bundle_pos,
					        snapdata->pmat, snapdata->win_half, is_persp, &dist_px_sq,
					        r_loc);
				}
			}

			break;
		}
		default:
			break;
	}

	if (retval) {
		*dist_px = sqrtf(dist_px_sq);
		*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);
		return true;
	}
	return false;
}

static int dm_looptri_to_poly_index(DerivedMesh *dm, const MLoopTri *lt)
{
	const int *index_mp_to_orig = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	return index_mp_to_orig ? index_mp_to_orig[lt->poly] : lt->poly;
}

static bool snapDerivedMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, DerivedMesh *dm, float obmat[4][4], const unsigned int ob_index,
        bool do_bb,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        ListBase *r_hit_list)
{
	bool retval = false;

	if (snapdata->snap_to == SCE_SNAP_MODE_FACE) {
		if (dm->getNumPolys(dm) == 0) {
			return retval;
		}
	}
	else if (snapdata->snap_to == SCE_SNAP_MODE_EDGE) {
		if (dm->getNumEdges(dm) == 0) {
			return retval;
		}
	}
	else {
		if (dm->getNumVerts(dm) == 0) {
			return retval;
		}
	}

	{
		bool need_ray_start_correction_init =
		        (snapdata->snap_to == SCE_SNAP_MODE_FACE) &&
		        (snapdata->view_proj == VIEW_PROJ_ORTHO);

		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		float local_scale, local_depth, len_diff;

		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(ray_start_local, snapdata->ray_start);
		copy_v3_v3(ray_normal_local, snapdata->ray_dir);

		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);

		/* local scale in normal direction */
		local_scale = normalize_v3(ray_normal_local);
		local_depth = *ray_depth;
		if (local_depth != BVH_RAYCAST_DIST_MAX) {
			local_depth *= local_scale;
		}

		float lpmat[4][4];
		float ray_org_local[3];
		float ray_min_dist;
		if (ELEM(snapdata->snap_to, SCE_SNAP_MODE_VERTEX, SCE_SNAP_MODE_EDGE)) {
			mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
			ray_min_dist = snapdata->depth_range[0] * local_scale;
		}

		copy_v3_v3(ray_org_local, snapdata->ray_origin);
		mul_m4_v3(imat, ray_org_local);

		if (do_bb) {
			BoundBox *bb = BKE_object_boundbox_get(ob);

			if (bb) {
				BoundBox bb_temp;

				/* We cannot afford a bounding box with some null dimension, which may happen in some cases...
				 * Threshold is rather high, but seems to be needed to get good behavior, see T46099. */
				bb = BKE_boundbox_ensure_minimum_dimensions(bb, &bb_temp, 1e-1f);

				/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see T46816. */
				if (ELEM(snapdata->snap_to, SCE_SNAP_MODE_VERTEX, SCE_SNAP_MODE_EDGE)) {
					float dist_px_sq = dist_squared_to_projected_aabb_simple(
					        lpmat, snapdata->win_half, ray_min_dist, snapdata->mval,
					        ray_org_local, ray_normal_local, bb->vec[0], bb->vec[6]);
					if (dist_px_sq > SQUARE(*dist_px))
					{
						return retval;
					}
				}
				else {
					/* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
					if (!isect_ray_aabb_v3_simple(
						ray_start_local, ray_normal_local, bb->vec[0], bb->vec[6], NULL, NULL))
					{
						return retval;
					}
				}
				/* was local_depth, see: T47838 */
				len_diff = dist_aabb_to_plane(bb->vec[0], bb->vec[6], ray_start_local, ray_normal_local);
				if (len_diff < 0) len_diff = 0.0f;
				need_ray_start_correction_init = false;
			}
		}

		SnapObjectData_Mesh *sod = NULL;
		BVHTreeFromMesh *treedata = NULL, treedata_stack;

		if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
			void **sod_p;
			if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
				sod = *sod_p;
			}
			else {
				sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
				sod->sd.type = SNAP_MESH;
			}

			int tree_index = -1;
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_FACE:
					tree_index = 2;
					break;
				case SCE_SNAP_MODE_EDGE:
					tree_index = 1;
					break;
				case SCE_SNAP_MODE_VERTEX:
					tree_index = 0;
					break;
			}
			if (tree_index != -1) {
				if (sod->bvh_trees[tree_index] == NULL) {
					sod->bvh_trees[tree_index] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
				}
				treedata = sod->bvh_trees[tree_index];

				/* the tree is owned by the DM and may have been freed since we last used! */
				if (treedata && treedata->tree) {
					if (treedata->cached && !bvhcache_has_tree(dm->bvhCache, treedata->tree)) {
						free_bvhtree_from_mesh(treedata);
					}
				}
			}
		}
		else {
			treedata = &treedata_stack;
			memset(treedata, 0, sizeof(*treedata));
		}

		if (treedata && treedata->tree == NULL) {
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_FACE:
					bvhtree_from_mesh_looptri(treedata, dm, 0.0f, 4, 6);
					break;
				case SCE_SNAP_MODE_EDGE:
					bvhtree_from_mesh_edges(treedata, dm, 0.0f, 2, 6);
					break;
				case SCE_SNAP_MODE_VERTEX:
					bvhtree_from_mesh_verts(treedata, dm, 0.0f, 2, 6);
					break;
			}
		}

		if (!treedata || !treedata->tree) {
			return retval;
		}

		if (snapdata->snap_to == SCE_SNAP_MODE_FACE) {
			/* Only use closer ray_start in case of ortho view! In perspective one, ray_start may already
			 * been *inside* boundbox, leading to snap failures (see T38409).
			 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
			 */
			if (snapdata->view_proj == VIEW_PROJ_ORTHO) {  /* do_ray_start_correction */
				if (need_ray_start_correction_init) {
					/* We *need* a reasonably valid len_diff in this case.
					 * Use BHVTree to find the closest face from ray_start_local.
					 */
					BVHTreeNearest nearest;
					nearest.index = -1;
					nearest.dist_sq = FLT_MAX;
					/* Compute and store result. */
					BLI_bvhtree_find_nearest(
					        treedata->tree, ray_start_local, &nearest, treedata->nearest_callback, treedata);
					if (nearest.index != -1) {
						float dvec[3];
						sub_v3_v3v3(dvec, nearest.co, ray_start_local);
						len_diff = dot_v3v3(dvec, ray_normal_local);
					}
				}
				/* You need to make sure that ray_start is really far away,
				 * because even in the Orthografic view, in some cases,
				 * the ray can start inside the object (see T50486) */
				if (len_diff > 400.0f) {
					/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with
					 * very far away ray_start values (as returned in case of ortho view3d), see T38358.
					 */
					len_diff -= local_scale;  /* make temp start point a bit away from bbox hit point. */
					madd_v3_v3v3fl(
					        ray_start_local, ray_org_local, ray_normal_local,
					        len_diff + snapdata->depth_range[0] * local_scale);
					local_depth -= len_diff;
				}
				else len_diff = 0.0f;
			}
			else {
				len_diff = 0.0f;
			}
			if (r_hit_list) {
				struct RayCastAll_Data data;

				data.bvhdata = treedata;
				data.raycast_callback = treedata->raycast_callback;
				data.obmat = obmat;
				data.timat = timat;
				data.len_diff = len_diff;
				data.local_scale = local_scale;
				data.ob = ob;
				data.ob_uuid = ob_index;
				data.hit_list = r_hit_list;
				data.retval = retval;

				BLI_bvhtree_ray_cast_all(
				        treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				        *ray_depth, raycast_all_cb, &data);

				retval = data.retval;
			}
			else {
				BVHTreeRayHit hit = {.index = -1, .dist = local_depth};

				if (BLI_bvhtree_ray_cast(
				        treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				        &hit, treedata->raycast_callback, treedata) != -1)
				{
					hit.dist += len_diff;
					hit.dist /= local_scale;
					if (hit.dist <= *ray_depth) {
						*ray_depth = hit.dist;
						copy_v3_v3(r_loc, hit.co);

						/* back to worldspace */
						mul_m4_v3(obmat, r_loc);

						if (r_no) {
							copy_v3_v3(r_no, hit.no);
							mul_m3_v3(timat, r_no);
							normalize_v3(r_no);
						}

						retval = true;

						if (r_index) {
							*r_index = dm_looptri_to_poly_index(dm, &treedata->looptri[hit.index]);
						}
					}
				}
			}
		}
		/* SCE_SNAP_MODE_VERTEX or SCE_SNAP_MODE_EDGE */
		else {
			BVHTreeFromMeshType treedata_type = {.userdata = treedata, .type = SNAP_MESH};

			Nearest2dUserData neasrest2d = {
			    .dist_px_sq = SQUARE(*dist_px),
			    .r_axis_closest = {1.0f, 1.0f, 1.0f},
			    .depth_range = {snapdata->depth_range[0],  *ray_depth + snapdata->depth_range[0]},
			    .userdata = &treedata_type,
			    .index = -1};

			dist_squared_to_projected_aabb_precalc(
			        &neasrest2d.data_precalc, lpmat,
			        snapdata->view_proj == VIEW_PROJ_PERSP, snapdata->win_half,
			        ray_min_dist, snapdata->mval, ray_org_local, ray_normal_local);

			BVHTree_WalkLeafCallback cb_walk_leaf =
			        (snapdata->snap_to == SCE_SNAP_MODE_VERTEX) ?
			        cb_walk_leaf_snap_vert : cb_walk_leaf_snap_edge;

			BLI_bvhtree_walk_dfs(
			        treedata->tree,
			        cb_walk_parent_snap_project, cb_walk_leaf, cb_nearest_walk_order, &neasrest2d);

			if (neasrest2d.index != -1) {
				copy_v3_v3(r_loc, neasrest2d.co);
				mul_m4_v3(obmat, r_loc);
				if (r_no) {
					copy_v3_v3(r_no, neasrest2d.no);
					mul_m3_v3(timat, r_no);
					normalize_v3(r_no);
				}
				*dist_px = sqrtf(neasrest2d.dist_px_sq);
				*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);

				retval = true;
			}
		}

		if ((sctx->flag & SNAP_OBJECT_USE_CACHE) == 0) {
			if (treedata) {
				free_bvhtree_from_mesh(treedata);
			}
		}
	}

	return retval;
}

static bool snapEditMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, BMEditMesh *em, float obmat[4][4], const unsigned int ob_index,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        ListBase *r_hit_list)
{
	bool retval = false;

	if (snapdata->snap_to == SCE_SNAP_MODE_FACE) {
		if (em->bm->totface == 0) {
			return retval;
		}
	}
	if (snapdata->snap_to == SCE_SNAP_MODE_EDGE) {
		if (em->bm->totedge == 0) {
			return retval;
		}
	}
	else {
		if (em->bm->totvert == 0) {
			return retval;
		}
	}

	{
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_normal_local[3];

		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(ray_normal_local, snapdata->ray_dir);

		mul_mat3_m4_v3(imat, ray_normal_local);

		/* local scale in normal direction */
		float local_scale = normalize_v3(ray_normal_local);
		float local_depth = *ray_depth;
		if (local_depth != BVH_RAYCAST_DIST_MAX) {
			local_depth *= local_scale;
		}

		SnapObjectData_EditMesh *sod = NULL;

		BVHTreeFromEditMesh *treedata = NULL, treedata_stack;

		if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
			void **sod_p;
			if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
				sod = *sod_p;
			}
			else {
				sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
				sod->sd.type = SNAP_EDIT_MESH;
			}

			int tree_index = -1;
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_FACE:
					tree_index = 2;
					break;
				case SCE_SNAP_MODE_EDGE:
					tree_index = 1;
					break;
				case SCE_SNAP_MODE_VERTEX:
					tree_index = 0;
					break;
			}
			if (tree_index != -1) {
				if (sod->bvh_trees[tree_index] == NULL) {
					sod->bvh_trees[tree_index] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
				}
				treedata = sod->bvh_trees[tree_index];
			}
		}
		else {
			treedata = &treedata_stack;
			memset(treedata, 0, sizeof(*treedata));
		}

		if (treedata && treedata->tree == NULL) {
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_FACE:
				{
					BLI_bitmap *looptri_mask = NULL;
					int looptri_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_face_fn) {
						looptri_mask = BLI_BITMAP_NEW(em->tottri, __func__);
						looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
						        em->bm, looptri_mask,
						        sctx->callbacks.edit_mesh.test_face_fn, sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_looptri_ex(treedata, em, looptri_mask, looptri_num_active, 0.0f, 4, 6, NULL);
					if (looptri_mask) {
						MEM_freeN(looptri_mask);
					}
					break;
				}
				case SCE_SNAP_MODE_EDGE:
				{
					BLI_bitmap *edges_mask = NULL;
					int edges_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_edge_fn) {
						edges_mask = BLI_BITMAP_NEW(em->bm->totedge, __func__);
						edges_num_active = BM_iter_mesh_bitmap_from_filter(
						        BM_EDGES_OF_MESH, em->bm, edges_mask,
						        (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_edge_fn,
						        sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_edges_ex(treedata, em, edges_mask, edges_num_active, 0.0f, 2, 6);
					if (edges_mask) {
						MEM_freeN(edges_mask);
					}
					break;
				}
				case SCE_SNAP_MODE_VERTEX:
				{
					BLI_bitmap *verts_mask = NULL;
					int verts_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_vert_fn) {
						verts_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
						verts_num_active = BM_iter_mesh_bitmap_from_filter(
						        BM_VERTS_OF_MESH, em->bm, verts_mask,
						        (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_vert_fn,
						        sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_verts_ex(treedata, em, verts_mask, verts_num_active, 0.0f, 2, 6);
					if (verts_mask) {
						MEM_freeN(verts_mask);
					}
					break;
				}
			}
		}

		if (!treedata || !treedata->tree) {
			return retval;
		}

		if (snapdata->snap_to == SCE_SNAP_MODE_FACE) {
			float ray_start_local[3];
			copy_v3_v3(ray_start_local, snapdata->ray_start);
			mul_m4_v3(imat, ray_start_local);

			/* Only use closer ray_start in case of ortho view! In perspective one, ray_start
			 * may already been *inside* boundbox, leading to snap failures (see T38409).
			 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
			 */
			float len_diff = 0.0f;
			if (snapdata->view_proj == VIEW_PROJ_ORTHO) {  /* do_ray_start_correction */
				/* We *need* a reasonably valid len_diff in this case.
				 * Use BHVTree to find the closest face from ray_start_local.
				 */
				BVHTreeNearest nearest;
				nearest.index = -1;
				nearest.dist_sq = FLT_MAX;
				/* Compute and store result. */
				if (BLI_bvhtree_find_nearest(
				        treedata->tree, ray_start_local, &nearest, NULL, NULL) != -1)
				{
					float dvec[3];
					sub_v3_v3v3(dvec, nearest.co, ray_start_local);
					len_diff = dot_v3v3(dvec, ray_normal_local);
					/* You need to make sure that ray_start is really far away,
					 * because even in the Orthografic view, in some cases,
					 * the ray can start inside the object (see T50486) */
					if (len_diff > 400.0f) {
						float ray_org_local[3];

						copy_v3_v3(ray_org_local, snapdata->ray_origin);
						mul_m4_v3(imat, ray_org_local);

						/* We pass a temp ray_start, set from object's boundbox,
						 * to avoid precision issues with very far away ray_start values
						 * (as returned in case of ortho view3d), see T38358.
						 */
						len_diff -= local_scale; /* make temp start point a bit away from bbox hit point. */
						madd_v3_v3v3fl(
						        ray_start_local, ray_org_local, ray_normal_local,
						        len_diff + snapdata->depth_range[0] * local_scale);
						local_depth -= len_diff;
					}
					else len_diff = 0.0f;
				}
			}
			if (r_hit_list) {
				struct RayCastAll_Data data;

				data.bvhdata = treedata;
				data.raycast_callback = treedata->raycast_callback;
				data.obmat = obmat;
				data.timat = timat;
				data.len_diff = len_diff;
				data.local_scale = local_scale;
				data.ob = ob;
				data.ob_uuid = ob_index;
				data.hit_list = r_hit_list;
				data.retval = retval;

				BLI_bvhtree_ray_cast_all(
				        treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				        *ray_depth, raycast_all_cb, &data);

				retval = data.retval;
			}
			else {
				BVHTreeRayHit hit = {.index = -1, .dist = local_depth};

				if (BLI_bvhtree_ray_cast(
				        treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				        &hit, treedata->raycast_callback, treedata) != -1)
				{
					hit.dist += len_diff;
					hit.dist /= local_scale;
					if (hit.dist <= *ray_depth) {
						*ray_depth = hit.dist;
						copy_v3_v3(r_loc, hit.co);

						/* back to worldspace */
						mul_m4_v3(obmat, r_loc);

						if (r_no) {
							copy_v3_v3(r_no, hit.no);
							mul_m3_v3(timat, r_no);
							normalize_v3(r_no);
						}

						retval = true;

						if (r_index) {
							*r_index = hit.index;
						}
					}
				}
			}
		}
		else {
			float ray_org_local[3];
			copy_v3_v3(ray_org_local, snapdata->ray_origin);
			mul_m4_v3(imat, ray_org_local);

			BVHTreeFromMeshType treedata_type = {.userdata = treedata, .type = SNAP_EDIT_MESH};

			Nearest2dUserData neasrest2d = {
			    .dist_px_sq = SQUARE(*dist_px),
			    .r_axis_closest = {1.0f, 1.0f, 1.0f},
			    .depth_range = {snapdata->depth_range[0], *ray_depth + snapdata->depth_range[0]},
			    .userdata = &treedata_type,
			    .index = -1};

			float lpmat[4][4];
			mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
			dist_squared_to_projected_aabb_precalc(
			        &neasrest2d.data_precalc, lpmat,
			        snapdata->view_proj == VIEW_PROJ_PERSP, snapdata->win_half,
			        (snapdata->depth_range[0] * local_scale), snapdata->mval,
			        ray_org_local, ray_normal_local);

			BVHTree_WalkLeafCallback cb_walk_leaf =
			        (snapdata->snap_to == SCE_SNAP_MODE_VERTEX) ?
			        cb_walk_leaf_snap_vert : cb_walk_leaf_snap_edge;

			BLI_bvhtree_walk_dfs(
			        treedata->tree,
			        cb_walk_parent_snap_project, cb_walk_leaf, cb_nearest_walk_order, &neasrest2d);

			if (neasrest2d.index != -1) {
				copy_v3_v3(r_loc, neasrest2d.co);
				mul_m4_v3(obmat, r_loc);
				if (r_no) {
					copy_v3_v3(r_no, neasrest2d.no);
					mul_m3_v3(timat, r_no);
					normalize_v3(r_no);
				}
				*dist_px = sqrtf(neasrest2d.dist_px_sq);
				*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);

				retval = true;
			}
		}

		if ((sctx->flag & SNAP_OBJECT_USE_CACHE) == 0) {
			if (treedata) {
				free_bvhtree_from_editmesh(treedata);
			}
		}
	}

	return retval;
}

/**
 * \param use_obedit: Uses the coordinates of BMesh (if any) to do the snapping;
 *
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static bool snapObject(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, float obmat[4][4], const unsigned int ob_index,
        bool use_obedit,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
	bool retval = false;

	if (ob->type == OB_MESH) {
		BMEditMesh *em;

		if (use_obedit) {
			em = BKE_editmesh_from_object(ob);
			retval = snapEditMesh(
			        sctx, snapdata, ob, em, obmat, ob_index,
			        ray_depth, dist_px,
			        r_loc, r_no, r_index,
			        r_hit_list);
		}
		else {
			/* in this case we want the mesh from the editmesh, avoids stale data. see: T45978.
			 * still set the 'em' to NULL, since we only want the 'dm'. */
			DerivedMesh *dm;
			em = BKE_editmesh_from_object(ob);
			if (em) {
				editbmesh_get_derived_cage_and_final(sctx->scene, ob, em, CD_MASK_BAREMESH, &dm);
			}
			else {
				dm = mesh_get_derived_final(sctx->scene, ob, CD_MASK_BAREMESH);
			}
			retval = snapDerivedMesh(
			        sctx, snapdata, ob, dm, obmat, ob_index,
			        true,
			        ray_depth, dist_px,
			        r_loc, r_no,
			        r_index, r_hit_list);

			dm->release(dm);
		}
	}
	else if (snapdata->snap_to != SCE_SNAP_MODE_FACE) {
		if (ob->type == OB_ARMATURE) {
			retval = snapArmature(
			        snapdata,
			        ob, ob->data, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
		}
		else if (ob->type == OB_CURVE) {
			retval = snapCurve(
			        snapdata,
			        ob, ob->data, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
		}
		else if (ob->type == OB_EMPTY) {
			retval = snapEmpty(
			        snapdata,
			        ob, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
		}
		else if (ob->type == OB_CAMERA) {
			retval = snapCamera(
			        sctx, snapdata, ob, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
		}
	}

	if (retval) {
		if (r_ob) {
			*r_ob = ob;
			copy_m4_m4(r_obmat, obmat);
		}
	}

	return retval;
}

/**
 * Main Snapping Function
 * ======================
 *
 * Walks through all objects in the scene to find the closest snap element ray.
 *
 * \param sctx: Snap context to store data.
 * \param snapdata: struct generated in `get_snapdata`.
 * \param snap_select: from enum SnapSelect.
 * \param use_object_edit_cage: Uses the coordinates of BMesh (if any) to do the snapping.
 *
 * Read/Write Args
 * ---------------
 *
 * \param ray_depth: maximum depth allowed for r_co, elements deeper than this value will be ignored.
 * \param dist_px: Maximum threshold distance (in pixels).
 *
 * Output Args
 * -----------
 *
 * \param r_loc: Hit location.
 * \param r_no: Hit normal (optional).
 * \param r_index: Hit index or -1 when no valid index is found.
 * (currently only set to the polygon index when when using ``snap_to == SCE_SNAP_MODE_FACE``).
 * \param r_ob: Hit object.
 * \param r_obmat: Object matrix (may not be #Object.obmat with dupli-instances).
 * \param r_hit_list: List of #SnapObjectHitDepth (caller must free).
 *
 */
static bool snapObjectsRay(
        SnapObjectContext *sctx, SnapData *snapdata,
        const SnapSelect snap_select,
        const bool use_object_edit_cage,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
	bool retval = false;

	unsigned int ob_index = 0;
	Object *obedit = use_object_edit_cage ? sctx->scene->obedit : NULL;

	/* Need an exception for particle edit because the base is flagged with BA_HAS_RECALC_DATA
	 * which makes the loop skip it, even the derived mesh will never change
	 *
	 * To solve that problem, we do it first as an exception.
	 * */
	Base *base_act = sctx->scene->basact;
	if (base_act && base_act->object && base_act->object->mode & OB_MODE_PARTICLE_EDIT) {
		Object *ob = base_act->object;

		retval |= snapObject(
		        sctx, snapdata, ob, ob->obmat, ob_index++, false,
		        ray_depth, dist_px,
		        r_loc, r_no, r_index, r_ob, r_obmat, r_hit_list);
	}

	bool ignore_object_selected = false, ignore_object_active = false;
	switch (snap_select) {
		case SNAP_ALL:
			break;
		case SNAP_NOT_SELECTED:
			ignore_object_selected = true;
			break;
		case SNAP_NOT_ACTIVE:
			ignore_object_active = true;
			break;
	}
	for (Base *base = sctx->scene->base.first; base != NULL; base = base->next) {
		if ((BASE_VISIBLE_BGMODE(sctx->v3d_data.v3d, sctx->scene, base)) &&
		    (base->flag & (BA_HAS_RECALC_OB | BA_HAS_RECALC_DATA)) == 0 &&

		    !((ignore_object_selected && (base->flag & (SELECT | BA_WAS_SEL))) ||
		      (ignore_object_active && base == base_act)))
		{
			Object *ob = base->object;

			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(sctx->bmain->eval_ctx, sctx->scene, ob);

				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					bool use_obedit_dupli = (obedit && dupli_ob->ob->data == obedit->data);
					Object *dupli_snap = (use_obedit_dupli) ? obedit : dupli_ob->ob;

					retval |= snapObject(
					        sctx, snapdata, dupli_snap, dupli_ob->mat,
					        ob_index++, use_obedit_dupli,
					        ray_depth, dist_px,
					        r_loc, r_no, r_index, r_ob, r_obmat, r_hit_list);
				}

				free_object_duplilist(lb);
			}

			bool use_obedit = (obedit != NULL) && (ob->data == obedit->data);
			Object *ob_snap = use_obedit ? obedit : ob;

			retval |= snapObject(
			        sctx, snapdata, ob_snap, ob->obmat, ob_index++, use_obedit,
			        ray_depth, dist_px,
			        r_loc, r_no, r_index, r_ob, r_obmat, r_hit_list);
		}
	}

	return retval;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *ED_transform_snap_object_context_create(
        Main *bmain, Scene *scene, int flag)
{
	SnapObjectContext *sctx = MEM_callocN(sizeof(*sctx), __func__);

	sctx->flag = flag;

	sctx->bmain = bmain;
	sctx->scene = scene;

	return sctx;
}

SnapObjectContext *ED_transform_snap_object_context_create_view3d(
        Main *bmain, Scene *scene, int flag,
        /* extra args for view3d */
        const ARegion *ar, const View3D *v3d)
{
	SnapObjectContext *sctx = ED_transform_snap_object_context_create(bmain, scene, flag);

	sctx->use_v3d = true;
	sctx->v3d_data.ar = ar;
	sctx->v3d_data.v3d = v3d;

	if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
		sctx->cache.object_map = BLI_ghash_ptr_new(__func__);
		sctx->cache.mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
	}

	return sctx;
}

static void snap_object_data_free(void *sod_v)
{
	switch (((SnapObjectData *)sod_v)->type) {
		case SNAP_MESH:
		{
			SnapObjectData_Mesh *sod = sod_v;
			for (int i = 0; i < ARRAY_SIZE(sod->bvh_trees); i++) {
				if (sod->bvh_trees[i]) {
					free_bvhtree_from_mesh(sod->bvh_trees[i]);
				}
			}
			break;
		}
		case SNAP_EDIT_MESH:
		{
			SnapObjectData_EditMesh *sod = sod_v;
			for (int i = 0; i < ARRAY_SIZE(sod->bvh_trees); i++) {
				if (sod->bvh_trees[i]) {
					free_bvhtree_from_editmesh(sod->bvh_trees[i]);
				}
			}
			break;
		}
	}
}

void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx)
{
	if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
		BLI_ghash_free(sctx->cache.object_map, NULL, snap_object_data_free);
		BLI_memarena_free(sctx->cache.mem_arena);
	}

	MEM_freeN(sctx);
}

void ED_transform_snap_object_context_set_editmesh_callbacks(
        SnapObjectContext *sctx,
        bool (*test_vert_fn)(BMVert *, void *user_data),
        bool (*test_edge_fn)(BMEdge *, void *user_data),
        bool (*test_face_fn)(BMFace *, void *user_data),
        void *user_data)
{
	sctx->callbacks.edit_mesh.test_vert_fn = test_vert_fn;
	sctx->callbacks.edit_mesh.test_edge_fn = test_edge_fn;
	sctx->callbacks.edit_mesh.test_face_fn = test_face_fn;

	sctx->callbacks.edit_mesh.user_data = user_data;
}

bool ED_transform_snap_object_project_ray_ex(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float UNUSED(ray_start[3]), const float UNUSED(ray_normal[3]),
        float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	const float depth_range[2] = {0.0f, FLT_MAX};

	SnapData snapdata;
	snap_data_set(&snapdata, sctx->v3d_data.ar, snap_to, VIEW_PROJ_NONE, NULL, r_loc, r_loc, r_no, depth_range);

	return snapObjectsRay(
	        sctx, &snapdata,
	        params->snap_select, params->use_object_edit_cage,
	        ray_depth, NULL,
	        r_loc, r_no, r_index, r_ob, r_obmat, NULL);
}

/**
 * Fill in a list of all hits.
 *
 * \param ray_depth: Only depths in this range are considered, -1.0 for maximum.
 * \param sort: Optionally sort the hits by depth.
 * \param r_hit_list: List of #SnapObjectHitDepth (caller must free).
 */
bool ED_transform_snap_object_project_ray_all(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3],
        float ray_depth, bool sort,
        ListBase *r_hit_list)
{
	const float depth_range[2] = {0.0f, FLT_MAX};
	if (ray_depth == -1.0f) {
		ray_depth = BVH_RAYCAST_DIST_MAX;
	}

#ifdef DEBUG
	float ray_depth_prev = ray_depth;
#endif

	SnapData snapdata;
	snap_data_set(&snapdata, sctx->v3d_data.ar, snap_to, VIEW_PROJ_NONE, NULL,
	        ray_start, ray_start, ray_normal, depth_range);

	bool retval = snapObjectsRay(
	        sctx, &snapdata,
	        params->snap_select, params->use_object_edit_cage,
	        &ray_depth, NULL,
	        NULL, NULL, NULL, NULL, NULL,
	        r_hit_list);

	/* meant to be readonly for 'all' hits, ensure it is */
#ifdef DEBUG
	BLI_assert(ray_depth_prev == ray_depth);
#endif

	if (sort) {
		BLI_listbase_sort(r_hit_list, hit_depth_cmp);
	}

	return retval;
}

/**
 * Convenience function for snap ray-casting.
 *
 * Given a ray, cast it into the scene (snapping to faces).
 *
 * \return Snap success
 */
static bool transform_snap_context_project_ray_impl(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_co[3], float r_no[3])
{
	bool ret;

	/* try snap edge, then face if it fails */
	ret = ED_transform_snap_object_project_ray_ex(
	        sctx,
	        SCE_SNAP_MODE_FACE,
	        params,
	        ray_start, ray_normal, ray_depth,
	        r_co, r_no, NULL,
	        NULL, NULL);

	return ret;
}

bool ED_transform_snap_object_project_ray(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float ray_origin[3], const float ray_direction[3], float *ray_depth,
        float r_co[3], float r_no[3])
{
	float ray_depth_fallback;
	if (ray_depth == NULL) {
		ray_depth_fallback = BVH_RAYCAST_DIST_MAX;
		ray_depth = &ray_depth_fallback;
	}

	return transform_snap_context_project_ray_impl(
	        sctx,
	        params,
	        ray_origin, ray_direction, ray_depth,
	        r_co, r_no);
}

static bool transform_snap_context_project_view3d_mixed_impl(
        SnapObjectContext *sctx,
        const unsigned short snap_to_flag,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        bool use_depth,
        float r_co[3], float r_no[3])
{
	float ray_depth = BVH_RAYCAST_DIST_MAX;
	bool is_hit = false;

	const int  elem_type[3] = {SCE_SNAP_MODE_VERTEX, SCE_SNAP_MODE_EDGE, SCE_SNAP_MODE_FACE};

	BLI_assert(snap_to_flag != 0);
	BLI_assert((snap_to_flag & ~(1 | 2 | 4)) == 0);

	if (use_depth) {
		const float dist_px_orig = *dist_px;
		for (int i = 2; i >= 0; i--) {
			if (snap_to_flag & (1 << i)) {
				if (i == 0)
					*dist_px = dist_px_orig;
				if (ED_transform_snap_object_project_view3d(
				        sctx,
				        elem_type[i], params,
				        mval, dist_px, &ray_depth,
				        r_co, r_no))
				{
					/* 0.01 is a random but small value to prioritizing
					 * the first elements of the loop */
					ray_depth += 0.01f;
					is_hit = true;
				}
			}
		}
	}
	else {
		for (int i = 0; i < 3; i++) {
			if (snap_to_flag & (1 << i)) {
				if (ED_transform_snap_object_project_view3d(
					sctx,
					elem_type[i], params,
					mval, dist_px, &ray_depth,
					r_co, r_no))
				{
					is_hit = true;
					break;
				}
			}
		}
	}

	return is_hit;
}

/**
 * Convenience function for performing snapping.
 *
 * Given a 2D region value, snap to vert/edge/face.
 *
 * \param sctx: Snap context.
 * \param mval_fl: Screenspace coordinate.
 * \param dist_px: Maximum distance to snap (in pixels).
 * \param use_depth: Snap to the closest element, use when using more than one snap type.
 * \param r_co: hit location.
 * \param r_no: hit normal (optional).
 * \return Snap success
 */
bool ED_transform_snap_object_project_view3d_mixed(
        SnapObjectContext *sctx,
        const unsigned short snap_to_flag,
        const struct SnapObjectParams *params,
        const float mval_fl[2], float *dist_px,
        bool use_depth,
        float r_co[3], float r_no[3])
{
	return transform_snap_context_project_view3d_mixed_impl(
	        sctx,
	        snap_to_flag, params,
	        mval_fl, dist_px, use_depth,
	        r_co, r_no);
}

bool ED_transform_snap_object_project_view3d_ex(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index)
{
	float ray_origin[3], ray_start[3], ray_normal[3], depth_range[2], ray_end[3];

	const ARegion *ar = sctx->v3d_data.ar;
	const RegionView3D *rv3d = ar->regiondata;

	ED_view3d_win_to_origin(ar, mval, ray_origin);
	ED_view3d_win_to_vector(ar, mval, ray_normal);

	ED_view3d_clip_range_get(
	        sctx->v3d_data.v3d, sctx->v3d_data.ar->regiondata,
	        &depth_range[0], &depth_range[1], false);

	madd_v3_v3v3fl(ray_start, ray_origin, ray_normal, depth_range[0]);
	madd_v3_v3v3fl(ray_end, ray_origin, ray_normal, depth_range[1]);

	if (!ED_view3d_clip_segment(rv3d, ray_start, ray_end)) {
		return false;
	}

	float ray_depth_fallback;
	if (ray_depth == NULL) {
		ray_depth_fallback = BVH_RAYCAST_DIST_MAX;
		ray_depth = &ray_depth_fallback;
	}

	SnapData snapdata;
	const enum eViewProj view_proj = ((RegionView3D *)ar->regiondata)->is_persp ? VIEW_PROJ_PERSP : VIEW_PROJ_ORTHO;
	snap_data_set(&snapdata, ar, snap_to, view_proj, mval,
	        ray_origin, ray_start, ray_normal, depth_range);

	return snapObjectsRay(
	        sctx, &snapdata,
	        params->snap_select, params->use_object_edit_cage,
	        ray_depth, dist_px,
	        r_loc, r_no, r_index, NULL, NULL, NULL);
}

bool ED_transform_snap_object_project_view3d(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        float r_loc[3], float r_no[3])
{
	return ED_transform_snap_object_project_view3d_ex(
	        sctx,
	        snap_to,
	        params,
	        mval, dist_px,
	        ray_depth,
	        r_loc, r_no, NULL);
}

/**
 * see: #ED_transform_snap_object_project_ray_all
 */
bool ED_transform_snap_object_project_all_view3d_ex(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2],
        float ray_depth, bool sort,
        ListBase *r_hit_list)
{
	float ray_start[3], ray_normal[3];

	if (!ED_view3d_win_to_ray_ex(
	        sctx->v3d_data.ar, sctx->v3d_data.v3d,
	        mval, NULL, ray_normal, ray_start, true))
	{
		return false;
	}

	return ED_transform_snap_object_project_ray_all(
	        sctx,
	        SCE_SNAP_MODE_FACE,
	        params,
	        ray_start, ray_normal, ray_depth, sort,
	        r_hit_list);
}

/** \} */
