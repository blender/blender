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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andr Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/shrinkwrap.c
 *  \ingroup bke
 */

#include <string.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"

#include "BKE_shrinkwrap.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_modifier.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"  /* for OMP limits. */
#include "BKE_subsurf.h"

#include "BLI_strict_flags.h"

/* for timing... */
#if 0
#  include "PIL_time_utildefines.h"
#else
#  define TIMEIT_BENCH(expr, id) (expr)
#endif

/* Util macros */
#define OUT_OF_MEMORY() ((void)printf("Shrinkwrap: Out of memory\n"))


typedef struct ShrinkwrapCalcData {
	ShrinkwrapModifierData *smd;    //shrinkwrap modifier data

	struct Object *ob;              //object we are applying shrinkwrap to

	struct MVert *vert;             //Array of verts being projected (to fetch normals or other data)
	float(*vertexCos)[3];          //vertexs being shrinkwraped
	int numVerts;

	struct MDeformVert *dvert;      //Pointer to mdeform array
	int vgroup;                     //Vertex group num
	bool invert_vgroup;             /* invert vertex group influence */

	struct Mesh *target;     //mesh we are shrinking to
	struct SpaceTransform local2target;    //transform to move between local and target space
	struct ShrinkwrapTreeData *tree; // mesh BVH tree data

	float keepDist;                 //Distance to keep above target surface (units are in local space)

} ShrinkwrapCalcData;

typedef struct ShrinkwrapCalcCBData {
	ShrinkwrapCalcData *calc;

	ShrinkwrapTreeData *tree;
	ShrinkwrapTreeData *aux_tree;

	float *proj_axis;
	SpaceTransform *local2aux;
} ShrinkwrapCalcCBData;


/* Checks if the modifier needs target normals with these settings. */
bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode)
{
	return shrinkType != MOD_SHRINKWRAP_NEAREST_VERTEX && shrinkMode == MOD_SHRINKWRAP_ABOVE_SURFACE;
}

/* Initializes the mesh data structure from the given mesh and settings. */
bool BKE_shrinkwrap_init_tree(ShrinkwrapTreeData *data, Mesh *mesh, int shrinkType, int shrinkMode, bool force_normals)
{
	memset(data, 0, sizeof(*data));

	if (!mesh || mesh->totvert <= 0) {
		return false;
	}

	data->mesh = mesh;

	if (shrinkType == MOD_SHRINKWRAP_NEAREST_VERTEX) {
		data->bvh = BKE_bvhtree_from_mesh_get(&data->treeData, mesh, BVHTREE_FROM_VERTS, 2);

		return data->bvh != NULL;
	}
	else {
		if (mesh->totpoly <= 0) {
			return false;
		}

		data->bvh = BKE_bvhtree_from_mesh_get(&data->treeData, mesh, BVHTREE_FROM_LOOPTRI, 4);

		if (data->bvh == NULL) {
			return false;
		}

		if (force_normals || BKE_shrinkwrap_needs_normals(shrinkType, shrinkMode)) {
			if ((mesh->flag & ME_AUTOSMOOTH) != 0) {
				data->clnors = CustomData_get_layer(&mesh->ldata, CD_NORMAL);
			}
		}

		return true;
	}
}

/* Frees the tree data if necessary. */
void BKE_shrinkwrap_free_tree(ShrinkwrapTreeData *data)
{
	free_bvhtree_from_mesh(&data->treeData);
}

/*
 * Shrinkwrap to the nearest vertex
 *
 * it builds a kdtree of vertexs we can attach to and then
 * for each vertex performs a nearest vertex search on the tree
 */
static void shrinkwrap_calc_nearest_vertex_cb_ex(
        void *__restrict userdata,
        const int i,
        const ParallelRangeTLS *__restrict tls)
{
	ShrinkwrapCalcCBData *data = userdata;

	ShrinkwrapCalcData *calc = data->calc;
	BVHTreeFromMesh *treeData = &data->tree->treeData;
	BVHTreeNearest *nearest = tls->userdata_chunk;

	float *co = calc->vertexCos[i];
	float tmp_co[3];
	float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

	if (calc->invert_vgroup) {
		weight = 1.0f - weight;
	}

	if (weight == 0.0f) {
		return;
	}

	/* Convert the vertex to tree coordinates */
	if (calc->vert) {
		copy_v3_v3(tmp_co, calc->vert[i].co);
	}
	else {
		copy_v3_v3(tmp_co, co);
	}
	BLI_space_transform_apply(&calc->local2target, tmp_co);

	/* Use local proximity heuristics (to reduce the nearest search)
	 *
	 * If we already had an hit before.. we assume this vertex is going to have a close hit to that other vertex
	 * so we can initiate the "nearest.dist" with the expected value to that last hit.
	 * This will lead in pruning of the search tree. */
	if (nearest->index != -1)
		nearest->dist_sq = len_squared_v3v3(tmp_co, nearest->co);
	else
		nearest->dist_sq = FLT_MAX;

	BLI_bvhtree_find_nearest(treeData->tree, tmp_co, nearest, treeData->nearest_callback, treeData);


	/* Found the nearest vertex */
	if (nearest->index != -1) {
		/* Adjusting the vertex weight,
		 * so that after interpolating it keeps a certain distance from the nearest position */
		if (nearest->dist_sq > FLT_EPSILON) {
			const float dist = sqrtf(nearest->dist_sq);
			weight *= (dist - calc->keepDist) / dist;
		}

		/* Convert the coordinates back to mesh coordinates */
		copy_v3_v3(tmp_co, nearest->co);
		BLI_space_transform_invert(&calc->local2target, tmp_co);

		interp_v3_v3v3(co, co, tmp_co, weight);  /* linear interpolation */
	}
}

static void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *calc)
{
	BVHTreeNearest nearest  = NULL_BVHTreeNearest;

	/* Setup nearest */
	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;

	ShrinkwrapCalcCBData data = {.calc = calc, .tree = calc->tree};
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
	settings.userdata_chunk = &nearest;
	settings.userdata_chunk_size = sizeof(nearest);
	BLI_task_parallel_range(0, calc->numVerts,
	                        &data, shrinkwrap_calc_nearest_vertex_cb_ex,
	                        &settings);
}


/*
 * This function raycast a single vertex and updates the hit if the "hit" is considered valid.
 * Returns true if "hit" was updated.
 * Opts control whether an hit is valid or not
 * Supported options are:
 *	MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 *	MOD_SHRINKWRAP_CULL_TARGET_BACKFACE (back faces hits are ignored)
 */
bool BKE_shrinkwrap_project_normal(
        char options, const float vert[3], const float dir[3],
        const float ray_radius, const SpaceTransform *transf,
        ShrinkwrapTreeData *tree, BVHTreeRayHit *hit)
{
	/* don't use this because this dist value could be incompatible
	 * this value used by the callback for comparing prev/new dist values.
	 * also, at the moment there is no need to have a corrected 'dist' value */
// #define USE_DIST_CORRECT

	float tmp_co[3], tmp_no[3];
	const float *co, *no;
	BVHTreeRayHit hit_tmp;

	/* Copy from hit (we need to convert hit rays from one space coordinates to the other */
	memcpy(&hit_tmp, hit, sizeof(hit_tmp));

	/* Apply space transform (TODO readjust dist) */
	if (transf) {
		copy_v3_v3(tmp_co, vert);
		BLI_space_transform_apply(transf, tmp_co);
		co = tmp_co;

		copy_v3_v3(tmp_no, dir);
		BLI_space_transform_apply_normal(transf, tmp_no);
		no = tmp_no;

#ifdef USE_DIST_CORRECT
		hit_tmp.dist *= mat4_to_scale(((SpaceTransform *)transf)->local2target);
#endif
	}
	else {
		co = vert;
		no = dir;
	}

	hit_tmp.index = -1;

	BLI_bvhtree_ray_cast(tree->bvh, co, no, ray_radius, &hit_tmp, tree->treeData.raycast_callback, &tree->treeData);

	if (hit_tmp.index != -1) {
		/* invert the normal first so face culling works on rotated objects */
		if (transf) {
			BLI_space_transform_invert_normal(transf, hit_tmp.no);
		}

		if (options & MOD_SHRINKWRAP_CULL_TARGET_MASK) {
			/* apply backface */
			const float dot = dot_v3v3(dir, hit_tmp.no);
			if (((options & MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE) && dot <= 0.0f) ||
			    ((options & MOD_SHRINKWRAP_CULL_TARGET_BACKFACE)  && dot >= 0.0f))
			{
				return false;  /* Ignore hit */
			}
		}

		if (transf) {
			/* Inverting space transform (TODO make coeherent with the initial dist readjust) */
			BLI_space_transform_invert(transf, hit_tmp.co);
#ifdef USE_DIST_CORRECT
			hit_tmp.dist = len_v3v3(vert, hit_tmp.co);
#endif
		}

		BLI_assert(hit_tmp.dist <= hit->dist);

		memcpy(hit, &hit_tmp, sizeof(hit_tmp));
		return true;
	}
	return false;
}

static void shrinkwrap_calc_normal_projection_cb_ex(
        void *__restrict userdata,
        const int i,
        const ParallelRangeTLS *__restrict tls)
{
	ShrinkwrapCalcCBData *data = userdata;

	ShrinkwrapCalcData *calc = data->calc;
	ShrinkwrapTreeData *tree = data->tree;
	ShrinkwrapTreeData *aux_tree = data->aux_tree;

	float *proj_axis = data->proj_axis;
	SpaceTransform *local2aux = data->local2aux;

	BVHTreeRayHit *hit = tls->userdata_chunk;

	const float proj_limit_squared = calc->smd->projLimit * calc->smd->projLimit;
	float *co = calc->vertexCos[i];
	float tmp_co[3], tmp_no[3];
	float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

	if (calc->invert_vgroup) {
		weight = 1.0f - weight;
	}

	if (weight == 0.0f) {
		return;
	}

	if (calc->vert) {
		/* calc->vert contains verts from evaluated mesh.  */
		/* this coordinated are deformed by vertexCos only for normal projection (to get correct normals) */
		/* for other cases calc->varts contains undeformed coordinates and vertexCos should be used */
		if (calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL) {
			copy_v3_v3(tmp_co, calc->vert[i].co);
			normal_short_to_float_v3(tmp_no, calc->vert[i].no);
		}
		else {
			copy_v3_v3(tmp_co, co);
			copy_v3_v3(tmp_no, proj_axis);
		}
	}
	else {
		copy_v3_v3(tmp_co, co);
		copy_v3_v3(tmp_no, proj_axis);
	}


	hit->index = -1;
	hit->dist = BVH_RAYCAST_DIST_MAX; /* TODO: we should use FLT_MAX here, but sweepsphere code isn't prepared for that */

	bool is_aux = false;

	/* Project over positive direction of axis */
	if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR) {
		if (aux_tree) {
			if (BKE_shrinkwrap_project_normal(
			            0, tmp_co, tmp_no, 0.0,
			            local2aux, aux_tree, hit))
			{
				is_aux = true;
			}
		}

		if (BKE_shrinkwrap_project_normal(
		            calc->smd->shrinkOpts, tmp_co, tmp_no, 0.0,
		            &calc->local2target, tree, hit))
		{
			is_aux = false;
		}
	}

	/* Project over negative direction of axis */
	if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR) {
		float inv_no[3];
		negate_v3_v3(inv_no, tmp_no);

		char options = calc->smd->shrinkOpts;

		if ((options & MOD_SHRINKWRAP_INVERT_CULL_TARGET) && (options & MOD_SHRINKWRAP_CULL_TARGET_MASK)) {
			options ^= MOD_SHRINKWRAP_CULL_TARGET_MASK;
		}

		if (aux_tree) {
			if (BKE_shrinkwrap_project_normal(
			            0, tmp_co, inv_no, 0.0,
			            local2aux, aux_tree, hit))
			{
				is_aux = true;
			}
		}

		if (BKE_shrinkwrap_project_normal(
		            options, tmp_co, inv_no, 0.0,
		            &calc->local2target, tree, hit))
		{
			is_aux = false;
		}
	}

	/* don't set the initial dist (which is more efficient),
	 * because its calculated in the targets space, we want the dist in our own space */
	if (proj_limit_squared != 0.0f) {
		if (hit->index != -1 && len_squared_v3v3(hit->co, co) > proj_limit_squared) {
			hit->index = -1;
		}
	}

	if (hit->index != -1) {
		if (is_aux) {
			BKE_shrinkwrap_snap_point_to_surface(
			        aux_tree, local2aux, calc->smd->shrinkMode,
			        hit->index, hit->co, hit->no, calc->keepDist, tmp_co, hit->co);
		}
		else {
			BKE_shrinkwrap_snap_point_to_surface(
			        tree, &calc->local2target, calc->smd->shrinkMode,
			        hit->index, hit->co, hit->no, calc->keepDist, tmp_co, hit->co);
		}

		interp_v3_v3v3(co, co, hit->co, weight);
	}
}

static void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc)
{
	/* Options about projection direction */
	float proj_axis[3]      = {0.0f, 0.0f, 0.0f};

	/* Raycast and tree stuff */

	/** \note 'hit.dist' is kept in the targets space, this is only used
	 * for finding the best hit, to get the real dist,
	 * measure the len_v3v3() from the input coord to hit.co */
	BVHTreeRayHit hit;

	/* auxiliary target */
	Mesh *auxMesh = NULL;
	bool auxMesh_free;
	ShrinkwrapTreeData *aux_tree = NULL;
	ShrinkwrapTreeData aux_tree_stack;
	SpaceTransform local2aux;

	/* If the user doesn't allows to project in any direction of projection axis
	 * then there's nothing todo. */
	if ((calc->smd->shrinkOpts & (MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)) == 0)
		return;

	/* Prepare data to retrieve the direction in which we should project each vertex */
	if (calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL) {
		if (calc->vert == NULL) return;
	}
	else {
		/* The code supports any axis that is a combination of X,Y,Z
		 * although currently UI only allows to set the 3 different axis */
		if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) proj_axis[0] = 1.0f;
		if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) proj_axis[1] = 1.0f;
		if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS) proj_axis[2] = 1.0f;

		normalize_v3(proj_axis);

		/* Invalid projection direction */
		if (len_squared_v3(proj_axis) < FLT_EPSILON) {
			return;
		}
	}

	if (calc->smd->auxTarget) {
		auxMesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(calc->smd->auxTarget, &auxMesh_free);
		if (!auxMesh)
			return;
		BLI_SPACE_TRANSFORM_SETUP(&local2aux, calc->ob, calc->smd->auxTarget);
	}

	if (BKE_shrinkwrap_init_tree(&aux_tree_stack, auxMesh, calc->smd->shrinkType, calc->smd->shrinkMode, false)) {
		aux_tree = &aux_tree_stack;
	}

	/* After successfully build the trees, start projection vertices. */
	ShrinkwrapCalcCBData data = {
		.calc = calc, .tree = calc->tree, .aux_tree = aux_tree,
		.proj_axis = proj_axis, .local2aux = &local2aux
	};
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
	settings.userdata_chunk = &hit;
	settings.userdata_chunk_size = sizeof(hit);
	BLI_task_parallel_range(0, calc->numVerts,
	                        &data,
	                        shrinkwrap_calc_normal_projection_cb_ex,
	                        &settings);

	/* free data structures */
	if (aux_tree) {
		BKE_shrinkwrap_free_tree(aux_tree);
	}
	if (auxMesh != NULL && auxMesh_free) {
		BKE_id_free(NULL, auxMesh);
	}
}

/*
 * Shrinkwrap moving vertexs to the nearest surface point on the target
 *
 * it builds a BVHTree from the target mesh and then performs a
 * NN matches for each vertex
 */
static void shrinkwrap_calc_nearest_surface_point_cb_ex(
        void *__restrict userdata,
        const int i,
        const ParallelRangeTLS *__restrict tls)
{
	ShrinkwrapCalcCBData *data = userdata;

	ShrinkwrapCalcData *calc = data->calc;
	BVHTreeFromMesh *treeData = &data->tree->treeData;
	BVHTreeNearest *nearest = tls->userdata_chunk;

	float *co = calc->vertexCos[i];
	float tmp_co[3];
	float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

	if (calc->invert_vgroup) {
		weight = 1.0f - weight;
	}

	if (weight == 0.0f) {
		return;
	}

	/* Convert the vertex to tree coordinates */
	if (calc->vert) {
		copy_v3_v3(tmp_co, calc->vert[i].co);
	}
	else {
		copy_v3_v3(tmp_co, co);
	}
	BLI_space_transform_apply(&calc->local2target, tmp_co);

	/* Use local proximity heuristics (to reduce the nearest search)
	 *
	 * If we already had an hit before.. we assume this vertex is going to have a close hit to that other vertex
	 * so we can initiate the "nearest.dist" with the expected value to that last hit.
	 * This will lead in pruning of the search tree. */
	if (nearest->index != -1)
		nearest->dist_sq = len_squared_v3v3(tmp_co, nearest->co);
	else
		nearest->dist_sq = FLT_MAX;

	BLI_bvhtree_find_nearest(treeData->tree, tmp_co, nearest, treeData->nearest_callback, treeData);

	/* Found the nearest vertex */
	if (nearest->index != -1) {
		BKE_shrinkwrap_snap_point_to_surface(
		        data->tree, NULL, calc->smd->shrinkMode,
		        nearest->index, nearest->co, nearest->no, calc->keepDist, tmp_co, tmp_co);

		/* Convert the coordinates back to mesh coordinates */
		BLI_space_transform_invert(&calc->local2target, tmp_co);
		interp_v3_v3v3(co, co, tmp_co, weight);  /* linear interpolation */
	}
}

/**
 * Compute a smooth normal of the target (if applicable) at the hit location.
 *
 * \param tree: information about the mesh
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_no: output in hit coordinate space; may be shared with inputs
 */
void BKE_shrinkwrap_compute_smooth_normal(
        const struct ShrinkwrapTreeData *tree, const struct SpaceTransform *transform,
        int looptri_idx, const float hit_co[3], const float hit_no[3], float r_no[3])
{
	const BVHTreeFromMesh *treeData = &tree->treeData;
	const MLoopTri *tri = &treeData->looptri[looptri_idx];

	/* Interpolate smooth normals if enabled. */
	if ((tree->mesh->mpoly[tri->poly].flag & ME_SMOOTH) != 0) {
		const MVert *verts[] = {
			&treeData->vert[treeData->loop[tri->tri[0]].v],
			&treeData->vert[treeData->loop[tri->tri[1]].v],
			&treeData->vert[treeData->loop[tri->tri[2]].v],
		};
		float w[3], no[3][3], tmp_co[3];

		/* Custom and auto smooth split normals. */
		if (tree->clnors) {
			copy_v3_v3(no[0], tree->clnors[tri->tri[0]]);
			copy_v3_v3(no[1], tree->clnors[tri->tri[1]]);
			copy_v3_v3(no[2], tree->clnors[tri->tri[2]]);
		}
		/* Ordinary vertex normals. */
		else {
			normal_short_to_float_v3(no[0], verts[0]->no);
			normal_short_to_float_v3(no[1], verts[1]->no);
			normal_short_to_float_v3(no[2], verts[2]->no);
		}

		/* Barycentric weights from hit point. */
		copy_v3_v3(tmp_co, hit_co);

		if (transform) {
			BLI_space_transform_apply(transform, tmp_co);
		}

		interp_weights_tri_v3(w, verts[0]->co, verts[1]->co, verts[2]->co, tmp_co);

		/* Interpolate using weights. */
		interp_v3_v3v3v3(r_no, no[0], no[1], no[2], w);

		if (transform) {
			BLI_space_transform_invert_normal(transform, r_no);
		}
		else {
			normalize_v3(r_no);
		}
	}
	/* Use the looptri normal if flat. */
	else {
		copy_v3_v3(r_no, hit_no);
	}
}

/* Helper for MOD_SHRINKWRAP_INSIDE, MOD_SHRINKWRAP_OUTSIDE and MOD_SHRINKWRAP_OUTSIDE_SURFACE. */
static void shrinkwrap_snap_with_side(float r_point_co[3], const float point_co[3], const float hit_co[3], const float hit_no[3], float goal_dist, float forcesign, bool forcesnap)
{
	float dist = len_v3v3(point_co, hit_co);

	/* If exactly on the surface, push out along normal */
	if (dist < FLT_EPSILON) {
		madd_v3_v3v3fl(r_point_co, hit_co, hit_no, goal_dist * forcesign);
	}
	/* Move to the correct side if needed */
	else {
		float delta[3];
		sub_v3_v3v3(delta, point_co, hit_co);
		float dsign = signf(dot_v3v3(delta, hit_no));

		/* If on the wrong side or too close, move to correct */
		if (forcesnap || dsign * forcesign < 0 || dist < goal_dist) {
			interp_v3_v3v3(r_point_co, point_co, hit_co, (dist - goal_dist * dsign * forcesign) / dist);
		}
		else {
			copy_v3_v3(r_point_co, point_co);
		}
	}
}

/**
 * Apply the shrink to surface modes to the given original coordinates and nearest point.
 *
 * \param tree: mesh data for smooth normals
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_point_co: may be the same memory location as point_co, hit_co, or hit_no.
 */
void BKE_shrinkwrap_snap_point_to_surface(
        const struct ShrinkwrapTreeData *tree, const struct SpaceTransform *transform,
        int mode, int hit_idx, const float hit_co[3], const float hit_no[3], float goal_dist,
        const float point_co[3], float r_point_co[3])
{
	float dist, tmp[3];

	switch (mode) {
		/* Offsets along the line between point_co and hit_co. */
		case MOD_SHRINKWRAP_ON_SURFACE:
			if (goal_dist > 0 && (dist = len_v3v3(point_co, hit_co)) > FLT_EPSILON) {
				interp_v3_v3v3(r_point_co, point_co, hit_co, (dist - goal_dist) / dist);
			}
			else {
				copy_v3_v3(r_point_co, hit_co);
			}
			break;

		case MOD_SHRINKWRAP_INSIDE:
			shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, -1, false);
			break;

		case MOD_SHRINKWRAP_OUTSIDE:
			shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, +1, false);
			break;

		case MOD_SHRINKWRAP_OUTSIDE_SURFACE:
			if (goal_dist > 0) {
				shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, +1, true);
			}
			else {
				copy_v3_v3(r_point_co, hit_co);
			}
			break;

		/* Offsets along the normal */
		case MOD_SHRINKWRAP_ABOVE_SURFACE:
			if (goal_dist > 0) {
				BKE_shrinkwrap_compute_smooth_normal(tree, transform, hit_idx, hit_co, hit_no, tmp);
				madd_v3_v3v3fl(r_point_co, hit_co, tmp, goal_dist);
			}
			else {
				copy_v3_v3(r_point_co, hit_co);
			}
			break;

		default:
			printf("Unknown Shrinkwrap surface snap mode: %d\n", mode);
			copy_v3_v3(r_point_co, hit_co);
	}
}

static void shrinkwrap_calc_nearest_surface_point(ShrinkwrapCalcData *calc)
{
	BVHTreeNearest nearest  = NULL_BVHTreeNearest;

	/* Setup nearest */
	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;

	/* Find the nearest vertex */
	ShrinkwrapCalcCBData data = {.calc = calc, .tree = calc->tree};
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
	settings.userdata_chunk = &nearest;
	settings.userdata_chunk_size = sizeof(nearest);
	BLI_task_parallel_range(0, calc->numVerts,
	                        &data,
	                        shrinkwrap_calc_nearest_surface_point_cb_ex,
	                        &settings);
}

/* Main shrinkwrap function */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd, struct Scene *scene, Object *ob, Mesh *mesh,
                               float (*vertexCos)[3], int numVerts)
{

	DerivedMesh *ss_mesh    = NULL;
	ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;
	bool target_free;

	/* remove loop dependencies on derived meshes (TODO should this be done elsewhere?) */
	if (smd->target == ob) smd->target = NULL;
	if (smd->auxTarget == ob) smd->auxTarget = NULL;


	/* Configure Shrinkwrap calc data */
	calc.smd = smd;
	calc.ob = ob;
	calc.numVerts = numVerts;
	calc.vertexCos = vertexCos;
	calc.invert_vgroup = (smd->shrinkOpts & MOD_SHRINKWRAP_INVERT_VGROUP) != 0;

	/* DeformVertex */
	calc.vgroup = defgroup_name_index(calc.ob, calc.smd->vgroup_name);
	if (mesh) {
		calc.dvert = mesh->dvert;
	}
	else if (calc.ob->type == OB_LATTICE) {
		calc.dvert = BKE_lattice_deform_verts_get(calc.ob);
	}


	if (smd->target) {
		calc.target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(smd->target, &target_free);

		/* TODO there might be several "bugs" on non-uniform scales matrixs
		 * because it will no longer be nearest surface, not sphere projection
		 * because space has been deformed */
		BLI_SPACE_TRANSFORM_SETUP(&calc.local2target, ob, smd->target);

		/* TODO: smd->keepDist is in global units.. must change to local */
		calc.keepDist = smd->keepDist;
	}



	calc.vgroup = defgroup_name_index(calc.ob, smd->vgroup_name);

	if (mesh != NULL && smd->shrinkType == MOD_SHRINKWRAP_PROJECT) {
		/* Setup arrays to get vertexs positions, normals and deform weights */
		calc.vert   = mesh->mvert;
		calc.dvert  = mesh->dvert;

		/* Using vertexs positions/normals as if a subsurface was applied */
		if (smd->subsurfLevels) {
			SubsurfModifierData ssmd = {{NULL}};
			ssmd.subdivType = ME_CC_SUBSURF;        /* catmull clark */
			ssmd.levels     = smd->subsurfLevels;   /* levels */

			/* TODO to be moved to Mesh once we are done with changes in subsurf code. */
			DerivedMesh *dm = CDDM_from_mesh(mesh);

			ss_mesh = subsurf_make_derived_from_derived(dm, &ssmd, scene, NULL, (ob->mode & OB_MODE_EDIT) ? SUBSURF_IN_EDIT_MODE : 0);

			if (ss_mesh) {
				calc.vert = ss_mesh->getVertDataArray(ss_mesh, CD_MVERT);
				if (calc.vert) {
					/* TRICKY: this code assumes subsurface will have the transformed original vertices
					 * in their original order at the end of the vert array. */
					calc.vert = calc.vert + ss_mesh->getNumVerts(ss_mesh) - dm->getNumVerts(dm);
				}
			}

			/* Just to make sure we are not leaving any memory behind */
			BLI_assert(ssmd.emCache == NULL);
			BLI_assert(ssmd.mCache == NULL);

			dm->release(dm);
		}
	}

	/* Projecting target defined - lets work! */
	ShrinkwrapTreeData tree;

	if (BKE_shrinkwrap_init_tree(&tree, calc.target, smd->shrinkType, smd->shrinkMode, false)) {
		calc.tree = &tree;

		switch (smd->shrinkType) {
			case MOD_SHRINKWRAP_NEAREST_SURFACE:
				TIMEIT_BENCH(shrinkwrap_calc_nearest_surface_point(&calc), deform_surface);
				break;

			case MOD_SHRINKWRAP_PROJECT:
				TIMEIT_BENCH(shrinkwrap_calc_normal_projection(&calc), deform_project);
				break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				TIMEIT_BENCH(shrinkwrap_calc_nearest_vertex(&calc), deform_vertex);
				break;
		}

		BKE_shrinkwrap_free_tree(&tree);
	}

	/* free memory */
	if (ss_mesh)
		ss_mesh->release(ss_mesh);

	if (target_free && calc.target) {
		BKE_id_free(NULL, calc.target);
	}
}
