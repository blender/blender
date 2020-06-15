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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void uv_warp_from_mat4_pair(float uv_dst[2], const float uv_src[2], float warp_mat[4][4])
{
  float tuv[3] = {0.0f};

  copy_v2_v2(tuv, uv_src);
  mul_m4_v3(warp_mat, tuv);
  copy_v2_v2(uv_dst, tuv);
}

static void initData(ModifierData *md)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;
  umd->axis_u = 0;
  umd->axis_v = 1;
  copy_v2_fl(umd->center, 0.5f);
  copy_v2_fl(umd->scale, 1.0f);
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
  bool invert_vgroup;
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

  int l;

  if (dvert) {
    for (l = 0; l < mp->totloop; l++, ml++, mluv++) {
      float uv[2];
      const float weight = data->invert_vgroup ?
                               1.0f - BKE_defvert_find_weight(&dvert[ml->v], defgrp_index) :
                               BKE_defvert_find_weight(&dvert[ml->v], defgrp_index);

      uv_warp_from_mat4_pair(uv, mluv->uv, warp_mat);
      interp_v2_v2v2(mluv->uv, mluv->uv, uv, weight);
    }
  }
  else {
    for (l = 0; l < mp->totloop; l++, ml++, mluv++) {
      uv_warp_from_mat4_pair(mluv->uv, mluv->uv, warp_mat);
    }
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;
  int numPolys, numLoops;
  MPoly *mpoly;
  MLoop *mloop;
  MLoopUV *mloopuv;
  MDeformVert *dvert;
  int defgrp_index;
  char uvname[MAX_CUSTOMDATA_LAYER_NAME];
  float warp_mat[4][4];
  const int axis_u = umd->axis_u;
  const int axis_v = umd->axis_v;
  const bool invert_vgroup = (umd->flag & MOD_UVWARP_INVERT_VGROUP) != 0;

  /* make sure there are UV Maps available */
  if (!CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
    return mesh;
  }

  if (!ELEM(NULL, umd->object_src, umd->object_dst)) {
    float mat_src[4][4];
    float mat_dst[4][4];
    float imat_dst[4][4];
    float shuf_mat[4][4];

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

    int shuf_indices[4] = {axis_u, axis_v, -1, 3};
    shuffle_m4(shuf_mat, shuf_indices);
    mul_m4_m4m4(warp_mat, shuf_mat, warp_mat);
    transpose_m4(shuf_mat);
    mul_m4_m4m4(warp_mat, warp_mat, shuf_mat);
  }
  else {
    unit_m4(warp_mat);
  }

  /* Apply direct 2d transform. */
  translate_m4(warp_mat, umd->center[0], umd->center[1], 0.0f);
  const float scale[3] = {umd->scale[0], umd->scale[1], 1.0f};
  rescale_m4(warp_mat, scale);
  rotate_m4(warp_mat, 'Z', umd->rotation);
  translate_m4(warp_mat, umd->offset[0], umd->offset[1], 0.0f);
  translate_m4(warp_mat, -umd->center[0], -umd->center[1], 0.0f);

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
      .invert_vgroup = invert_vgroup,
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

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  MOD_depsgraph_update_object_bone_relation(
      ctx->node, umd->object_src, umd->bone_src, "UVWarp Modifier");
  MOD_depsgraph_update_object_bone_relation(
      ctx->node, umd->object_dst, umd->bone_dst, "UVWarp Modifier");

  DEG_add_modifier_to_transform_relation(ctx->node, "UVWarp Modifier");
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  PointerRNA warp_obj_ptr;
  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, &ptr, "uv_layer", &obj_data_ptr, "uv_layers", NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "center", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "axis_u", 0, IFACE_("Axis U"), ICON_NONE);
  uiItemR(col, &ptr, "axis_v", 0, IFACE_("V"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "object_from", 0, NULL, ICON_NONE);
  warp_obj_ptr = RNA_pointer_get(&ptr, "object_from");
  if (!RNA_pointer_is_null(&warp_obj_ptr) && RNA_enum_get(&warp_obj_ptr, "type") == OB_ARMATURE) {
    PointerRNA warp_obj_data_ptr = RNA_pointer_get(&warp_obj_ptr, "data");
    uiItemPointerR(col, &ptr, "bone_from", &warp_obj_data_ptr, "bones", NULL, ICON_NONE);
  }

  uiItemR(col, &ptr, "object_to", 0, IFACE_("To"), ICON_NONE);
  warp_obj_ptr = RNA_pointer_get(&ptr, "object_to");
  if (!RNA_pointer_is_null(&warp_obj_ptr) && RNA_enum_get(&warp_obj_ptr, "type") == OB_ARMATURE) {
    PointerRNA warp_obj_data_ptr = RNA_pointer_get(&warp_obj_ptr, "data");
    uiItemPointerR(col, &ptr, "bone_to", &warp_obj_data_ptr, "bones", NULL, ICON_NONE);
  }

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  modifier_panel_end(layout, &ptr);
}

static void transform_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "offset", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "scale", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "rotation", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_UVWarp, panel_draw);
  modifier_subpanel_register(
      region_type, "offset", "Transform", NULL, transform_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_UVWarp = {
    /* name */ "UVWarp",
    /* structName */ "UVWarpModifierData",
    /* structSize */ sizeof(UVWarpModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

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
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
