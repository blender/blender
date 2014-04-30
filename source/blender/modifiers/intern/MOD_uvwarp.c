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
 * Contributor(s): Pawel Kowal, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_uvwarp.c
 *  \ingroup modifiers
 */

#include <string.h>

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"  /* BKE_pose_channel_find_name */
#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_util.h"


static void uv_warp_from_mat4_pair(float uv_dst[2], const float uv_src[2], float warp_mat[4][4],
                                   int axis_u, int axis_v)
{
	float tuv[3] = {0.0f};

	tuv[axis_u] = uv_src[0];
	tuv[axis_v] = uv_src[1];

	mul_m4_v3(warp_mat, tuv);

	uv_dst[0] = tuv[axis_u];
	uv_dst[1] = tuv[axis_v];
}

static void initData(ModifierData *md)
{
	UVWarpModifierData *umd = (UVWarpModifierData *) md;
	umd->axis_u = 0;
	umd->axis_v = 1;
	copy_v2_fl(umd->center, 0.5f);
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	UVWarpModifierData *umd  = (UVWarpModifierData *)md;
	UVWarpModifierData *tumd = (UVWarpModifierData *)target;
#endif
	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	UVWarpModifierData *umd = (UVWarpModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (umd->vgroup_name[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void matrix_from_obj_pchan(float mat[4][4], Object *ob, const char *bonename)
{
	bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bonename);
	if (pchan) {
		mul_m4_m4m4(mat, ob->obmat, pchan->pose_mat);
	}
	else {
		copy_m4_m4(mat, ob->obmat);
	}
}

#ifdef _OPENMP
#  define OMP_LIMIT 1000
#endif

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	UVWarpModifierData *umd = (UVWarpModifierData *) md;
	int i, numPolys, numLoops;
	MPoly *mpoly;
	MLoop *mloop;
	MLoopUV *mloopuv;
	MDeformVert *dvert;
	int defgrp_index;
	char uvname[MAX_CUSTOMDATA_LAYER_NAME];
	float mat_src[4][4];
	float mat_dst[4][4];
	float imat_dst[4][4];
	float warp_mat[4][4];
	const int axis_u = umd->axis_u;
	const int axis_v = umd->axis_v;

	/* make sure there are UV Maps available */
	if (!CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
		return dm;
	}
	else if (ELEM(NULL, umd->object_src, umd->object_dst)) {
		modifier_setError(md, "From/To objects must be set");
		return dm;
	}

	/* make sure anything moving UVs is available */
	matrix_from_obj_pchan(mat_src, umd->object_src, umd->bone_src);
	matrix_from_obj_pchan(mat_dst, umd->object_dst, umd->bone_dst);

	invert_m4_m4(imat_dst, mat_dst);
	mul_m4_m4m4(warp_mat, imat_dst, mat_src);

	/* apply warp */
	if (!is_zero_v2(umd->center)) {
		float mat_cent[4][4];
		float imat_cent[4][4];

		unit_m4(mat_cent);
		mat_cent[3][axis_u] = umd->center[0];
		mat_cent[3][axis_v] = umd->center[1];

		invert_m4_m4(imat_cent, mat_cent);

		mul_m4_m4m4(warp_mat, warp_mat, imat_cent);
		mul_m4_m4m4(warp_mat, mat_cent, warp_mat);
	}

	/* make sure we're using an existing layer */
	CustomData_validate_layer_name(&dm->loopData, CD_MLOOPUV, umd->uvlayer_name, uvname);

	numPolys = dm->getNumPolys(dm);
	numLoops = dm->getNumLoops(dm);

	mpoly = dm->getPolyArray(dm);
	mloop = dm->getLoopArray(dm);
	/* make sure we are not modifying the original UV map */
	mloopuv = CustomData_duplicate_referenced_layer_named(&dm->loopData, CD_MLOOPUV, uvname, numLoops);
	modifier_get_vgroup(ob, dm, umd->vgroup_name, &dvert, &defgrp_index);

	if (dvert) {
#pragma omp parallel for if (numPolys > OMP_LIMIT)
		for (i = 0; i < numPolys; i++) {
			float uv[2];
			MPoly *mp     = &mpoly[i];
			MLoop *ml     = &mloop[mp->loopstart];
			MLoopUV *mluv = &mloopuv[mp->loopstart];
			int l;
			for (l = 0; l < mp->totloop; l++, ml++, mluv++) {
				const float weight = defvert_find_weight(&dvert[ml->v], defgrp_index);
				uv_warp_from_mat4_pair(uv, mluv->uv, warp_mat, axis_u, axis_v);
				interp_v2_v2v2(mluv->uv, mluv->uv, uv, weight);
			}
		}
	}
	else {
#pragma omp parallel for if (numPolys > OMP_LIMIT)
		for (i = 0; i < numPolys; i++) {
			MPoly *mp     = &mpoly[i];
			// MLoop *ml     = &mloop[mp->loopstart];
			MLoopUV *mluv = &mloopuv[mp->loopstart];
			int l;
			for (l = 0; l < mp->totloop; l++, /* ml++, */ mluv++) {
				uv_warp_from_mat4_pair(mluv->uv, mluv->uv, warp_mat, axis_u, axis_v);
			}
		}
	}

	dm->dirty |= DM_DIRTY_TESS_CDLAYERS;

	return dm;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
	UVWarpModifierData *umd = (UVWarpModifierData *) md;

	walk(userData, ob, &umd->object_dst);
	walk(userData, ob, &umd->object_src);
}

static void uv_warp_deps_object_bone(DagForest *forest, DagNode *obNode,
                                Object *obj, const char *bonename)
{
	if (obj) {
		DagNode *curNode = dag_get_node(forest, obj);

		if (bonename[0])
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "UVWarp Modifier");
		else
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA, "UVWarp Modifier");
	}
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	UVWarpModifierData *umd = (UVWarpModifierData *) md;

	uv_warp_deps_object_bone(forest, obNode, umd->object_src, umd->bone_src);
	uv_warp_deps_object_bone(forest, obNode, umd->object_dst, umd->bone_dst);
}

ModifierTypeInfo modifierType_UVWarp = {
	/* name */              "UVWarp",
	/* structName */        "UVWarpModifierData",
	/* structSize */        sizeof(UVWarpModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,
	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
