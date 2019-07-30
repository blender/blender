/*
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
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_deform.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph_query.h"

#include "MOD_util.h"

static void uv_warp_from_mat4_pair(
    float uv_dst[2], const float uv_src[2], float warp_mat[4][4], int axis_u, int axis_v)
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
  UVWarpModifierData *umd = (UVWarpModifierData *)md;
  umd->axis_u = 0;
  umd->axis_v = 1;
  copy_v2_fl(umd->center, 0.5f);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (umd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
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

typedef struct UVWarpData {
  MPoly *mpoly;
  MLoop *mloop;
  MLoopUV *mloopuv;

  MDeformVert *dvert;
  int defgrp_index;

  float (*warp_mat)[4];
  int axis_u;
  int axis_v;
} UVWarpData;

static void uv_warp_compute(void *__restrict userdata,
                            const int i,
                            const TaskParallelTLS *__restrict UNUSED(tls))
{
  const UVWarpData *data = userdata;

  const MPoly *mp = &data->mpoly[i];
  const MLoop *ml = &data->mloop[mp->loopstart];
  MLoopUV *mluv = &data->mloopuv[mp->loopstart];

  const MDeformVert *dvert = data->dvert;
  const int defgrp_index = data->defgrp_index;

  float(*warp_mat)[4] = data->warp_mat;
  const int axis_u = data->axis_u;
  const int axis_v = data->axis_v;

  int l;

  if (dvert) {
    for (l = 0; l < mp->totloop; l++, ml++, mluv++) {
      float uv[2];
      const float weight = defvert_find_weight(&dvert[ml->v], defgrp_index);

      uv_warp_from_mat4_pair(uv, mluv->uv, warp_mat, axis_u, axis_v);
      interp_v2_v2v2(mluv->uv, mluv->uv, uv, weight);
    }
  }
  else {
    for (l = 0; l < mp->totloop; l++, ml++, mluv++) {
      uv_warp_from_mat4_pair(mluv->uv, mluv->uv, warp_mat, axis_u, axis_v);
    }
  }
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;
  int numPolys, numLoops;
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
  if (!CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
    return mesh;
  }
  else if (ELEM(NULL, umd->object_src, umd->object_dst)) {
    modifier_setError(md, "From/To objects must be set");
    return mesh;
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
  CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPUV, umd->uvlayer_name, uvname);

  numPolys = mesh->totpoly;
  numLoops = mesh->totloop;

  mpoly = mesh->mpoly;
  mloop = mesh->mloop;
  /* make sure we are not modifying the original UV map */
  mloopuv = CustomData_duplicate_referenced_layer_named(
      &mesh->ldata, CD_MLOOPUV, uvname, numLoops);
  MOD_get_vgroup(ctx->object, mesh, umd->vgroup_name, &dvert, &defgrp_index);

  UVWarpData data = {
      .mpoly = mpoly,
      .mloop = mloop,
      .mloopuv = mloopuv,
      .dvert = dvert,
      .defgrp_index = defgrp_index,
      .warp_mat = warp_mat,
      .axis_u = axis_u,
      .axis_v = axis_v,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (numPolys > 1000);
  BLI_task_parallel_range(0, numPolys, &data, uv_warp_compute, &settings);

  /* XXX TODO is this still needed? */
  //  me_eval->dirty |= DM_DIRTY_TESS_CDLAYERS;

  return mesh;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  walk(userData, ob, &umd->object_dst, IDWALK_CB_NOP);
  walk(userData, ob, &umd->object_src, IDWALK_CB_NOP);
}

static void uv_warp_deps_object_bone_new(struct DepsNodeHandle *node,
                                         Object *object,
                                         const char *bonename)
{
  if (object != NULL) {
    if (bonename[0]) {
      DEG_add_object_relation(node, object, DEG_OB_COMP_EVAL_POSE, "UVWarp Modifier");
    }
    else {
      DEG_add_object_relation(node, object, DEG_OB_COMP_TRANSFORM, "UVWarp Modifier");
    }
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  uv_warp_deps_object_bone_new(ctx->node, umd->object_src, umd->bone_src);
  uv_warp_deps_object_bone_new(ctx->node, umd->object_dst, umd->bone_dst);

  DEG_add_modifier_to_transform_relation(ctx->node, "UVWarp Modifier");
}

ModifierTypeInfo modifierType_UVWarp = {
    /* name */ "UVWarp",
    /* structName */ "UVWarpModifierData",
    /* structSize */ sizeof(UVWarpModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
