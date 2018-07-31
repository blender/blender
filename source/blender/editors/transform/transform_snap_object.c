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
#include "BKE_mesh_runtime.h"

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

#define MAX_CLIPPLANE_LEN 3

enum eViewProj {
	VIEW_PROJ_NONE = -1,
	VIEW_PROJ_ORTHO = 0,
	VIEW_PROJ_PERSP = -1,
};

typedef struct SnapData {
	short snap_to_flag;
	float mval[2];
	float pmat[4][4]; /* perspective matrix */
	float win_size[2];/* win x and y */
	enum eViewProj view_proj;
	float clip_plane[MAX_CLIPPLANE_LEN][4];
	short clip_plane_len;
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

	 /* It's like a boundbox. It is tested first to avoid
	  * to create a bvhtree for all the edited objects. */
	float min[3], max[3];

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

static void min_max_from_bmesh(
        BMesh *bm, float r_min[3], float r_max[3])
{
	BMIter iter;
	BMVert *eve;

	INIT_MINMAX(r_min, r_max);
	BM_ITER_MESH(eve, &iter, bm, BM_VERTS_OF_MESH) {
		minmax_v3v3_v3(r_min, r_max, eve->co);
	}
}

static SnapObjectData_Mesh *snap_object_data_mesh_create(SnapObjectContext *sctx)
{
	SnapObjectData_Mesh *sod = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
	sod->sd.type = SNAP_MESH;
	/* start assuming that it has each of these element types */
	sod->has_looptris = true;
	sod->has_loose_edge = true;
	sod->has_loose_vert = true;

	return sod;
}

static SnapObjectData_EditMesh *snap_object_data_editmesh_create(SnapObjectContext *sctx, BMesh *bm)
{
	SnapObjectData_EditMesh *sod = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
	sod->sd.type = SNAP_EDIT_MESH;
	min_max_from_bmesh(bm, sod->min, sod->max);

	return sod;
}

/**
 * Walks through all objects in the scene to create the list of objets to snap.
 *
 * \param sctx: Snap context to store data.
 * \param snap_select : from enum eSnapSelect.
 * \param obedit : Object Edited to use its coordinates of BMesh(if any) to do the snapping.
 */
static void iter_snap_objects(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        IterSnapObjsCallback sob_callback,
        void *data)
{
	ViewLayer *view_layer = DEG_get_input_view_layer(sctx->depsgraph);
	const eSnapSelect snap_select = params->snap_select;
	const bool use_object_edit_cage = params->use_object_edit_cage;

	Base *base_act = view_layer->basact;
	for (Base *base = view_layer->object_bases.first; base != NULL; base = base->next) {
		if ((BASE_VISIBLE(base)) && (base->flag_legacy & BA_SNAP_FIX_DEPS_FIASCO) == 0 &&
		    !((snap_select == SNAP_NOT_SELECTED && ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL))) ||
		      (snap_select == SNAP_NOT_ACTIVE && base == base_act)))
		{
			Object *obj_eval = DEG_get_evaluated_object(sctx->depsgraph, base->object);
			if (obj_eval->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(sctx->depsgraph, sctx->scene, obj_eval);
				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					sob_callback(sctx, use_object_edit_cage, dupli_ob->ob, dupli_ob->mat, data);
				}
				free_object_duplilist(lb);
			}

			sob_callback(sctx, use_object_edit_cage, obj_eval, obj_eval->obmat, data);
		}
	}
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
		sod = *sod_p = snap_object_data_mesh_create(sctx);
	}

	BVHTreeFromMesh *treedata = &sod->treedata;

	/* The tree is owned by the Mesh and may have been freed since we last used. */
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
					*r_index = treedata->looptri[hit.index].poly;
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
	Object *em_ob = em->ob;

	BLI_assert(em_ob->data == BKE_object_get_pre_modified_mesh(ob));

	void **sod_p;
	/* Use `em->ob` as the key in ghash since the editmesh is used
	 * to create bvhtree and is the same for each linked object. */
	if (BLI_ghash_ensure_p(sctx->cache.object_map, em_ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = snap_object_data_editmesh_create(sctx, em->bm);
	}

	{
		float min[3], max[3];
		mul_v3_m4v3(min, obmat, sod->min);
		mul_v3_m4v3(max, obmat, sod->max);

		if (!isect_ray_aabb_v3_simple(
		        ray_start, ray_dir, min, max, NULL, NULL))
		{
			return retval;
		}
	}

	if (sod->bvh_trees[2] == NULL) {
		sod->bvh_trees[2] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
	}
	treedata = sod->bvh_trees[2];

	BVHCache *em_bvh_cache = ((Mesh *)em_ob->data)->runtime.bvh_cache;

	if (sctx->callbacks.edit_mesh.test_face_fn == NULL) {
		/* The tree is owned by the Mesh and may have been freed since we last used! */
		if (!bvhcache_has_tree(em_bvh_cache, treedata->tree)) {
			free_bvhtree_from_editmesh(treedata);
		}
	}

	if (treedata->tree == NULL) {
		BVHCache **bvh_cache = NULL;
		BLI_bitmap *elem_mask = NULL;
		int looptri_num_active = -1;

		if (sctx->callbacks.edit_mesh.test_face_fn) {
			elem_mask = BLI_BITMAP_NEW(em->tottri, __func__);
			looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
			        em->bm, elem_mask,
			        sctx->callbacks.edit_mesh.test_face_fn, sctx->callbacks.edit_mesh.user_data);
		}
		else {
			/* Only cache if bvhtree is created without a mask.
			 * This helps keep a standardized bvhtree in cache. */
			bvh_cache = &em_bvh_cache;
		}

		bvhtree_from_editmesh_looptri_ex(
		        treedata, em, elem_mask, looptri_num_active,
		        0.0f, 4, 6, bvh_cache);

		if (elem_mask) {
			MEM_freeN(elem_mask);
		}
		if (treedata->tree == NULL) {
			return retval;
		}
	}
	else {
		/* COW hack: Update pointers */
		treedata->em = em;
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
					*r_index = BM_elem_index_get(em->looptris[hit.index][0]->f);
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
        bool use_obedit, bool use_occlusion_test,
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
	bool retval = false;

	if (use_occlusion_test) {
		if (use_obedit && sctx->use_v3d &&
		    !(sctx->v3d_data.v3d->flag & V3D_ZBUF_SELECT))
		{
			/* Use of occlude geometry in editing mode disabled. */
			return false;
		}
	}

	switch (ob->type) {
		case OB_MESH:
			if (use_obedit && BKE_object_is_in_editmode(ob)) {
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
	bool use_occlusion_test;
	bool ret;
};

static void raycast_obj_cb(SnapObjectContext *sctx, bool use_obedit, Object *ob, float obmat[4][4], void *data)
{
	struct RaycastObjUserData *dt = data;

	dt->ret |= raycastObj(
	        sctx,
	        dt->ray_start, dt->ray_dir,
	        ob, obmat, dt->ob_index++,
	        use_obedit, dt->use_occlusion_test,
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
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_dir[3],
        /* read/write args */
        float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4],
        ListBase *r_hit_list)
{
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
		.use_occlusion_test = params->use_occlusion_test,
		.ret = false,
	};

	iter_snap_objects(sctx, params, raycast_obj_cb, &data);

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
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        const bool is_persp, const float co[3],
        float *dist_px_sq, float r_co[3])
{
	if (!isect_point_planes_v3_negated(clip_plane, clip_plane_len, co)) {
		return false;
	}

	float co2d[2] = {
		(dot_m4_v3_row_x(precalc->pmat, co) + precalc->pmat[3][0]),
		(dot_m4_v3_row_y(precalc->pmat, co) + precalc->pmat[3][1]),
	};

	if (is_persp) {
		float w = mul_project_m4_v3_zfac(precalc->pmat, co);
		mul_v2_fl(co2d, 1.0f / w);
	}

	const float dist_sq = len_squared_v2v2(precalc->mval, co2d);
	if (dist_sq < *dist_px_sq) {
		copy_v3_v3(r_co, co);
		*dist_px_sq = dist_sq;
		return true;
	}
	return false;
}

static bool test_projected_edge_dist(
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        const bool is_persp, const float va[3], const float vb[3],
        float *dist_px_sq, float r_co[3])
{
	float near_co[3], lambda;
	if (!isect_ray_seg_v3(
	        precalc->ray_origin,
	        precalc->ray_direction,
	        va, vb, &lambda))
	{
		copy_v3_v3(near_co, va);
	}
	else {
		if (lambda <= 0.0f) {
			copy_v3_v3(near_co, va);
		}
		else if (lambda >= 1.0f) {
			copy_v3_v3(near_co, vb);
		}
		else {
			interp_v3_v3v3(near_co, va, vb, lambda);
		}
	}

	return test_projected_vert_dist(
	        precalc, clip_plane, clip_plane_len,
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

	void *userdata;
	Nearest2DGetVertCoCallback get_vert_co;
	Nearest2DGetEdgeVertsCallback get_edge_verts_index;
	Nearest2DGetTriVertsCallback get_tri_verts_index;
	Nearest2DGetTriEdgesCallback get_tri_edges_index;
	Nearest2DCopyVertNoCallback copy_vert_no;

} Nearest2dUserData;


static void cb_snap_vert(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	const float *co;
	data->get_vert_co(index, &co, data->userdata);

	if (test_projected_vert_dist(
	        precalc,
	        clip_plane,
	        clip_plane_len,
	        data->is_persp, co,
	        &nearest->dist_sq,
	        nearest->co))
	{
		data->copy_vert_no(index, nearest->no, data->userdata);
		nearest->index = index;
	}
}

static void cb_snap_edge(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	int vindex[2];
	data->get_edge_verts_index(index, vindex, data->userdata);

	const float *v_pair[2];
	data->get_vert_co(vindex[0], &v_pair[0], data->userdata);
	data->get_vert_co(vindex[1], &v_pair[1], data->userdata);

	if (test_projected_edge_dist(
	        precalc,
	        clip_plane,
	        clip_plane_len,
	        data->is_persp,
	        v_pair[0], v_pair[1],
	        &nearest->dist_sq,
	        nearest->co))
	{
		sub_v3_v3v3(nearest->no, v_pair[0], v_pair[1]);
		nearest->index = index;
	}
}

static void cb_snap_edge_verts(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	int vindex[2];
	data->get_edge_verts_index(index, vindex, data->userdata);

	for (int i = 2; i--;) {
		if (vindex[i] == nearest->index) {
			continue;
		}
		cb_snap_vert(
		        userdata, vindex[i], precalc,
		        clip_plane, clip_plane_len, nearest);
	}
}

static void cb_snap_tri_edges(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	int eindex[3];
	data->get_tri_edges_index(index, eindex, data->userdata);
	for (int i = 3; i--;) {
		if (eindex[i] != -1) {
			if (eindex[i] == nearest->index) {
				continue;
			}
			cb_snap_edge(
			        userdata, eindex[i], precalc,
			        clip_plane, clip_plane_len, nearest);
		}
	}
}

static void cb_snap_tri_verts(
        void *userdata, int index,
        const struct DistProjectedAABBPrecalc *precalc,
        const float (*clip_plane)[4], const int clip_plane_len,
        BVHTreeNearest *nearest)
{
	struct Nearest2dUserData *data = userdata;

	int vindex[3];
	data->get_tri_verts_index(index, vindex, data->userdata);
	for (int i = 3; i--;) {
		if (vindex[i] == nearest->index) {
			continue;
		}
		cb_snap_vert(
		        userdata, vindex[i], precalc,
		        clip_plane, clip_plane_len, nearest);
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

static short snap_mesh_polygon(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, float obmat[4][4],
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index)
{
	short elem = 0;

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	transpose_m4_m4(tobmat, obmat);
	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	Nearest2dUserData nearest2d = {
		.is_persp = snapdata->view_proj == VIEW_PROJ_PERSP,
	};

	BVHTreeNearest nearest = {
		.index = -1,
		.dist_sq = SQUARE(*dist_px),
	};

	SnapObjectData *sod = BLI_ghash_lookup(sctx->cache.object_map, ob);
	if (sod == NULL) {
		/* The object is in edit mode, and the key used
		 * was the object referenced in BMEditMesh */
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		sod = BLI_ghash_lookup(sctx->cache.object_map, em->ob);
	}

	BLI_assert(sod != NULL);

	if (sod->type == SNAP_MESH) {
		BVHTreeFromMesh *treedata = &((SnapObjectData_Mesh *)sod)->treedata;

		nearest2d.userdata             = treedata;
		nearest2d.get_vert_co          = (Nearest2DGetVertCoCallback)cb_mvert_co_get;
		nearest2d.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_medge_verts_get;
		nearest2d.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_mvert_no_copy;

		MPoly *mp = &((Mesh *)ob->data)->mpoly[*r_index];
		const MLoop *ml;
		if (snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
			elem = SCE_SNAP_MODE_EDGE;
			treedata->edge = ((Mesh *)ob->data)->medge;
			ml = &treedata->loop[mp->loopstart];
			for (int i = mp->totloop; i--; ml++) {
				cb_snap_edge(
				        &nearest2d, ml->e, &neasrest_precalc,
				        clip_planes_local, snapdata->clip_plane_len,
				        &nearest);
			}
		}
		else {
			elem = SCE_SNAP_MODE_VERTEX;
			ml = &treedata->loop[mp->loopstart];
			for (int i = mp->totloop; i--; ml++) {
				cb_snap_vert(
				        &nearest2d, ml->v, &neasrest_precalc,
				        clip_planes_local, snapdata->clip_plane_len,
				        &nearest);
			}
		}
	}
	else {
		BLI_assert(sod->type == SNAP_EDIT_MESH);
		BMEditMesh *em = BKE_editmesh_from_object(ob);

		nearest2d.userdata             = em;
		nearest2d.get_vert_co          = (Nearest2DGetVertCoCallback)cb_bvert_co_get;
		nearest2d.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_bedge_verts_get;
		nearest2d.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_bvert_no_copy;

		BM_mesh_elem_table_ensure(em->bm, BM_FACE);
		BMFace *f = BM_face_at_index(em->bm, *r_index);
		BMLoop *l_iter, *l_first;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		if (snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
			elem = SCE_SNAP_MODE_EDGE;
			BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE);
			do {
				cb_snap_edge(
				        &nearest2d, BM_elem_index_get(l_iter->e),
				        &neasrest_precalc,
				        clip_planes_local, snapdata->clip_plane_len,
				        &nearest);
			} while ((l_iter = l_iter->next) != l_first);
		}
		else {
			elem = SCE_SNAP_MODE_VERTEX;
			BM_mesh_elem_table_ensure(em->bm, BM_VERT);
			do {
				cb_snap_vert(
				        &nearest2d, BM_elem_index_get(l_iter->v),
				        &neasrest_precalc,
				        clip_planes_local, snapdata->clip_plane_len,
				        &nearest);
			} while ((l_iter = l_iter->next) != l_first);
		}
	}

	if (nearest.index != -1) {
		*dist_px = sqrtf(nearest.dist_sq);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);

		if (r_no) {
			float imat[4][4];
			invert_m4_m4(imat, obmat);

			copy_v3_v3(r_no, nearest.no);
			mul_transposed_mat3_m4_v3(imat, r_no);
			normalize_v3(r_no);
		}

		if (r_index) {
			*r_index = nearest.index;
		}

		return elem;
	}

	return 0;
}


static short snap_mesh_edge_verts_mixed(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, float obmat[4][4], float original_dist_px,
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index)
{
	short elem = SCE_SNAP_MODE_EDGE;

	if (ob->type != OB_MESH) {
		return elem;
	}

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	Nearest2dUserData nearest2d = {
		.is_persp     = snapdata->view_proj == VIEW_PROJ_PERSP,
	};

	BVHTreeNearest nearest = {
		.index = -1,
		.dist_sq = SQUARE(original_dist_px),
	};

	SnapObjectData *sod = BLI_ghash_lookup(sctx->cache.object_map, ob);
	BLI_assert(sod != NULL);

	if (sod->type == SNAP_MESH) {
		nearest2d.userdata             = &((SnapObjectData_Mesh *)sod)->treedata;
		nearest2d.get_vert_co          = (Nearest2DGetVertCoCallback)cb_mvert_co_get;
		nearest2d.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_medge_verts_get;
		nearest2d.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_mvert_no_copy;
	}
	else {
		BLI_assert(sod->type == SNAP_EDIT_MESH);
		nearest2d.userdata             = BKE_editmesh_from_object(ob);
		nearest2d.get_vert_co          = (Nearest2DGetVertCoCallback)cb_bvert_co_get;
		nearest2d.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_bedge_verts_get;
		nearest2d.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_bvert_no_copy;
	}

	int vindex[2];
	nearest2d.get_edge_verts_index(*r_index, vindex, nearest2d.userdata);

	const float *v_pair[2];
	nearest2d.get_vert_co(vindex[0], &v_pair[0], nearest2d.userdata);
	nearest2d.get_vert_co(vindex[1], &v_pair[1], nearest2d.userdata);

	float lambda;
	if (!isect_ray_seg_v3(
	        neasrest_precalc.ray_origin,
	        neasrest_precalc.ray_direction,
	        v_pair[0], v_pair[1], &lambda))
	{
		/* do nothing */;
	}
	else if (lambda < 0.25f || 0.75f < lambda) {
		int v_id = lambda < 0.5f ? 0 : 1;

		if (test_projected_vert_dist(
		        &neasrest_precalc, NULL, 0,
		        nearest2d.is_persp, v_pair[v_id],
		        &nearest.dist_sq, nearest.co))
		{
			nearest.index = vindex[v_id];
			nearest2d.copy_vert_no(vindex[v_id], nearest.no, nearest2d.userdata);
			elem = SCE_SNAP_MODE_VERTEX;
		}
	}

	if (nearest.index != -1) {
		*dist_px = sqrtf(nearest.dist_sq);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);

		if (r_no) {
			float imat[4][4];
			invert_m4_m4(imat, obmat);

			copy_v3_v3(r_no, nearest.no);
			mul_transposed_mat3_m4_v3(imat, r_no);
			normalize_v3(r_no);
		}

		if (r_index) {
			*r_index = nearest.index;
		}
	}

	return elem;
}

static short snapArmature(
        SnapData *snapdata,
        Object *ob, float obmat[4][4], bool use_obedit,
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no), int *r_index)
{
	short retval = 0;

	if (snapdata->snap_to_flag == SCE_SNAP_MODE_FACE) { /* Currently only edge and vert */
		return retval;
	}

	float lpmat[4][4], dist_px_sq = SQUARE(*dist_px);
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	use_obedit = use_obedit && BKE_object_is_in_editmode(ob);

	if (use_obedit == false) {
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
	}

	float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	transpose_m4_m4(tobmat, obmat);
	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;

	bArmature *arm = ob->data;
	if (arm->edbo) {
		for (EditBone *eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_ROOTSEL | BONE_TIPSEL)) == 0) {
					bool has_vert_snap = false;

					if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
						has_vert_snap = test_projected_vert_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, eBone->head, &dist_px_sq, r_loc);
						has_vert_snap |= test_projected_vert_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, eBone->tail, &dist_px_sq, r_loc);

						if (has_vert_snap) {
							retval = SCE_SNAP_MODE_VERTEX;
						}
					}
					if (!has_vert_snap && snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
						if (test_projected_edge_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, eBone->head, eBone->tail,
						        &dist_px_sq, r_loc))
						{
							retval = SCE_SNAP_MODE_EDGE;
						}
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
				bool has_vert_snap = false;
				const float *head_vec = pchan->pose_head;
				const float *tail_vec = pchan->pose_tail;

				if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
					has_vert_snap = test_projected_vert_dist(
					        &neasrest_precalc,
					        clip_planes_local, snapdata->clip_plane_len,
					        is_persp, head_vec, &dist_px_sq, r_loc);
					has_vert_snap |= test_projected_vert_dist(
					        &neasrest_precalc,
					        clip_planes_local, snapdata->clip_plane_len,
					        is_persp, tail_vec, &dist_px_sq, r_loc);

					if (has_vert_snap) {
						retval = SCE_SNAP_MODE_VERTEX;
					}
				}
				if (!has_vert_snap && snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
					if (test_projected_edge_dist(
					        &neasrest_precalc,
					        clip_planes_local, snapdata->clip_plane_len,
					        is_persp, head_vec, tail_vec,
					        &dist_px_sq, r_loc))
					{
						retval = SCE_SNAP_MODE_EDGE;
					}
				}
			}
		}
	}

	if (retval) {
		*dist_px = sqrtf(dist_px_sq);
		mul_m4_v3(obmat, r_loc);
		if (r_index) {
			/* Does not support index. */
			*r_index = -1;
		}
		return retval;
	}

	return 0;
}

static short snapCurve(
        SnapData *snapdata,
        Object *ob, float obmat[4][4], bool use_obedit,
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no), int *r_index)
{
	bool has_snap = false;

	/* only vertex snapping mode (eg control points and handles) supported for now) */
	if (snapdata->snap_to_flag != SCE_SNAP_MODE_VERTEX) {
		return 0;
	}

	Curve *cu = ob->data;
	float dist_px_sq = SQUARE(*dist_px);

	float lpmat[4][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);

	struct DistProjectedAABBPrecalc neasrest_precalc;
	dist_squared_to_projected_aabb_precalc(
	        &neasrest_precalc, lpmat, snapdata->win_size, snapdata->mval);

	use_obedit = use_obedit && BKE_object_is_in_editmode(ob);

	if (use_obedit == false) {
		/* Test BoundBox */
		BoundBox *bb = BKE_curve_boundbox_get(ob);
		if (bb) {
			bool dummy[3];
			/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see: T46099, T46816 */
			float bb_dist_px_sq = dist_squared_to_projected_aabb(
			        &neasrest_precalc, bb->vec[0], bb->vec[6], dummy);

			if (bb_dist_px_sq > dist_px_sq) {
				return 0;
			}
		}
	}

	float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	transpose_m4_m4(tobmat, obmat);
	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;

	for (Nurb *nu = (use_obedit ? cu->editnurb->nurbs.first : cu->nurb.first); nu; nu = nu->next) {
		for (int u = 0; u < nu->pntsu; u++) {
			if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
				if (use_obedit) {
					if (nu->bezt) {
						/* don't snap to selected (moving) or hidden */
						if (nu->bezt[u].f2 & SELECT || nu->bezt[u].hide != 0) {
							break;
						}
						has_snap |= test_projected_vert_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, nu->bezt[u].vec[1], &dist_px_sq,
						        r_loc);
						/* don't snap if handle is selected (moving), or if it is aligning to a moving handle */
						if (!(nu->bezt[u].f1 & SELECT) &&
						    !(nu->bezt[u].h1 & HD_ALIGN && nu->bezt[u].f3 & SELECT))
						{
							has_snap |= test_projected_vert_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, nu->bezt[u].vec[0], &dist_px_sq,
						        r_loc);
						}
						if (!(nu->bezt[u].f3 & SELECT) &&
						    !(nu->bezt[u].h2 & HD_ALIGN && nu->bezt[u].f1 & SELECT))
						{
							has_snap |= test_projected_vert_dist(
							        &neasrest_precalc,
							        clip_planes_local, snapdata->clip_plane_len,
							        is_persp, nu->bezt[u].vec[2], &dist_px_sq,
							        r_loc);
						}
					}
					else {
						/* don't snap to selected (moving) or hidden */
						if (nu->bp[u].f1 & SELECT || nu->bp[u].hide != 0) {
							break;
						}
						has_snap |= test_projected_vert_dist(
						        &neasrest_precalc,
						        clip_planes_local, snapdata->clip_plane_len,
						        is_persp, nu->bp[u].vec, &dist_px_sq,
						        r_loc);
					}
				}
				else {
					/* curve is not visible outside editmode if nurb length less than two */
					if (nu->pntsu > 1) {
						if (nu->bezt) {
							has_snap |= test_projected_vert_dist(
							        &neasrest_precalc,
							        clip_planes_local, snapdata->clip_plane_len,
							        is_persp, nu->bezt[u].vec[1], &dist_px_sq,
							        r_loc);
						}
						else {
							has_snap |= test_projected_vert_dist(
							        &neasrest_precalc,
							        clip_planes_local, snapdata->clip_plane_len,
							        is_persp, nu->bp[u].vec, &dist_px_sq,
							        r_loc);
						}
					}
				}
			}
		}
	}
	if (has_snap) {
		*dist_px = sqrtf(dist_px_sq);
		mul_m4_v3(obmat, r_loc);
		if (r_index) {
			/* Does not support index yet. */
			*r_index = -1;
		}
		return SCE_SNAP_MODE_VERTEX;
	}

	return 0;
}

/* may extend later (for now just snaps to empty center) */
static short snapEmpty(
        SnapData *snapdata,
        Object *ob, float obmat[4][4],
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no), int *r_index)
{
	short retval = 0;

	if (ob->transflag & OB_DUPLI) {
		return retval;
	}

	/* for now only vertex supported */
	if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
		struct DistProjectedAABBPrecalc neasrest_precalc;
		dist_squared_to_projected_aabb_precalc(
		        &neasrest_precalc, snapdata->pmat, snapdata->win_size, snapdata->mval);

		float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
		transpose_m4_m4(tobmat, obmat);
		for (int i = snapdata->clip_plane_len; i--;) {
			mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
		}

		bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
		float dist_px_sq = SQUARE(*dist_px);
		float co[3];
		copy_v3_v3(co, obmat[3]);
		if (test_projected_vert_dist(
		        &neasrest_precalc,
		        clip_planes_local, snapdata->clip_plane_len,
		        is_persp, co, &dist_px_sq, r_loc))
		{
			*dist_px = sqrtf(dist_px_sq);
			retval = SCE_SNAP_MODE_VERTEX;
		}
	}

	if (retval) {
		if (r_index) {
			/* Does not support index. */
			*r_index = -1;
		}
		return retval;
	}

	return 0;
}

static short snapCamera(
        const SnapObjectContext *sctx, SnapData *snapdata,
        Object *object, float obmat[4][4],
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float *UNUSED(r_no), int *r_index)
{
	short retval = 0;

	Depsgraph *depsgraph = sctx->depsgraph;
	Scene *scene = sctx->scene;

	bool is_persp = snapdata->view_proj == VIEW_PROJ_PERSP;
	float dist_px_sq = SQUARE(*dist_px);

	float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
	MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
	MovieTracking *tracking;

	if (clip == NULL) {
		return retval;
	}
	if (object->transflag & OB_DUPLI) {
		return retval;
	}

	float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	transpose_m4_m4(tobmat, obmat);
	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	tracking = &clip->tracking;

	BKE_tracking_get_camera_object_matrix(depsgraph, scene, object, orig_camera_mat);

	invert_m4_m4(orig_camera_imat, orig_camera_mat);
	invert_m4_m4(imat, obmat);

	if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
		struct DistProjectedAABBPrecalc neasrest_precalc;
		dist_squared_to_projected_aabb_precalc(
		        &neasrest_precalc, snapdata->pmat, snapdata->win_size, snapdata->mval);

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
				if (test_projected_vert_dist(
				        &neasrest_precalc,
				        clip_planes_local, snapdata->clip_plane_len,
				        is_persp, bundle_pos, &dist_px_sq, r_loc))
				{
					retval = SCE_SNAP_MODE_VERTEX;
				}
			}
		}
	}

	if (retval) {
		*dist_px = sqrtf(dist_px_sq);
		if (r_index) {
			/* Does not support index. */
			*r_index = -1;
		}
		return retval;
	}

	return 0;
}

static short snapMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, Mesh *me, float obmat[4][4],
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index)
{
	BLI_assert(snapdata->snap_to_flag != SCE_SNAP_MODE_FACE);

	if ((snapdata->snap_to_flag & ~SCE_SNAP_MODE_FACE) == SCE_SNAP_MODE_EDGE) {
		if (me->totedge == 0) {
			return 0;
		}
	}
	else {
		if (me->totvert == 0) {
			return 0;
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
			return 0;
		}
	}

	SnapObjectData_Mesh *sod = NULL;

	void **sod_p;
	if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = snap_object_data_mesh_create(sctx);
	}

	BVHTreeFromMesh *treedata, dummy_treedata;
	BVHTree **bvhtree;
	treedata = &sod->treedata;
	bvhtree = sod->bvhtree;

	/* The tree is owned by the Mesh and may have been freed since we last used! */
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
	if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
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

	Nearest2dUserData nearest2d = {
		.is_persp             = snapdata->view_proj == VIEW_PROJ_PERSP,
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
	int last_index = nearest.index;
	short elem = SCE_SNAP_MODE_VERTEX;

	float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	transpose_m4_m4(tobmat, obmat);
	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	if (bvhtree[1] && (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX)) {
		/* snap to loose verts */
		BLI_bvhtree_find_nearest_projected(
		        bvhtree[1], lpmat, snapdata->win_size, snapdata->mval,
		        clip_planes_local, snapdata->clip_plane_len,
		        &nearest, cb_snap_vert, &nearest2d);

		last_index = nearest.index;
	}

	if (snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
		if (bvhtree[0]) {
			/* snap to loose edges */
			BLI_bvhtree_find_nearest_projected(
			        bvhtree[0], lpmat, snapdata->win_size, snapdata->mval,
			        clip_planes_local, snapdata->clip_plane_len,
			        &nearest, cb_snap_edge, &nearest2d);
		}

		if (treedata->tree) {
			/* snap to looptris */
			BLI_bvhtree_find_nearest_projected(
			        treedata->tree, lpmat, snapdata->win_size, snapdata->mval,
			        clip_planes_local, snapdata->clip_plane_len,
			        &nearest, cb_snap_tri_edges, &nearest2d);
		}

		if (last_index != nearest.index) {
			elem = SCE_SNAP_MODE_EDGE;
		}
	}
	else {
		BLI_assert(snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX);
		if (bvhtree[0]) {
			/* snap to loose edges */
			BLI_bvhtree_find_nearest_projected(
			        bvhtree[0], lpmat, snapdata->win_size, snapdata->mval,
			        clip_planes_local, snapdata->clip_plane_len,
			        &nearest, cb_snap_edge_verts, &nearest2d);
		}

		if (treedata->tree) {
			/* snap to looptris */
			BLI_bvhtree_find_nearest_projected(
			        treedata->tree, lpmat, snapdata->win_size, snapdata->mval,
			        clip_planes_local, snapdata->clip_plane_len,
			        &nearest, cb_snap_tri_verts, &nearest2d);
		}
	}

	if (nearest.index != -1) {
		*dist_px = sqrtf(nearest.dist_sq);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);

		if (r_no) {
			float imat[4][4];
			invert_m4_m4(imat, obmat);

			copy_v3_v3(r_no, nearest.no);
			mul_transposed_mat3_m4_v3(imat, r_no);
			normalize_v3(r_no);
		}
		if (r_index) {
			*r_index = nearest.index;
		}

		return elem;
	}

	return 0;
}

static short snapEditMesh(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, BMEditMesh *em, float obmat[4][4],
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index)
{
	BLI_assert(snapdata->snap_to_flag != SCE_SNAP_MODE_FACE);

	if ((snapdata->snap_to_flag & ~SCE_SNAP_MODE_FACE) == SCE_SNAP_MODE_EDGE) {
		if (em->bm->totedge == 0) {
			return 0;
		}
	}
	else {
		if (em->bm->totvert == 0) {
			return 0;
		}
	}

	SnapObjectData_EditMesh *sod = NULL;
	BVHTreeFromEditMesh *treedata_vert = NULL, *treedata_edge = NULL;
	Object *em_ob = em->ob;

	BLI_assert(em_ob->data == BKE_object_get_pre_modified_mesh(ob));
	UNUSED_VARS_NDEBUG(ob);

	void **sod_p;
	/* Use `em->ob` as the key in ghash since the editmesh is used
	 * to create bvhtree and is the same for each linked object. */
	if (BLI_ghash_ensure_p(sctx->cache.object_map, em_ob, &sod_p)) {
		sod = *sod_p;
	}
	else {
		sod = *sod_p = snap_object_data_editmesh_create(sctx, em->bm);
	}

	float dist_px_sq = SQUARE(*dist_px);

	{
		float min[3], max[3];
		mul_v3_m4v3(min, obmat, sod->min);
		mul_v3_m4v3(max, obmat, sod->max);

		/* In vertex and edges you need to get the pixel distance from ray to BoundBox, see: T46099, T46816 */
		struct DistProjectedAABBPrecalc data_precalc;
		dist_squared_to_projected_aabb_precalc(
		        &data_precalc, snapdata->pmat, snapdata->win_size, snapdata->mval);

		bool dummy[3];
		float bb_dist_px_sq = dist_squared_to_projected_aabb(
		        &data_precalc, min, max, dummy);

		if (bb_dist_px_sq > dist_px_sq) {
			return 0;
		}
	}

	BVHCache *em_bvh_cache = ((Mesh *)em_ob->data)->runtime.bvh_cache;

	if (snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
		if (sod->bvh_trees[0] == NULL) {
			sod->bvh_trees[0] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(**sod->bvh_trees));
		}
		treedata_vert = sod->bvh_trees[0];

		if (sctx->callbacks.edit_mesh.test_vert_fn == NULL) {
			/* The tree is owned by the Mesh and may have been freed since we last used! */
			if (!bvhcache_has_tree(em_bvh_cache, treedata_vert->tree)) {
				free_bvhtree_from_editmesh(treedata_vert);
			}
		}

		if (treedata_vert->tree == NULL) {
			BLI_bitmap *verts_mask = NULL;
			int verts_num_active = -1;
			if (sctx->callbacks.edit_mesh.test_vert_fn) {
				verts_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
				verts_num_active = BM_iter_mesh_bitmap_from_filter(
				        BM_VERTS_OF_MESH, em->bm, verts_mask,
				        (bool(*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_vert_fn,
				        sctx->callbacks.edit_mesh.user_data);

				bvhtree_from_editmesh_verts_ex(treedata_vert, em, verts_mask, verts_num_active, 0.0f, 2, 6);
				MEM_freeN(verts_mask);
			}
			else {
				bvhtree_from_editmesh_verts(treedata_vert, em, 0.0f, 2, 6, &em_bvh_cache);
			}
		}
	}

	if (snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
		if (sod->bvh_trees[1] == NULL) {
			sod->bvh_trees[1] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(**sod->bvh_trees));
		}
		treedata_edge = sod->bvh_trees[1];

		if (sctx->callbacks.edit_mesh.test_edge_fn == NULL) {
			/* The tree is owned by the Mesh and may have been freed since we last used! */
			if (!bvhcache_has_tree(em_bvh_cache, treedata_edge->tree)) {
				free_bvhtree_from_editmesh(treedata_edge);
			}
		}

		if (treedata_edge->tree == NULL) {
			BLI_bitmap *edges_mask = NULL;
			int edges_num_active = -1;
			if (sctx->callbacks.edit_mesh.test_edge_fn) {
				edges_mask = BLI_BITMAP_NEW(em->bm->totedge, __func__);
				edges_num_active = BM_iter_mesh_bitmap_from_filter(
				        BM_EDGES_OF_MESH, em->bm, edges_mask,
				        (bool(*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_edge_fn,
				        sctx->callbacks.edit_mesh.user_data);

				bvhtree_from_editmesh_edges_ex(treedata_edge, em, edges_mask, edges_num_active, 0.0f, 2, 6);
				MEM_freeN(edges_mask);
			}
			else {
				bvhtree_from_editmesh_edges(treedata_edge, em, 0.0f, 2, 6, &em_bvh_cache);
			}
		}
	}

	Nearest2dUserData nearest2d = {
		.is_persp             = snapdata->view_proj == VIEW_PROJ_PERSP,
		.userdata             = em,
		.get_vert_co          = (Nearest2DGetVertCoCallback)cb_bvert_co_get,
		.get_edge_verts_index = (Nearest2DGetEdgeVertsCallback)cb_bedge_verts_get,
		.copy_vert_no         = (Nearest2DCopyVertNoCallback)cb_bvert_no_copy,
	};

	BVHTreeNearest nearest = {
		.index = -1,
		.dist_sq = dist_px_sq,
	};
	int last_index = nearest.index;
	short elem = SCE_SNAP_MODE_VERTEX;

	float lpmat[4][4], tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
	mul_m4_m4m4(lpmat, snapdata->pmat, obmat);
	transpose_m4_m4(tobmat, obmat);

	for (int i = snapdata->clip_plane_len; i--;) {
		mul_v4_m4v4(clip_planes_local[i], tobmat, snapdata->clip_plane[i]);
	}

	if (treedata_vert && snapdata->snap_to_flag & SCE_SNAP_MODE_VERTEX) {
		BM_mesh_elem_table_ensure(em->bm, BM_VERT);
		BLI_bvhtree_find_nearest_projected(
		        treedata_vert->tree, lpmat, snapdata->win_size, snapdata->mval,
		        clip_planes_local, snapdata->clip_plane_len,
		        &nearest, cb_snap_vert, &nearest2d);

		last_index = nearest.index;
	}

	if (treedata_edge && snapdata->snap_to_flag & SCE_SNAP_MODE_EDGE) {
		BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_VERT);
		BLI_bvhtree_find_nearest_projected(
		        treedata_edge->tree, lpmat, snapdata->win_size, snapdata->mval,
		        clip_planes_local, snapdata->clip_plane_len,
		        &nearest, cb_snap_edge, &nearest2d);

		if (last_index != nearest.index) {
			elem = SCE_SNAP_MODE_EDGE;
		}
	}

	if (nearest.index != -1) {
		*dist_px = sqrtf(nearest.dist_sq);

		copy_v3_v3(r_loc, nearest.co);
		mul_m4_v3(obmat, r_loc);
		if (r_no) {
			float imat[4][4];
			invert_m4_m4(imat, obmat);

			copy_v3_v3(r_no, nearest.no);
			mul_transposed_mat3_m4_v3(imat, r_no);
			normalize_v3(r_no);
		}
		if (r_index) {
			*r_index = nearest.index;
		}

		return elem;
	}

	return 0;
}

/**
 * \param use_obedit: Uses the coordinates of BMesh (if any) to do the snapping;
 *
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static short snapObject(
        SnapObjectContext *sctx, SnapData *snapdata,
        Object *ob, float obmat[4][4], bool use_obedit,
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	short retval = 0;

	switch (ob->type) {
		case OB_MESH:
			if (use_obedit && BKE_object_is_in_editmode(ob)) {
				BMEditMesh *em = BKE_editmesh_from_object(ob);
				retval = snapEditMesh(
				        sctx, snapdata, ob, em, obmat,
				        dist_px,
				        r_loc, r_no, r_index);
			}
			else {
				retval = snapMesh(
				        sctx, snapdata, ob, ob->data, obmat,
				        dist_px,
				        r_loc, r_no, r_index);
			}
			break;

		case OB_ARMATURE:
			retval = snapArmature(
			        snapdata,
			        ob, obmat, use_obedit,
			        dist_px,
			        r_loc, r_no, r_index);
			break;

		case OB_CURVE:
			retval = snapCurve(
			        snapdata,
			        ob, obmat, use_obedit,
			        dist_px,
			        r_loc, r_no, r_index);
			break;

		case OB_EMPTY:
			retval = snapEmpty(
			        snapdata, ob, obmat,
			        dist_px,
			        r_loc, r_no, r_index);
			break;
		case OB_GPENCIL:
			retval = snapEmpty(
			        snapdata, ob, obmat,
			        dist_px,
			        r_loc, r_no, r_index);
			break;
		case OB_CAMERA:
			retval = snapCamera(
			        sctx, snapdata, ob, obmat,
			        dist_px,
			        r_loc, r_no, r_index);
			break;
	}

	if (retval) {
		if (r_ob) {
			*r_ob = ob;
		}
		if (r_obmat) {
			copy_m4_m4(r_obmat, obmat);
		}
		return retval;
	}

	return 0;
}


struct SnapObjUserData {
	SnapData *snapdata;
	/* read/write args */
	float *dist_px;
	/* return args */
	float *r_loc;
	float *r_no;
	int *r_index;
	Object **r_ob;
	float (*r_obmat)[4];
	short ret;
};

static void sanp_obj_cb(SnapObjectContext *sctx, bool is_obedit, Object *ob, float obmat[4][4], void *data)
{
	struct SnapObjUserData *dt = data;

	short elem = snapObject(
	        sctx, dt->snapdata,
	        ob, obmat, is_obedit,
	        /* read/write args */
	        dt->dist_px,
	        /* return args */
	        dt->r_loc, dt->r_no, dt->r_index,
	        dt->r_ob, dt->r_obmat);

	if (elem) {
		dt->ret = elem;
	}
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
static short snapObjectsRay(
        SnapObjectContext *sctx, SnapData *snapdata,
        const struct SnapObjectParams *params,
        /* read/write args */
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	struct SnapObjUserData data = {
		.snapdata = snapdata,
		.dist_px = dist_px,
		.r_loc = r_loc,
		.r_no = r_no,
		.r_ob = r_ob,
		.r_index = r_index,
		.r_obmat = r_obmat,
		.ret = 0,
	};

	iter_snap_objects(sctx, params, sanp_obj_cb, &data);

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
	        sctx, params,
	        ray_start, ray_normal,
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
	        sctx, params,
	        ray_start, ray_normal,
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

static short transform_snap_context_project_view3d_mixed_impl(
        SnapObjectContext *sctx,
        const unsigned short snap_to_flag,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	BLI_assert(
	        (snap_to_flag & (
	        SCE_SNAP_MODE_VERTEX |
	        SCE_SNAP_MODE_EDGE   |
	        SCE_SNAP_MODE_FACE)) != 0);

	short retval = 0;
	bool has_hit = false;
	int index = -1;

	float loc[3], no[3], obmat[4][4];
	Object *ob = NULL;

	const ARegion *ar = sctx->v3d_data.ar;
	const RegionView3D *rv3d = ar->regiondata;

	bool use_occlusion_test =
	        params->use_occlusion_test &&
	        !(sctx->v3d_data.v3d->shading.flag & V3D_SHADING_XRAY);

	if (snap_to_flag & SCE_SNAP_MODE_FACE || use_occlusion_test) {
		float ray_start[3], ray_normal[3];

		if (!ED_view3d_win_to_ray_ex(
		        sctx->depsgraph,
		        sctx->v3d_data.ar, sctx->v3d_data.v3d,
		        mval, NULL, ray_normal, ray_start, true))
		{
			return false;
		}

		float dummy_ray_depth = BVH_RAYCAST_DIST_MAX;

		has_hit = raycastObjects(
		        sctx, params,
		        ray_start, ray_normal,
		        &dummy_ray_depth, loc, no,
		        &index, &ob, obmat, NULL);

		if (has_hit && (snap_to_flag & SCE_SNAP_MODE_FACE)) {
			retval = SCE_SNAP_MODE_FACE;
		}
	}

	if (snap_to_flag & (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE)) {
		short elem;
		float dist_px_tmp = *dist_px;

		SnapData snapdata;
		copy_m4_m4(snapdata.pmat, rv3d->persmat);
		snapdata.win_size[0] = ar->winx;
		snapdata.win_size[1] = ar->winy;
		copy_v2_v2(snapdata.mval, mval);
		snapdata.snap_to_flag = snap_to_flag;
		snapdata.view_proj = rv3d->is_persp ? VIEW_PROJ_PERSP : VIEW_PROJ_ORTHO;

		planes_from_projmat(
		        snapdata.pmat,
		        NULL, NULL, NULL, NULL,
		        snapdata.clip_plane[0], snapdata.clip_plane[1]);

		snapdata.clip_plane_len = 2;

		if (has_hit) {
			/* Compute the new clip_pane but do not add it yet. */
			float new_clipplane[4];
			plane_from_point_normal_v3(new_clipplane, loc, no);
			if (dot_v3v3(snapdata.clip_plane[0], new_clipplane) > 0.0f) {
				/* The plane is facing the wrong direction. */
				negate_v4(new_clipplane);
			}

			/* Try to snap only to the polygon. */
			elem = snap_mesh_polygon(
			        sctx, &snapdata, ob, obmat,
			        &dist_px_tmp, loc, no, &index);

			if (elem) {
				retval = elem;
			}

			/* Add the new clip plane to the beginning of the list. */
			for (int i = snapdata.clip_plane_len; i != 0; i--) {
				copy_v4_v4(snapdata.clip_plane[i], snapdata.clip_plane[i - 1]);
			}
			copy_v4_v4(snapdata.clip_plane[0], new_clipplane);
			snapdata.clip_plane_len++;
		}

		elem = snapObjectsRay(
		        sctx, &snapdata, params,
		        &dist_px_tmp, loc, no, &index, &ob, obmat);

		if (elem) {
			retval = elem;
		}

		if ((retval == SCE_SNAP_MODE_EDGE) &&
		    (snapdata.snap_to_flag & SCE_SNAP_MODE_VERTEX))
		{
			retval = snap_mesh_edge_verts_mixed(
			        sctx, &snapdata,
			        ob, obmat, *dist_px,
			        &dist_px_tmp, loc, no, &index);
		}

		*dist_px = dist_px_tmp;
	}

	if (retval) {
		copy_v3_v3(r_loc, loc);
		if (r_no) {
			copy_v3_v3(r_no, no);
		}
		if (r_ob) {
			*r_ob = ob;
		}
		if (r_obmat) {
			copy_m4_m4(r_obmat, obmat);
		}
		if (r_index) {
			*r_index = index;
		}
		return retval;
	}

	return 0;
}

bool ED_transform_snap_object_project_view3d_ex(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	return transform_snap_context_project_view3d_mixed_impl(
	        sctx,
	        snap_to, params,
	        mval, dist_px,
	        r_loc, r_no, r_index, r_ob, r_obmat) != 0;
}

/**
 * Convenience function for performing snapping.
 *
 * Given a 2D region value, snap to vert/edge/face.
 *
 * \param sctx: Snap context.
 * \param mval_fl: Screenspace coordinate.
 * \param dist_px: Maximum distance to snap (in pixels).
 * \param r_co: hit location.
 * \param r_no: hit normal (optional).
 * \return Snap success
 */
bool ED_transform_snap_object_project_view3d(
        SnapObjectContext *sctx,
        const unsigned short snap_to,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float r_loc[3], float r_no[3])
{
	return ED_transform_snap_object_project_view3d_ex(
	        sctx,
	        snap_to,
	        params,
	        mval, dist_px,
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
