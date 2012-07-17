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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 by Bastien Montagne.
 * All rights reserved.
 *
 * Contributor(s): None yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_weightvgproximity.c
 *  \ingroup modifiers
 */

#define DO_PROFILE 0

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#if DO_PROFILE
	#include "PIL_time.h"
#endif

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_shrinkwrap.h"       /* For SpaceTransform stuff. */
#include "BKE_texture.h"          /* Texture masking. */

#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"
#include "MOD_util.h"
#include "MOD_weightvg_util.h"

/**************************************
 * Util functions.                    *
 **************************************/

/* Util macro. */
#define OUT_OF_MEMORY() ((void)printf("WeightVGProximity: Out of memory.\n"))

/**
 * Find nearest vertex and/or edge and/or face, for each vertex (adapted from shrinkwrap.c).
 */
static void get_vert2geom_distance(int numVerts, float (*v_cos)[3],
                                   float *dist_v, float *dist_e, float *dist_f,
                                   DerivedMesh *target, const SpaceTransform *loc2trgt)
{
	int i;
	BVHTreeFromMesh treeData_v = NULL_BVHTreeFromMesh;
	BVHTreeFromMesh treeData_e = NULL_BVHTreeFromMesh;
	BVHTreeFromMesh treeData_f = NULL_BVHTreeFromMesh;
	BVHTreeNearest nearest_v   = NULL_BVHTreeNearest;
	BVHTreeNearest nearest_e   = NULL_BVHTreeNearest;
	BVHTreeNearest nearest_f   = NULL_BVHTreeNearest;

	if (dist_v) {
		/* Create a bvh-tree of the given target's verts. */
		bvhtree_from_mesh_verts(&treeData_v, target, 0.0, 2, 6);
		if (treeData_v.tree == NULL) {
			OUT_OF_MEMORY();
			return;
		}
	}
	if (dist_e) {
		/* Create a bvh-tree of the given target's edges. */
		bvhtree_from_mesh_edges(&treeData_e, target, 0.0, 2, 6);
		if (treeData_e.tree == NULL) {
			OUT_OF_MEMORY();
			return;
		}
	}
	if (dist_f) {
		/* Create a bvh-tree of the given target's faces. */
		bvhtree_from_mesh_faces(&treeData_f, target, 0.0, 2, 6);
		if (treeData_f.tree == NULL) {
			OUT_OF_MEMORY();
			return;
		}
	}

	/* Setup nearest. */
	nearest_v.index = nearest_e.index = nearest_f.index = -1;
	/*nearest_v.dist  = nearest_e.dist  = nearest_f.dist  = FLT_MAX;*/
	/* Find the nearest vert/edge/face. */
#ifndef __APPLE__
#pragma omp parallel for default(none) private(i) firstprivate(nearest_v,nearest_e,nearest_f) \
                         shared(treeData_v,treeData_e,treeData_f,numVerts,v_cos,dist_v,dist_e, \
                                dist_f,loc2trgt) \
                         schedule(static)
#endif
	for (i = 0; i < numVerts; i++) {
		float tmp_co[3];

		/* Convert the vertex to tree coordinates. */
		copy_v3_v3(tmp_co, v_cos[i]);
		space_transform_apply(loc2trgt, tmp_co);

		/* Use local proximity heuristics (to reduce the nearest search).
		 *
		 * If we already had an hit before, we assume this vertex is going to have a close hit to
		 * that other vertex, so we can initiate the "nearest.dist" with the expected value to that
		 * last hit.
		 * This will lead in prunning of the search tree.
		 */
		if (dist_v) {
			nearest_v.dist = nearest_v.index != -1 ? len_squared_v3v3(tmp_co, nearest_v.co) : FLT_MAX;
			/* Compute and store result. If invalid (-1 idx), keep FLT_MAX dist. */
			BLI_bvhtree_find_nearest(treeData_v.tree, tmp_co, &nearest_v, treeData_v.nearest_callback, &treeData_v);
			dist_v[i] = sqrtf(nearest_v.dist);
		}
		if (dist_e) {
			nearest_e.dist = nearest_e.index != -1 ? len_squared_v3v3(tmp_co, nearest_e.co) : FLT_MAX;
			/* Compute and store result. If invalid (-1 idx), keep FLT_MAX dist. */
			BLI_bvhtree_find_nearest(treeData_e.tree, tmp_co, &nearest_e, treeData_e.nearest_callback, &treeData_e);
			dist_e[i] = sqrtf(nearest_e.dist);
		}
		if (dist_f) {
			nearest_f.dist = nearest_f.index != -1 ? len_squared_v3v3(tmp_co, nearest_f.co) : FLT_MAX;
			/* Compute and store result. If invalid (-1 idx), keep FLT_MAX dist. */
			BLI_bvhtree_find_nearest(treeData_f.tree, tmp_co, &nearest_f, treeData_f.nearest_callback, &treeData_f);
			dist_f[i] = sqrtf(nearest_f.dist);
		}
	}

	if (dist_v)
		free_bvhtree_from_mesh(&treeData_v);
	if (dist_e)
		free_bvhtree_from_mesh(&treeData_e);
	if (dist_f)
		free_bvhtree_from_mesh(&treeData_f);
}

/**
 * Returns the real distance between a vertex and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static void get_vert2ob_distance(int numVerts, float (*v_cos)[3], float *dist,
                                 Object *ob, Object *obr)
{
	/* Vertex and ref object coordinates. */
	float v_wco[3];
	unsigned int i = numVerts;

	while (i-- > 0) {
		/* Get world-coordinates of the vertex (constraints and anim included). */
		mul_v3_m4v3(v_wco, ob->obmat, v_cos[i]);
		/* Return distance between both coordinates. */
		dist[i] = len_v3v3(v_wco, obr->obmat[3]);
	}
}

/**
 * Returns the real distance between an object and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static float get_ob2ob_distance(const Object *ob, const Object *obr)
{
	return len_v3v3(ob->obmat[3], obr->obmat[3]); 
}

/**
 * Maps distances to weights, with an optional "smoothing" mapping.
 */
void do_map(float *weights, const int nidx, const float min_d, const float max_d, short mode)
{
	const float range_inv = 1.0f / (max_d - min_d); /* invert since multiplication is faster */
	unsigned int i = nidx;
	if (max_d == min_d) {
		while (i-- > 0) {
			weights[i] = (weights[i] >= max_d) ? 1.0f : 0.0f; /* "Step" behavior... */
		}
	}
	else if (max_d > min_d) {
		while (i-- > 0) {
			if     (weights[i] >= max_d) weights[i] = 1.0f;  /* most likely case first */
			else if (weights[i] <= min_d) weights[i] = 0.0f;
			else weights[i] = (weights[i] - min_d) * range_inv;
		}
	}
	else {
		while (i-- > 0) {
			if     (weights[i] <= max_d) weights[i] = 1.0f;  /* most likely case first */
			else if (weights[i] >= min_d) weights[i] = 0.0f;
			else weights[i] = (weights[i] - min_d) * range_inv;
		}
	}

	if (!ELEM(mode, MOD_WVG_MAPPING_NONE, MOD_WVG_MAPPING_CURVE)) {
		weightvg_do_map(nidx, weights, mode, NULL);
	}
}

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;

	wmd->proximity_mode       = MOD_WVG_PROXIMITY_OBJECT;
	wmd->proximity_flags      = MOD_WVG_PROXIMITY_GEOM_VERTS;

	wmd->falloff_type         = MOD_WVG_MAPPING_NONE;

	wmd->mask_constant        = 1.0f;
	wmd->mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT; /* Use intensity by default. */
	wmd->mask_tex_mapping     = MOD_DISP_MAP_LOCAL;
	wmd->max_dist             = 1.0f; /* vert arbitrary distance, but don't use 0 */
}

static void copyData(ModifierData *md, ModifierData *target)
{
	WeightVGProximityModifierData *wmd  = (WeightVGProximityModifierData *) md;
	WeightVGProximityModifierData *twmd = (WeightVGProximityModifierData *) target;

	BLI_strncpy(twmd->defgrp_name, wmd->defgrp_name, sizeof(twmd->defgrp_name));
	twmd->proximity_mode         = wmd->proximity_mode;
	twmd->proximity_flags        = wmd->proximity_flags;
	twmd->proximity_ob_target    = wmd->proximity_ob_target;

	twmd->falloff_type           = wmd->falloff_type;

	twmd->mask_constant          = wmd->mask_constant;
	BLI_strncpy(twmd->mask_defgrp_name, wmd->mask_defgrp_name, sizeof(twmd->mask_defgrp_name));
	twmd->mask_texture           = wmd->mask_texture;
	twmd->mask_tex_use_channel   = wmd->mask_tex_use_channel;
	twmd->mask_tex_mapping       = wmd->mask_tex_mapping;
	twmd->mask_tex_map_obj       = wmd->mask_tex_map_obj;
	BLI_strncpy(twmd->mask_tex_uvlayer_name, wmd->mask_tex_uvlayer_name, sizeof(twmd->mask_tex_uvlayer_name));
	twmd->min_dist               = wmd->min_dist;
	twmd->max_dist               = wmd->max_dist;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;
	CustomDataMask dataMask = 0;

	/* We need vertex groups! */
	dataMask |= CD_MASK_MDEFORMVERT;

	/* Ask for UV coordinates if we need them. */
	if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV)
		dataMask |= CD_MASK_MTFACE;

	/* No need to ask for CD_PREVIEW_MLOOPCOL... */

	return dataMask;
}

static int dependsOnTime(ModifierData *md)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;

	if (wmd->mask_texture)
		return BKE_texture_dependsOnTime(wmd->mask_texture);
	return 0;
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              void (*walk)(void *userData, Object *ob, Object **obpoin),
                              void *userData)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;
	walk(userData, ob, &wmd->proximity_ob_target);
	walk(userData, ob, &wmd->mask_tex_map_obj);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;

	walk(userData, ob, (ID **)&wmd->mask_texture);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "mask_texture");
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *UNUSED(scene),
                           Object *UNUSED(ob), DagNode *obNode)
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;
	DagNode *curNode;

	if (wmd->proximity_ob_target) {
		curNode = dag_get_node(forest, wmd->proximity_ob_target);
		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGProximity Modifier");
	}

	if (wmd->mask_tex_map_obj && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
		curNode = dag_get_node(forest, wmd->mask_tex_map_obj);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGProximity Modifier");
	}

	if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL)
		dag_add_relation(forest, obNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGProximity Modifier");
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;
	/* If no vertex group, bypass. */
	if (wmd->defgrp_name[0] == '\0') return 1;
	/* If no target object, bypass. */
	return (wmd->proximity_ob_target == NULL);
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *) md;
	DerivedMesh *dm = derivedData;
	MDeformVert *dvert = NULL;
	MDeformWeight **dw, **tdw;
	int numVerts;
	float (*v_cos)[3] = NULL; /* The vertices coordinates. */
	Object *obr = NULL; /* Our target object. */
	int defgrp_idx;
	float *tw = NULL;
	float *org_w = NULL;
	float *new_w = NULL;
	int *tidx, *indices = NULL;
	int numIdx = 0;
	int i;
	/* Flags. */
#if 0
	int do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview);
#endif

#if DO_PROFILE
	TIMEIT_START(perf)
#endif

	/* Get number of verts. */
	numVerts = dm->getNumVerts(dm);

	/* Check if we can just return the original mesh.
	 * Must have verts and therefore verts assigned to vgroups to do anything useful!
	 */
	if ((numVerts == 0) || (ob->defbase.first == NULL))
		return dm;

	/* Get our target object. */
	obr = wmd->proximity_ob_target;
	if (obr == NULL)
		return dm;

	/* Get vgroup idx from its name. */
	defgrp_idx = defgroup_name_index(ob, wmd->defgrp_name);
	if (defgrp_idx < 0)
		return dm;

	dvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MDEFORMVERT, numVerts);
	/* If no vertices were ever added to an object's vgroup, dvert might be NULL.
	 * As this modifier never add vertices to vgroup, just return. */
	if (!dvert)
		return dm;

	/* Find out which vertices to work on (all vertices in vgroup), and get their relevant weight.
	 */
	tidx = MEM_mallocN(sizeof(int) * numVerts, "WeightVGProximity Modifier, tidx");
	tw = MEM_mallocN(sizeof(float) * numVerts, "WeightVGProximity Modifier, tw");
	tdw = MEM_mallocN(sizeof(MDeformWeight *) * numVerts, "WeightVGProximity Modifier, tdw");
	for (i = 0; i < numVerts; i++) {
		MDeformWeight *_dw = defvert_find_index(&dvert[i], defgrp_idx);
		if (_dw) {
			tidx[numIdx] = i;
			tw[numIdx] = _dw->weight;
			tdw[numIdx++] = _dw;
		}
	}
	/* If no vertices found, return org data! */
	if (numIdx == 0) {
		MEM_freeN(tidx);
		MEM_freeN(tw);
		MEM_freeN(tdw);
		return dm;
	}
	if (numIdx != numVerts) {
		indices = MEM_mallocN(sizeof(int) * numIdx, "WeightVGProximity Modifier, indices");
		memcpy(indices, tidx, sizeof(int) * numIdx);
		org_w = MEM_mallocN(sizeof(float) * numIdx, "WeightVGProximity Modifier, org_w");
		memcpy(org_w, tw, sizeof(float) * numIdx);
		dw = MEM_mallocN(sizeof(MDeformWeight *) * numIdx, "WeightVGProximity Modifier, dw");
		memcpy(dw, tdw, sizeof(MDeformWeight *) * numIdx);
		MEM_freeN(tw);
		MEM_freeN(tdw);
	}
	else {
		org_w = tw;
		dw = tdw;
	}
	new_w = MEM_mallocN(sizeof(float) * numIdx, "WeightVGProximity Modifier, new_w");
	MEM_freeN(tidx);

	/* Get our vertex coordinates. */
	v_cos = MEM_mallocN(sizeof(float[3]) * numIdx, "WeightVGProximity Modifier, v_cos");
	if (numIdx != numVerts) {
		/* XXX In some situations, this code can be up to about 50 times more performant
		 *     than simply using getVertCo for each affected vertex...
		 */
		float (*tv_cos)[3] = MEM_mallocN(sizeof(float[3]) * numVerts, "WeightVGProximity Modifier, tv_cos");
		dm->getVertCos(dm, tv_cos);
		for (i = 0; i < numIdx; i++)
			copy_v3_v3(v_cos[i], tv_cos[indices[i]]);
		MEM_freeN(tv_cos);
	}
	else
		dm->getVertCos(dm, v_cos);

	/* Compute wanted distances. */
	if (wmd->proximity_mode == MOD_WVG_PROXIMITY_OBJECT) {
		const float dist = get_ob2ob_distance(ob, obr);
		for (i = 0; i < numIdx; i++)
			new_w[i] = dist;
	}
	else if (wmd->proximity_mode == MOD_WVG_PROXIMITY_GEOMETRY) {
		const short use_trgt_verts = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_VERTS);
		const short use_trgt_edges = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_EDGES);
		const short use_trgt_faces = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_FACES);

		if (use_trgt_verts || use_trgt_edges || use_trgt_faces) {
			DerivedMesh *target_dm = obr->derivedFinal;
			short free_target_dm = FALSE;
			if (!target_dm) {
				if (ELEM3(obr->type, OB_CURVE, OB_SURF, OB_FONT))
					target_dm = CDDM_from_curve(obr);
				else if (obr->type == OB_MESH) {
					Mesh *me = (Mesh *)obr->data;
					if (me->edit_btmesh)
						target_dm = CDDM_from_BMEditMesh(me->edit_btmesh, me, FALSE, FALSE);
					else
						target_dm = CDDM_from_mesh(me, obr);
				}
				free_target_dm = TRUE;
			}

			/* We must check that we do have a valid target_dm! */
			if (target_dm) {
				SpaceTransform loc2trgt;
				float *dists_v = use_trgt_verts ? MEM_mallocN(sizeof(float) * numIdx, "dists_v") : NULL;
				float *dists_e = use_trgt_edges ? MEM_mallocN(sizeof(float) * numIdx, "dists_e") : NULL;
				float *dists_f = use_trgt_faces ? MEM_mallocN(sizeof(float) * numIdx, "dists_f") : NULL;

				space_transform_setup(&loc2trgt, ob, obr);
				get_vert2geom_distance(numIdx, v_cos, dists_v, dists_e, dists_f,
				                       target_dm, &loc2trgt);
				for (i = 0; i < numIdx; i++) {
					new_w[i] = dists_v ? dists_v[i] : FLT_MAX;
					if (dists_e)
						new_w[i] = minf(dists_e[i], new_w[i]);
					if (dists_f)
						new_w[i] = minf(dists_f[i], new_w[i]);
				}
				if (free_target_dm) target_dm->release(target_dm);
				if (dists_v) MEM_freeN(dists_v);
				if (dists_e) MEM_freeN(dists_e);
				if (dists_f) MEM_freeN(dists_f);
			}
			/* Else, fall back to default obj2vert behavior. */
			else {
				get_vert2ob_distance(numIdx, v_cos, new_w, ob, obr);
			}
		}
		else {
			get_vert2ob_distance(numIdx, v_cos, new_w, ob, obr);
		}
	}

	/* Map distances to weights. */
	do_map(new_w, numIdx, wmd->min_dist, wmd->max_dist, wmd->falloff_type);

	/* Do masking. */
	weightvg_do_mask(numIdx, indices, org_w, new_w, ob, dm, wmd->mask_constant,
	                 wmd->mask_defgrp_name, wmd->modifier.scene, wmd->mask_texture,
	                 wmd->mask_tex_use_channel, wmd->mask_tex_mapping,
	                 wmd->mask_tex_map_obj, wmd->mask_tex_uvlayer_name);

	/* Update vgroup. Note we never add nor remove vertices from vgroup here. */
	weightvg_update_vg(dvert, defgrp_idx, dw, numIdx, indices, org_w, FALSE, 0.0f, FALSE, 0.0f);

	/* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
	if (do_prev)
		DM_update_weight_mcol(ob, dm, 0, org_w, numIdx, indices);
#endif

	/* Freeing stuff. */
	MEM_freeN(org_w);
	MEM_freeN(new_w);
	MEM_freeN(dw);
	if (indices)
		MEM_freeN(indices);
	MEM_freeN(v_cos);

#if DO_PROFILE
	TIMEIT_END(perf)
#endif

	/* Return the vgroup-modified mesh. */
	return dm;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, MOD_APPLY_USECACHE);
}


ModifierTypeInfo modifierType_WeightVGProximity = {
	/* name */              "VertexWeightProximity",
	/* structName */        "WeightVGProximityModifierData",
	/* structSize */        sizeof(WeightVGProximityModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
