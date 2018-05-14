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
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_bvhutils.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_object.h"
#include "BKE_anim.h"  /* for duplis */
#include "BKE_editmesh.h"
#include "BKE_main.h"
#include "BKE_tracking.h"
#include "BKE_context.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "ED_armature.h"

#include "transform.h"

/* -------------------------------------------------------------------- */
/** Internal Data Types
 * \{ */

enum eViewProj {
	VIEW_PROJ_NONE = -1,
	VIEW_PROJ_ORTHO = 0,
	VIEW_PROJ_PERSP = -1,
};

typedef struct SnapData {
	short snap_to;
	float mval[2];
	float ray_start[3];
	float ray_dir[3];
	float pmat[4][4]; /* perspective matrix */
	float win_size[2];/* win x and y */
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
	BVHTreeFromMesh treedata;
	BVHTree *bvhtree[2]; /* from loose verts and from loose edges */
	uint has_looptris   : 1;
	uint has_loose_edge : 1;
	uint has_loose_vert : 1;

} SnapObjectData_Mesh;

typedef struct SnapObjectData_EditMesh {
	SnapObjectData sd;
	BVHTreeFromEditMesh *bvh_trees[3];

} SnapObjectData_EditMesh;

struct SnapObjectContext {
	Main *bmain;
	Scene *scene;
	Depsgraph *depsgraph;

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

/** \} */

/* -------------------------------------------------------------------- */
/** Common Utilities
 * \{ */


typedef void(*IterSnapObjsCallback)(SnapObjectContext *sctx, bool is_obedit, Object *ob, float obmat[4][4], void *data);

/**
 * Walks through all objects in the scene to create the list of objets to snap.
 *
 * \param sctx: Snap context to store data.
 * \param snap_select : from enum eSnapSelect.
 * \param obedit : Object Edited to use its coordinates of BMesh(if any) to do the snapping.
 */
static void iter_snap_objects(
        SnapObjectContext *sctx,
        const eSnapSelect snap_select,
        Object *obedit,
        IterSnapObjsCallback sob_callback,
        void *data)
{
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(sctx->depsgraph);
	Base *base_act = view_layer->basact;
	for (Base *base = view_layer->object_bases.first; base != NULL; base = base->next) {
		if ((BASE_VISIBLE(base)) && (base->flag_legacy & BA_SNAP_FIX_DEPS_FIASCO) == 0 &&
		    !((snap_select == SNAP_NOT_SELECTED && ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL))) ||
		      (snap_select == SNAP_NOT_ACTIVE && base == base_act)))
		{
			bool use_obedit;
			Object *obj = base->object;
			if (obj->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(sctx->depsgraph, sctx->scene, obj);
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					use_obedit = obedit && dupli_ob->ob->data == obedit->data;
					sob_callback(sctx, use_obedit, use_obedit ? obedit : dupli_ob->ob, dupli_ob->mat, data);
				}
				free_object_duplilist(lb);
			}

			use_obedit = obedit && obj->data == obedit->data;
			sob_callback(sctx, use_obedit, use_obedit ? obedit : obj, obj->obmat, data);
		}
	}
}


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
        const float mval[2], const float ray_start[3], const float ray_direction[3],
        const float depth_range[2])
{
	copy_m4_m4(snapdata->pmat, ((RegionView3D *)ar->regiondata)->persmat);
	snapdata->win_size[0] = ar->winx;
	snapdata->win_size[1] = ar->winy;
	copy_v2_v2(snapdata->mval, mval);
	snapdata->snap_to = snap_to;
	copy_v3_v3(snapdata->ray_start, ray_start);
	copy_v3_v3(snapdata->ray_dir, ray_direction);
	snapdata->view_proj = view_proj;
	copy_v2_v2(snapdata->depth_range, depth_range);
}


MINLINE float depth_get(const float co[3], const float ray_start[3], const float ray_dir[3])
{
	float dvec[3];
	sub_v3_v3v3(dvec, co, ray_start);
	return dot_v3v3(dvec, ray_dir);
}


static bool walk_parent_bvhroot_cb(const BVHTreeAxisRange *bounds, void *userdata)
{
	BVHTreeRay *ray = userdata;
	const float bbmin[3] = {bounds[0].min, bounds[1].min, bounds[2].min};
	const float bbmax[3] = {bounds[0].max, bounds[1].max, bounds[2].max};
	if (!isect_ray_aabb_v3_simple(ray->origin, ray->direction, bbmin, bbmax, &ray->radius, NULL)) {
		ray->radius = -1;
	}
	return false;
}


static bool isect_ray_bvhroot_v3(struct BVHTree *tree, const float ray_start[3], const float ray_dir[3], float *depth)
{
	BVHTreeRay ray;
	copy_v3_v3(ray.origin, ray_start);
	copy_v3_v3(ray.direction, ray_dir);

	BLI_bvhtree_walk_dfs(tree, walk_parent_bvhroot_cb, NULL, NULL, &ray);

	if (ray.radius > 0) {
		*depth = ray.radius;
		return true;
	}
	else {
		return false;
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray Cast Funcs
 * \{ */

/* Store all ray-hits
 * Support for storing all depths, not just the first (raycast 'all') */

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

		struct SnapObjectHitDepth *hit_item = hit_depth_create(
		        depth, location, normal, hit->index,
		        data->ob, data->obmat, data->ob_uuid);
		BLI_addtail(data->hit_list, hit_item);
	}
}


static bool raycastMesh(
        SnapObjectContext *sctx,
        const float ray_start[3], const float ray_dir[3],
        Object *ob, Mesh *me, float obmat[4][4], const unsigned int ob_index,
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        ListBase *r_hit_list)
{
	bool retval = false;

	if (me->totpoly == 0) {
		return retval;
	}

	float imat[4][4];
	float timat[3][3]; /* transpose inverse matrix for normals */
	float ray_start_local[3], ray_normal_local[3];
	float local_scale, local_depth, len_diff = 0.0f;

	invert_m4_m4(imat, obmat);
	transpose_m3_m4(timat, imat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_dir);

	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	/* local scale in normal direction */
	local_scale = normalize_v3(ray_normal_local);
	local_depth = *ray_depth;
	if (local_depth != BVH_RAYCAST_DIST_MAX) {
		local_depth *= local_scale;
	}

	/* Test BoundBox */
	BoundBox *bb = BKE_mesh_boundbox_get(ob);
	if (bb) {
		/* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
		if (!isect_ray_aabb_v3_simple(
		        ray_start_local, ray_normal_local, bb->vec[0], bb->vec[6], &len_diff, NULL))
		{
			return retval;
		}
	}

	SnapObjectData_Mesh *sod = NULL;

	void **sod_p;
	if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
		sod->sd.type = SNAP_MESH;
	}

	BVHTreeFromMesh *treedata = &sod->treedata;

	/* The tree is owned by the DM and may have been freed since we last used. */
	if (treedata->tree) {
		BLI_assert(treedata->cached);
		if (!bvhcache_has_tree(me->runtime.bvh_cache, treedata->tree)) {
			free_bvhtree_from_mesh(treedata);
		}
		else {
			/* Update Pointers. */
			if (treedata->vert && treedata->vert_allocated == false) {
				treedata->vert = me->mvert;
			}
			if (treedata->loop && treedata->loop_allocated == false) {
				treedata->loop = me->mloop;
			}
			if (treedata->looptri && treedata->looptri_allocated == false) {
				treedata->looptri = BKE_mesh_runtime_looptri_ensure(me);
			}
		}
	}

	if (treedata->tree == NULL) {
		BKE_bvhtree_from_mesh_get(treedata, me, BVHTREE_FROM_LOOPTRI, 4);

		if (treedata->tree == NULL) {
			return retval;
		}
	}

	/* Only use closer ray_start in case of ortho view! In perspective one, ray_start may already
	 * been *inside* boundbox, leading to snap failures (see T38409).
	 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
	 */
	if (len_diff == 0.0f) {  /* do_ray_start_correction */
		/* We *need* a reasonably valid len_diff in this case.
		 * Get the distance to bvhtree root */
		if (!isect_ray_bvhroot_v3(treedata->tree, ray_start_local, ray_normal_local, &len_diff)) {
			return retval;
		}
	}
	/* You need to make sure that ray_start is really far away,
	 * because even in the Orthografic view, in some cases,
	 * the ray can start inside the object (see T50486) */
	if (len_diff > 400.0f) {
		/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with
		 * very far away ray_start values (as returned in case of ortho view3d), see T38358.
		 */
		len_diff -= local_scale; /* make temp start point a bit away from bbox hit point. */
		madd_v3_v3fl(ray_start_local, ray_normal_local, len_diff);
		local_depth -= len_diff;
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
					*r_index = hit.index;
				}
			}
		}
	}

	return retval;
}

static bool raycastEditMesh(
        SnapObjectContext *sctx,
        const float ray_start[3], const float ray_dir[3],
        Object *ob, BMEditMesh *em, float obmat[4][4], const unsigned int ob_index,
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        ListBase *r_hit_list)
{
	bool retval = false;
	if (em->bm->totface == 0) {
		return retval;
	}

	SnapObjectData_EditMesh *sod = NULL;
	BVHTreeFromEditMesh *treedata = NULL;

	void **sod_p;
	if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
		sod->sd.type = SNAP_EDIT_MESH;
	}

	if (sod->bvh_trees[2] == NULL) {
		sod->bvh_trees[2] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
	}
	treedata = sod->bvh_trees[2];

	if (treedata->tree == NULL) {
		BLI_bitmap *elem_mask = NULL;
		int looptri_num_active = -1;

		if (sctx->callbacks.edit_mesh.test_face_fn) {
			elem_mask = BLI_BITMAP_NEW(em->tottri, __func__);
			looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
			        em->bm, elem_mask,
			        sctx->callbacks.edit_mesh.test_face_fn, sctx->callbacks.edit_mesh.user_data);
		}
		bvhtree_from_editmesh_looptri_ex(treedata, em, elem_mask, looptri_num_active, 0.0f, 4, 6, NULL);

		if (elem_mask) {
			MEM_freeN(elem_mask);
		}
		if (treedata->tree == NULL) {
			return retval;
		}
	}

	float imat[4][4];
	float timat[3][3]; /* transpose inverse matrix for normals */
	float ray_normal_local[3], ray_start_local[3], len_diff = 0.0f;

	invert_m4_m4(imat, obmat);
	transpose_m3_m4(timat, imat);

	copy_v3_v3(ray_normal_local, ray_dir);
	mul_mat3_m4_v3(imat, ray_normal_local);

	copy_v3_v3(ray_start_local, ray_start);
	mul_m4_v3(imat, ray_start_local);

	/* local scale in normal direction */
	float local_scale = normalize_v3(ray_normal_local);
	float local_depth = *ray_depth;
	if (local_depth != BVH_RAYCAST_DIST_MAX) {
		local_depth *= local_scale;
	}

	/* Only use closer ray_start in case of ortho view! In perspective one, ray_start
	 * may already been *inside* boundbox, leading to snap failures (see T38409).
	 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
	 */
	if (sctx->use_v3d && !((RegionView3D *)sctx->v3d_data.ar->regiondata)->is_persp) {  /* do_ray_start_correction */
		/* We *need* a reasonably valid len_diff in this case.
		 * Get the distance to bvhtree root */
		if (!isect_ray_bvhroot_v3(treedata->tree, ray_start_local, ray_normal_local, &len_diff)) {
			return retval;
		}
		/* You need to make sure that ray_start is really far away,
		 * because even in the Orthografic view, in some cases,
		 * the ray can start inside the object (see T50486) */
		if (len_diff > 400.0f) {
			/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with
			 * very far away ray_start values (as returned in case of ortho view3d), see T38358.
			 */
			len_diff -= local_scale; /* make temp start point a bit away from bbox hit point. */
			madd_v3_v3fl(ray_start_local, ray_normal_local, len_diff);
			local_depth -= len_diff;
		}
		else len_diff = 0.0f;
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

	return retval;
}


/**
 * \param use_obedit: Uses the coordinates of BMesh (if any) to do the snapping;
 *
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static bool raycastObj(
        SnapObjectContext *sctx,
        const float ray_start[3], const float ray_dir[3],
        Object *ob, float obmat[4][4], const unsigned int ob_index,
        bool use_obedit,
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
	bool retval = false;

	switch (ob->type) {
		case OB_MESH:
			if (use_obedit) {
				BMEditMesh *em = BKE_editmesh_from_object(ob);
				retval = raycastEditMesh(
				        sctx,
				        ray_start, ray_dir,
				        ob, em, obmat, ob_index,
				        ray_depth, r_loc, r_no, r_index, r_hit_list);
			}
			else {
				retval = raycastMesh(
				        sctx,
				        ray_start, ray_dir,
				        ob, ob->data, obmat, ob_index,
				        ray_depth, r_loc, r_no, r_index, r_hit_list);
			}
			break;
	}

	if (retval) {
		if (r_ob) {
			*r_ob = ob;
		}
		if (r_obmat) {
			copy_m4_m4(r_obmat, obmat);
		}
		return true;
	}

	return false;
}


struct RaycastObjUserData {
	const float *ray_start;
	const float *ray_dir;
	unsigned int ob_index;
	/* read/write args */
	float *ray_depth;
	/* return args */
	float *r_loc;
	float *r_no;
	int *r_index;
	Object **r_ob;
	float (*r_obmat)[4];
	ListBase *r_hit_list;
	bool ret;
};

static void raycast_obj_cb(SnapObjectContext *sctx, bool is_obedit, Object *ob, float obmat[4][4], void *data)
{
	struct RaycastObjUserData *dt = data;
	dt->ret |= raycastObj(
	        sctx,
	        dt->ray_start, dt->ray_dir,
	        ob, obmat, dt->ob_index++, is_obedit,
	        dt->ray_depth,
	        dt->r_loc, dt->r_no, dt->r_index,
	        dt->r_ob, dt->r_obmat,
	        dt->r_hit_list);
}

/**
 * Main RayCast Function
 * ======================
 *
 * Walks through all objects in the scene to find the `hit` on object surface.
 *
 * \param sctx: Snap context to store data.
 * \param snap_select : from enum eSnapSelect.
 * \param use_object_edit_cage : Uses the coordinates of BMesh(if any) to do the snapping.
 * \param obj_list: List with objects to snap (created in `create_object_list`).
 *
 * Read/Write Args
 * ---------------
 *
 * \param ray_depth: maximum depth allowed for r_co, elements deeper than this value will be ignored.
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
static bool raycastObjects(
        SnapObjectContext *sctx,
        const float ray_start[3], const float ray_dir[3],
        const eSnapSelect snap_select, const bool use_object_edit_cage,
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(sctx->depsgraph);
	Object *obedit = use_object_edit_cage ? OBEDIT_FROM_VIEW_LAYER(view_layer) : NULL;

	struct RaycastObjUserData data = {
		.ray_start = ray_start,
		.ray_dir = ray_dir,
		.ob_index = 0,
		.ray_depth = ray_depth,
		.r_loc = r_loc,
		.r_no = r_no,
		.r_index = r_index,
		.r_ob = r_ob,
		.r_obmat = r_obmat,
		.r_hit_list = r_hit_list,
		.ret = false,
	};

	iter_snap_objects(sctx, snap_select, obedit, raycast_obj_cb, &data);

	return data.ret;
}


/** \} */

/* -------------------------------------------------------------------- */
/** Snap Nearest utilities
 * \{ */

static void cb_mvert_co_get(
        const int index, const float **co, const BVHTreeFromMesh *data)
{
	*co = data->vert[index].co;
}

static void cb_bvert_co_get(
        const int index, const float **co, const BMEditMesh *data)
{
	BMVert *eve = BM_vert_at_index(data->bm, index);
	*co = eve->co;
}

static void cb_mvert_no_copy(
        const int index, float r_no[3], const BVHTreeFromMesh *data)
{
	const MVert *vert = data->vert + index;

	normal_short_to_float_v3(r_no, vert->no);
}

static void cb_bvert_no_copy(
        const int index, float r_no[3], const BMEditMesh *data)
{
	BMVert *eve = BM_vert_at_index(data->bm, index);

	copy_v3_v3(r_no, eve->no);
}

static void cb_medge_verts_get(
        const int index, int v_index[2], const BVHTreeFromMesh *data)
{
	const MEdge *edge = &data->edge[index];

	v_index[0] = edge->v1;
	v_index[1] = edge->v2;

}

static void cb_bedge_verts_get(
        const int index, int v_index[2], const BMEditMesh *data)
{
	BMEdge *eed = BM_edge_at_index(data->bm, index);

	v_index[0] = BM_elem_index_get(eed->v1);
	v_index[1] = BM_elem_index_get(eed->v2);
}

static void cb_mlooptri_edges_get(
        const int index, int v_index[3], const BVHTreeFromMesh *data)
{
	const MEdge *medge = data->edge;
	const MLoop *mloop = data->loop;
	const MLoopTri *lt = &data->looptri[index];
	for (int j = 2, j_next = 0; j_next < 3; j = j_next++) {
		const MEdge *ed = &medge[mloop[lt->tri[j]].e];
		unsigned int tri_edge[2] = {mloop[lt->tri[j]].v, mloop[lt->tri[j_next]].v};
		if (ELEM(ed->v1, tri_edge[0], tri_edge[1]) &&
		    ELEM(ed->v2, tri_edge[0], tri_edge[1]))
		{
			//printf("real edge found\n");
			v_index[j] = mloop[lt->tri[j]].e;
		}
		else
			v_index[j] = -1;
	}
}

static void cb_mlooptri_verts_get(
        const int index, int v_index[3], const BVHTreeFromMesh *data)
{
	const MLoop *loop = data->loop;
	const MLoopTri *looptri = &data->looptri[index];

	v_index[0] = loop[looptri->tri[0]].v;
	v_index[1] = loop[looptri->tri[1]].v;
	v_index[2] = loop[looptri->tri[2]].v;
}

static bool test_projected_vert_dist(
        const struct DistProjectedAABBPrecalc *neasrest_precalc,
        const float depth_range[2],
        const bool is_persp, const float co[3],
        float *dist_px_sq, float r_co[3])
{
	float w;
	if (is_persp) {
		w = mul_project_m4_v3_zfac(neasrest_precalc->pmat, co);
		if (w < depth_range[0] || w > depth_range[1]) {
			return false;
		}
	}

	float co2d[2] = {
		(dot_m4_v3_row_x(neasrest_precalc->pmat, co) + neasrest_precalc->pmat[3][0]),
		(dot_m4_v3_row_y(neasrest_precalc->pmat, co) + neasrest_precalc->pmat[3][1]),
	};

	if (is_persp) {
		mul_v2_fl(co2d, 1.0f / w);
	}

	const float dist_sq = len_squared_v2v2(neasrest_precalc->mval, co2d);
	if (dist_sq < *dist_px_sq) {
		copy_v3_v3(r_co, co);
		*dist_px_sq = dist_sq;
		return true;
	}
	return false;
}

static bool test_projected_edge_dist(
        const struct DistProjectedAABBPrecalc *neasrest_precalc,
        const float depth_range[2], const bool is_persp,
        const float va[3], const float vb[3],
        float *dist_px_sq, float r_co[3])
{

	float near_co[3], dummy_depth;
	dist_squared_ray_to_seg_v3(
	        neasrest_precalc->ray_origin,
	        neasrest_precalc->ray_direction,
	        va, vb, near_co, &dummy_depth);

	return test_projected_vert_dist(
	        neasrest_precalc, depth_range,
	        is_persp, near_co, dist_px_sq, r_co);
}

/** \} */

/* -------------------------------------------------------------------- */
/** Walk DFS
 * \{ */

typedef void (*Nearest2DGetVertCoCallback)(const int index, const float **co, void *data);
typedef void (*Nearest2DGetEdgeVertsCallback)(const int index, int v_index[2], void *data);
typedef void (*Nearest2DGetTriVertsCallback)(const int index, int v_index[3], void *data);
typedef void (*Nearest2DGetTriEdgesCallback)(const int index, int e_index[3], void *data); /* Equal the previous one */
typedef void (*Nearest2DCopyVertNoCallback)(const int index, float r_no[3], void *data);

typedef struct Nearest2dUserData {
	bool is_persp;
	float depth_range[2];
	short snap_to;

	void *userdata;
	Nearest2DGetVertCoCallback get_vert_co;
	Nearest2DGetEdgeVertsCallback get_edge_verts_index;
	Nearest2DGetTriVertsCallback get_tri_verts_index;
	Nearest2DGetTriEdgesCallback get_tri_edges_index;
	Nearest2DCopyVertNoCallback copy_vert_no;

} Nearest2dUserData;


static void cb_walk_leaf_snap_vert(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	const float *co;
	data->get_vert_co(index, &co, data->userdata);

	if (test_projected_vert_dist(
	        precalc,
	        data->depth_range,
	        data->is_persp,
	        co,
	        &nearest->dist_sq,
	        nearest->co))
	{
		data->copy_vert_no(index, nearest->no, data->userdata);
		nearest->index = index;
	}
}

static void cb_walk_leaf_snap_edge(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	int vindex[2];
	data->get_edge_verts_index(index, vindex, data->userdata);

	if (data->snap_to == SCE_SNAP_MODE_EDGE) {
		const float *v_pair[2];
		data->get_vert_co(vindex[0], &v_pair[0], data->userdata);
		data->get_vert_co(vindex[1], &v_pair[1], data->userdata);

		if (test_projected_edge_dist(
		        precalc,
		        data->depth_range,
		        data->is_persp,
		        v_pair[0], v_pair[1],
		        &nearest->dist_sq,
		        nearest->co))
		{
			sub_v3_v3v3(nearest->no, v_pair[0], v_pair[1]);
			nearest->index = index;
		}
	}
	else {
		for (int i = 0; i < 2; i++) {
			if (vindex[i] == nearest->index) {
				continue;
			}
			cb_walk_leaf_snap_vert(userdata, vindex[i], precalc, nearest);
		}
	}
}

static void cb_walk_leaf_snap_tri(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	if (data->snap_to == SCE_SNAP_MODE_EDGE) {
		int eindex[3];
		data->get_tri_edges_index(index, eindex, data->userdata);
		for (int i = 0; i < 3; i++) {
			if (eindex[i] != -1) {
				if (eindex[i] == nearest->index) {
					continue;
				}
				cb_walk_leaf_snap_edge(userdata, eindex[i], precalc, nearest);
			}
		}
	}
	else {
		int vindex[3];
		data->get_tri_verts_index(index, vindex, data->userdata);
		for (int i = 0; i < 3; i++) {
			if (vindex[i] == nearest->index) {
				continue;
			}
			cb_walk_leaf_snap_vert(userdata, vindex[i], precalc, nearest);
		}
	}
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

	if (snapdata->snap_to == SCE_SNAP_MODE_FACE) { /* Currently only edge and vert */
		return retval;
	}

	float lpmat[4][4], dist_px_sq = SQUARE(*dist_px);
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	/* Test BoundBox */
	BoundBox *bb = BKE_armature_boundbox_get(ob);
	if (bb) {
		bool dummy[3];
		/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see: T46099, T46816 */
		float bb_dist_px_sq = dist_squared_to_projected_aabb(
			&neasrest_precalc, bb->vec[0], bb->vec[6], dummy);

		if (bb_dist_px_sq > dist_px_sq) {
			return retval;
		}
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;

	if (arm->edbo) {
		for (EditBone *eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_ROOTSEL | BONE_TIPSEL)) == 0) {
					switch (snapdata->snap_to) {
						case SCE_SNAP_MODE_VERTEX:
							retval |= test_projected_vert_dist(
							        &neasrest_precalc, snapdata->depth_range,
							        is_persp, eBone->head, &dist_px_sq, r_loc);
							retval |= test_projected_vert_dist(
							        &neasrest_precalc, snapdata->depth_range,
							        is_persp, eBone->tail, &dist_px_sq, r_loc);
							break;
						case SCE_SNAP_MODE_EDGE:
							retval |= test_projected_edge_dist(
							        &neasrest_precalc, snapdata->depth_range,
							        is_persp, eBone->head, eBone->tail,
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
						        &neasrest_precalc, snapdata->depth_range,
						        is_persp, head_vec, &dist_px_sq, r_loc);
						retval |= test_projected_vert_dist(
						        &neasrest_precalc, snapdata->depth_range,
						        is_persp, tail_vec, &dist_px_sq, r_loc);
						break;
					case SCE_SNAP_MODE_EDGE:
						retval |= test_projected_edge_dist(
						        &neasrest_precalc, snapdata->depth_range,
						        is_persp, head_vec, tail_vec,
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
        Object *ob, float obmat[4][4], bool use_obedit,
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

	Curve *cu = ob->data;
	float dist_px_sq = SQUARE(*dist_px);

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	/* Test BoundBox */
	BoundBox *bb = BKE_curve_boundbox_get(ob);
	if (bb) {
		bool dummy[3];
		/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see: T46099, T46816 */
		float bb_dist_px_sq = dist_squared_to_projected_aabb(
		        &neasrest_precalc, bb->vec[0], bb->vec[6], dummy);

		if (bb_dist_px_sq > dist_px_sq) {
			return retval;
		}
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;

	for (Nurb *nu = (use_obedit ? cu->editnurb->nurbs.first : cu->nurb.first); nu; nu = nu->next) {
		for (int u = 0; u < nu->pntsu; u++) {
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_VERTEX:
				{
					if (use_obedit) {
						if (nu->bezt) {
							/* don't snap to selected (moving) or hidden */
							if (nu->bezt[u].f2 & SELECT || nu->bezt[u].hide != 0) {
								break;
							}
							retval |= test_projected_vert_dist(
							        &neasrest_precalc, snapdata->depth_range,
							        is_persp, nu->bezt[u].vec[1], &dist_px_sq,
							        r_loc);
							/* don't snap if handle is selected (moving), or if it is aligning to a moving handle */
							if (!(nu->bezt[u].f1 & SELECT) &&
							    !(nu->bezt[u].h1 & HD_ALIGN && nu->bezt[u].f3 & SELECT))
							{
								retval |= test_projected_vert_dist(
								        &neasrest_precalc, snapdata->depth_range,
								        is_persp, nu->bezt[u].vec[0], &dist_px_sq,
								        r_loc);
							}
							if (!(nu->bezt[u].f3 & SELECT) &&
							    !(nu->bezt[u].h2 & HD_ALIGN && nu->bezt[u].f1 & SELECT))
							{
								retval |= test_projected_vert_dist(
								        &neasrest_precalc, snapdata->depth_range,
								        is_persp, nu->bezt[u].vec[2], &dist_px_sq,
								        r_loc);
							}
						}
						else {
							/* don't snap to selected (moving) or hidden */
							if (nu->bp[u].f1 & SELECT || nu->bp[u].hide != 0) {
								break;
							}
							retval |= test_projected_vert_dist(
							        &neasrest_precalc, snapdata->depth_range,
							        is_persp, nu->bp[u].vec, &dist_px_sq,
							        r_loc);
						}
					}
					else {
						/* curve is not visible outside editmode if nurb length less than two */
						if (nu->pntsu > 1) {
							if (nu->bezt) {
								retval |= test_projected_vert_dist(
								        &neasrest_precalc, snapdata->depth_range,
								        is_persp, nu->bezt[u].vec[1], &dist_px_sq,
								        r_loc);
							}
							else {
								retval |= test_projected_vert_dist(
								        &neasrest_precalc, snapdata->depth_range,
								        is_persp, nu->bp[u].vec, &dist_px_sq,
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
			struct DistProjectedAABBPrecalc neasrest_precalc;
			dist_squared_to_projected_aabb_precalc(
			        &neasrest_precalc, snapdata->pmat, snapdata->win_size, snapdata->mval);

			bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
			float dist_px_sq = SQUARE(*dist_px);
			float co[3];
			copy_v3_v3(co, obmat[3]);
			if (test_projected_vert_dist(
			        &neasrest_precalc, snapdata->depth_range,
			        is_persp, co, &dist_px_sq, r_loc))
			{
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
			struct DistProjectedAABBPrecalc neasrest_precalc;
			dist_squared_to_projected_aabb_precalc(
			        &neasrest_precalc, snapdata->pmat, snapdata->win_size, snapdata->mval);

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
					        &neasrest_precalc, snapdata->depth_range,
					        is_persp, bundle_pos, &dist_px_sq, r_loc);
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

static bool snapMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, Mesh *me, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3])
{
	bool retval = false;

	if (snapdata->snap_to == SCE_SNAP_MODE_EDGE) {
		if (me->totedge == 0) {
			return retval;
		}
	}
	else {
		if (me->totvert == 0) {
			return retval;
		}
	}

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	float dist_px_sq = SQUARE(*dist_px);

	/* Test BoundBox */
	BoundBox *bb = BKE_mesh_boundbox_get(ob);
	if (bb) {
		/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see: T46099, T46816 */

		struct DistProjectedAABBPrecalc data_precalc;
		dist_squared_to_projected_aabb_precalc(
		        &data_precalc, lpmat, snapdata->win_size, snapdata->mval);

		bool dummy[3];
		float bb_dist_px_sq = dist_squared_to_projected_aabb(
		        &data_precalc, bb->vec[0], bb->vec[6], dummy);

		if (bb_dist_px_sq > dist_px_sq) {
			return retval;
		}
	}

	SnapObjectData_Mesh *sod = NULL;

	void **sod_p;
	if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
		sod->sd.type = SNAP_MESH;
		/* start assuming that it has each of these element types */
		sod->has_looptris = true;
		sod->has_loose_edge = true;
		sod->has_loose_vert = true;
	}

	BVHTreeFromMesh *treedata, dummy_treedata;
	BVHTree **bvhtree;
	treedata = &sod->treedata;
	bvhtree = sod->bvhtree;

	/* the tree is owned by the DM and may have been freed since we last used! */
	if ((sod->has_looptris   && treedata->tree && !bvhcache_has_tree(me->runtime.bvh_cache, treedata->tree)) ||
	    (sod->has_loose_edge && bvhtree[0]     && !bvhcache_has_tree(me->runtime.bvh_cache, bvhtree[0]))     ||
	    (sod->has_loose_vert && bvhtree[1]     && !bvhcache_has_tree(me->runtime.bvh_cache, bvhtree[1])))
	{
		BLI_assert(!treedata->tree || !bvhcache_has_tree(me->runtime.bvh_cache, treedata->tree));
		BLI_assert(!bvhtree[0]     || !bvhcache_has_tree(me->runtime.bvh_cache, bvhtree[0]));
		BLI_assert(!bvhtree[1]     || !bvhcache_has_tree(me->runtime.bvh_cache, bvhtree[1]));

		free_bvhtree_from_mesh(treedata);
		bvhtree[0] = NULL;
		bvhtree[1] = NULL;
	}

	if (sod->has_looptris && treedata->tree == NULL) {
		BKE_bvhtree_from_mesh_get(treedata, me, BVHTREE_FROM_LOOPTRI, 4);
		sod->has_looptris = (treedata->tree != NULL);
		if (sod->has_looptris) {
			/* Make sure that the array of edges is referenced in the callbacks. */
			treedata->edge = me->medge; /* CustomData_get_layer(&me->edata, CD_MEDGE);? */
		}
	}
	if (sod->has_loose_edge && bvhtree[0] == NULL) {
		bvhtree[0] = BKE_bvhtree_from_mesh_get(&dummy_treedata, me, BVHTREE_FROM_LOOSEEDGES, 2);
		sod->has_loose_edge = bvhtree[0] != NULL;

		if (sod->has_loose_edge) {
			BLI_assert(treedata->vert_allocated == false);
			treedata->vert = dummy_treedata.vert;
			treedata->vert_allocated = dummy_treedata.vert_allocated;

			BLI_assert(treedata->edge_allocated == false);
			treedata->edge = dummy_treedata.edge;
			treedata->edge_allocated = dummy_treedata.edge_allocated;
		}
	}
	if (snapdata->snap_to == SCE_SNAP_MODE_VERTEX) {
		if (sod->has_loose_vert && bvhtree[1] == NULL) {
			bvhtree[1] = BKE_bvhtree_from_mesh_get(&dummy_treedata, me, BVHTREE_FROM_LOOSEVERTS, 2);
			sod->has_loose_vert = bvhtree[1] != NULL;

			if (sod->has_loose_vert) {
				BLI_assert(treedata->vert_allocated == false);
				treedata->vert = dummy_treedata.vert;
				treedata->vert_allocated = dummy_treedata.vert_allocated;
			}
		}
	}
	else {
		/* Not necessary, just to keep the data more consistent. */
		sod->has_loose_vert = false;
	}

	/* Update pointers. */
	if (treedata->vert_allocated == false) {
		treedata->vert = me->mvert; /* CustomData_get_layer(&me->vdata, CD_MVERT);? */
	}
	if (treedata->tree || bvhtree[0]) {
		if (treedata->edge_allocated == false) {
			/* If raycast has been executed before, `treedata->edge` can be NULL. */
			treedata->edge = me->medge; /* CustomData_get_layer(&me->edata, CD_MEDGE);? */
		}
		if (treedata->loop && treedata->loop_allocated == false) {
			treedata->loop = me->mloop; /* CustomData_get_layer(&me->edata, CD_MLOOP);? */
		}
		if (treedata->looptri && treedata->looptri_allocated == false) {
			treedata->looptri = BKE_mesh_runtime_looptri_ensure(me);
		}
	}

	/* Warning: the depth_max is currently being used only in perspective view.
	 * It is not correct to limit the maximum depth for elements obtained with nearest
	 * since this limitation depends on the normal and the size of the occlusion face.
	 * And more... ray_depth is being confused with Z-depth here... (varies only the precision) */
	const float ray_depth_max_global = *ray_depth + snapdata->depth_range[0];

	Nearest2dUserData neasrest2d = {
		.is_persp             = snapdata->view_proj == VIEW_PROJ_PERSP,
		.depth_range          = {snapdata->depth_range[0], ray_depth_max_global},
		.snap_to              = snapdata->snap_to,
		.userdata             = treedata,
		.get_vert_co          = (Nearest2DGetVertCoCallback)cb_mvert_co_get,
		.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_medge_verts_get,
		.get_tri_verts_index  = (Nearest2DGetTriVertsCallback)cb_mlooptri_verts_get,
		.get_tri_edges_index  = (Nearest2DGetTriEdgesCallback)cb_mlooptri_edges_get,
		.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_mvert_no_copy,
	};

	BVHTreeNearest nearest = {
		.index = -1,
		.dist_sq = dist_px_sq,
	};

	if (bvhtree[1]) {
		/* snap to loose verts */
		BLI_bvhtree_find_nearest_projected(
		        bvhtree[1], lpmat, snapdata->win_size, snapdata->mval,
		        NULL, 0, &nearest, cb_walk_leaf_snap_vert, &neasrest2d);
	}

	if (bvhtree[0]) {
		/* snap to loose edges */
		BLI_bvhtree_find_nearest_projected(
		        bvhtree[0], lpmat, snapdata->win_size, snapdata->mval,
		        NULL, 0, &nearest, cb_walk_leaf_snap_edge, &neasrest2d);
	}

	if (treedata->tree) {
		/* snap to looptris */
		BLI_bvhtree_find_nearest_projected(
		        treedata->tree, lpmat, snapdata->win_size, snapdata->mval,
		        NULL, 0, &nearest, cb_walk_leaf_snap_tri, &neasrest2d);
	}

	if (nearest.index != -1) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);
		if (r_no) {
			copy_v3_v3(r_no, nearest.no);
			mul_m3_v3(timat, r_no);
			normalize_v3(r_no);
		}
		*dist_px = sqrtf(nearest.dist_sq);
		*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);

		retval = true;
	}

	return retval;
}

static bool snapEditMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, BMEditMesh *em, float obmat[4][4],
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3])
{
	bool retval = false;

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

	SnapObjectData_EditMesh *sod = NULL;
	BVHTreeFromEditMesh *treedata = NULL;

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

	if (treedata) {
		if (treedata->tree == NULL) {
			BLI_bitmap *elem_mask = NULL;
			switch (snapdata->snap_to) {
				case SCE_SNAP_MODE_EDGE:
				{
					int edges_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_edge_fn) {
						elem_mask = BLI_BITMAP_NEW(em->bm->totedge, __func__);
						edges_num_active = BM_iter_mesh_bitmap_from_filter(
						        BM_EDGES_OF_MESH, em->bm, elem_mask,
						        (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_edge_fn,
						        sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_edges_ex(treedata, em, elem_mask, edges_num_active, 0.0f, 2, 6);
					break;
				}
				case SCE_SNAP_MODE_VERTEX:
				{
					int verts_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_vert_fn) {
						elem_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
						verts_num_active = BM_iter_mesh_bitmap_from_filter(
						        BM_VERTS_OF_MESH, em->bm, elem_mask,
						        (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_vert_fn,
						        sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_verts_ex(treedata, em, elem_mask, verts_num_active, 0.0f, 2, 6);
					break;
				}
			}
			if (elem_mask) {
				MEM_freeN(elem_mask);
			}
		}
		if (treedata->tree == NULL) {
			return retval;
		}
	}
	else {
		return retval;
	}

	Nearest2dUserData neasrest2d = {
		.is_persp             = snapdata->view_proj == VIEW_PROJ_PERSP,
		.depth_range          = {snapdata->depth_range[0], *ray_depth + snapdata->depth_range[0]},
		.snap_to              = snapdata->snap_to,
		.userdata             = treedata->em,
		.get_vert_co          = (Nearest2DGetVertCoCallback)cb_bvert_co_get,
		.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_bedge_verts_get,
		.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_bvert_no_copy,
	};

	BVHTreeNearest nearest = {
		.index = -1,
		.dist_sq = SQUARE(*dist_px),
	};

	BVHTree_NearestProjectedCallback cb_walk_leaf =
	        (snapdata->snap_to == SCE_SNAP_MODE_VERTEX) ?
	        cb_walk_leaf_snap_vert : cb_walk_leaf_snap_edge;

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
	BLI_bvhtree_find_nearest_projected(
	        treedata->tree, lpmat, snapdata->win_size, snapdata->mval,
	        NULL, 0, &nearest, cb_walk_leaf, &neasrest2d);

	if (nearest.index != -1) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);
		if (r_no) {
			copy_v3_v3(r_no, nearest.no);
			mul_m3_v3(timat, r_no);
			normalize_v3(r_no);
		}
		*dist_px = sqrtf(nearest.dist_sq);
		*ray_depth = depth_get(r_loc, snapdata->ray_start, snapdata->ray_dir);

		retval = true;
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
        Object *ob, float obmat[4][4], bool use_obedit,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3],
        Object **r_ob, float r_obmat[4][4])
{
	BLI_assert(snapdata->snap_to != SCE_SNAP_MODE_FACE);
	bool retval = false;

	switch (ob->type) {
		case OB_MESH:
			if (use_obedit) {
				BMEditMesh *em = BKE_editmesh_from_object(ob);
				retval = snapEditMesh(
				        sctx, snapdata, ob, em, obmat,
				        ray_depth, dist_px,
				        r_loc, r_no);
			}
			else {
				retval = snapMesh(
				        sctx, snapdata, ob, ob->data, obmat,
				        ray_depth, dist_px,
				        r_loc, r_no);
			}
			break;

		case OB_ARMATURE:
			retval = snapArmature(
			        snapdata,
			        ob, ob->data, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
			break;

		case OB_CURVE:
			retval = snapCurve(
			        snapdata,
			        ob, obmat, use_obedit,
			        ray_depth, dist_px,
			        r_loc, r_no);
			break;

		case OB_EMPTY:
			retval = snapEmpty(
			        snapdata, ob, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
			break;

		case OB_CAMERA:
			retval = snapCamera(
			        sctx, snapdata, ob, obmat,
			        ray_depth, dist_px,
			        r_loc, r_no);
			break;
	}

	if (retval) {
		if (r_ob) {
			*r_ob = ob;
		}
		if (r_obmat) {
			copy_m4_m4(r_obmat, obmat);
		}
		return true;
	}

	return false;
}


struct SnapObjUserData {
	SnapData *snapdata;
	/* read/write args */
	float *ray_depth;
	float *dist_px;
	/* return args */
	float *r_loc;
	float *r_no;
	Object **r_ob;
	float (*r_obmat)[4];
	bool ret;
};

static void sanp_obj_cb(SnapObjectContext *sctx, bool is_obedit, Object *ob, float obmat[4][4], void *data)
{
	struct SnapObjUserData *dt = data;
	dt->ret |= snapObject(
	        sctx, dt->snapdata,
	        ob, obmat, is_obedit,
	        /* read/write args */
	        dt->ray_depth, dt->dist_px,
	        /* return args */
	        dt->r_loc, dt->r_no,
	        dt->r_ob, dt->r_obmat);
}


/**
 * Main Snapping Function
 * ======================
 *
 * Walks through all objects in the scene to find the closest snap element ray.
 *
 * \param sctx: Snap context to store data.
 * \param snapdata: struct generated in `get_snapdata`.
 * \param snap_select : from enum eSnapSelect.
 * \param use_object_edit_cage : Uses the coordinates of BMesh(if any) to do the snapping.
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
 *
 */
static bool snapObjectsRay(
        SnapObjectContext *sctx, SnapData *snapdata,
        const eSnapSelect snap_select, const bool use_object_edit_cage,
        /* read/write args */
        float *ray_depth, float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3],
        Object **r_ob, float r_obmat[4][4])
{
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(sctx->depsgraph);
	Object *obedit = use_object_edit_cage ? OBEDIT_FROM_VIEW_LAYER(view_layer) : NULL;

	struct SnapObjUserData data = {
		.snapdata = snapdata,
		.ray_depth = ray_depth,
		.dist_px = dist_px,
		.r_loc = r_loc,
		.r_no = r_no,
		.r_ob = r_ob,
		.r_obmat = r_obmat,
		.ret = false,
	};

	iter_snap_objects(sctx, snap_select, obedit, sanp_obj_cb, &data);

	return data.ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *ED_transform_snap_object_context_create(
        Main *bmain, Scene *scene, Depsgraph *depsgraph, int flag)
{
	SnapObjectContext *sctx = MEM_callocN(sizeof(*sctx), __func__);

	sctx->flag = flag;

	sctx->bmain = bmain;
	sctx->scene = scene;
	sctx->depsgraph = depsgraph;

	sctx->cache.object_map = BLI_ghash_ptr_new(__func__);
	sctx->cache.mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

	return sctx;
}

SnapObjectContext *ED_transform_snap_object_context_create_view3d(
        Main *bmain, Scene *scene, Depsgraph *depsgraph, int flag,
        /* extra args for view3d */
        const ARegion *ar, const View3D *v3d)
{
	SnapObjectContext *sctx = ED_transform_snap_object_context_create(bmain, scene, depsgraph, flag);

	sctx->use_v3d = true;
	sctx->v3d_data.ar = ar;
	sctx->v3d_data.v3d = v3d;

	return sctx;
}

static void snap_object_data_free(void *sod_v)
{
	switch (((SnapObjectData *)sod_v)->type) {
		case SNAP_MESH:
		{
			SnapObjectData_Mesh *sod = sod_v;
			if (sod->treedata.tree) {
				free_bvhtree_from_mesh(&sod->treedata);
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
	BLI_ghash_free(sctx->cache.object_map, NULL, snap_object_data_free);
	BLI_memarena_free(sctx->cache.mem_arena);

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
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3],
        float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	return raycastObjects(
	        sctx,
	        ray_start, ray_normal,
	        params->snap_select, params->use_object_edit_cage,
	        ray_depth, r_loc, r_no, r_index, r_ob, r_obmat, NULL);
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
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3],
        float ray_depth, bool sort,
        ListBase *r_hit_list)
{
	if (ray_depth == -1.0f) {
		ray_depth = BVH_RAYCAST_DIST_MAX;
	}

#ifdef DEBUG
	float ray_depth_prev = ray_depth;
#endif

	bool retval = raycastObjects(
	        sctx,
	        ray_start, ray_normal,
	        params->snap_select, params->use_object_edit_cage,
	        &ray_depth, NULL, NULL, NULL, NULL, NULL,
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
		const float dist_px_orig = dist_px ? *dist_px : 0;
		for (int i = 2; i >= 0; i--) {
			if (snap_to_flag & (1 << i)) {
				if (i == 0) {
					BLI_assert(dist_px != NULL);
					*dist_px = dist_px_orig;
				}
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
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	const ARegion *ar = sctx->v3d_data.ar;
	const RegionView3D *rv3d = ar->regiondata;

	float ray_origin[3], ray_end[3], ray_start[3], ray_normal[3], depth_range[2];

	ED_view3d_win_to_origin(ar, mval, ray_origin);
	ED_view3d_win_to_vector(ar, mval, ray_normal);

	ED_view3d_clip_range_get(
	        sctx->depsgraph,
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

	if (snap_to == SCE_SNAP_MODE_FACE) {
		return raycastObjects(
		        sctx,
		        ray_start, ray_normal,
		        params->snap_select, params->use_object_edit_cage,
		        ray_depth, r_loc, r_no, r_index, r_ob, r_obmat, NULL);
	}
	else {
		SnapData snapdata;
		const enum eViewProj view_proj = rv3d->is_persp ?
		                                 VIEW_PROJ_PERSP : VIEW_PROJ_ORTHO;

		snap_data_set(
		        &snapdata, ar, snap_to, view_proj, mval,
		        ray_start, ray_normal, depth_range);

		return snapObjectsRay(
		        sctx, &snapdata,
		        params->snap_select, params->use_object_edit_cage,
		        ray_depth, dist_px, r_loc, r_no, r_ob, r_obmat);
	}
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
	        r_loc, r_no, NULL,
	        NULL, NULL);
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
	        sctx->depsgraph,
	        sctx->v3d_data.ar, sctx->v3d_data.v3d,
	        mval, NULL, ray_normal, ray_start, true))
	{
		return false;
	}

	return ED_transform_snap_object_project_ray_all(
	        sctx,
	        params,
	        ray_start, ray_normal, ray_depth, sort,
	        r_hit_list);
}

/** \} */
