/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void uv_warp_from_mat4_pair(float uv_dst[2],
                                   const float uv_src[2],
                                   const float warp_mat[4][4])
{
  float tuv[3] = {0.0f};

  copy_v2_v2(tuv, uv_src);
  mul_m4_v3(warp_mat, tuv);
  copy_v2_v2(uv_dst, tuv);
}

static void init_data(ModifierData *md)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(umd, modifier));

  MEMCPY_STRUCT_AFTER(umd, DNA_struct_default_get(UVWarpModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (umd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void matrix_from_obj_pchan(float mat[4][4], Object *ob, const char *bonename)
{
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bonename);
  if (pchan) {
    mul_m4_m4m4(mat, ob->object_to_world, pchan->pose_mat);
  }
  else {
    copy_m4_m4(mat, ob->object_to_world);
  }
}

struct UVWarpData {
  blender::OffsetIndices<int> faces;
  blender::Span<int> corner_verts;
  float (*mloopuv)[2];

  const MDeformVert *dvert;
  int defgrp_index;

  float (*warp_mat)[4];
  bool invert_vgroup;
};

static void uv_warp_compute(void *__restrict userdata,
                            const int i,
                            const TaskParallelTLS *__restrict /*tls*/)
{
  const UVWarpData *data = static_cast<const UVWarpData *>(userdata);
  const blender::IndexRange face = data->faces[i];
  const blender::Span<int> face_verts = data->corner_verts.slice(face);

  float(*mluv)[2] = &data->mloopuv[face.start()];

  const MDeformVert *dvert = data->dvert;
  const int defgrp_index = data->defgrp_index;

  float(*warp_mat)[4] = data->warp_mat;

  int l;

  if (dvert) {
    for (l = 0; l < face.size(); l++, mluv++) {
      const int vert_i = face_verts[l];
      float uv[2];
      const float weight = data->invert_vgroup ?
                               1.0f - BKE_defvert_find_weight(&dvert[vert_i], defgrp_index) :
                               BKE_defvert_find_weight(&dvert[vert_i], defgrp_index);

      uv_warp_from_mat4_pair(uv, (*mluv), warp_mat);
      interp_v2_v2v2((*mluv), (*mluv), uv, weight);
    }
  }
  else {
    for (l = 0; l < face.size(); l++, mluv++) {
      uv_warp_from_mat4_pair(*mluv, *mluv, warp_mat);
    }
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;
  const MDeformVert *dvert;
  int defgrp_index;
  char uvname[MAX_CUSTOMDATA_LAYER_NAME];
  float warp_mat[4][4];
  const int axis_u = umd->axis_u;
  const int axis_v = umd->axis_v;
  const bool invert_vgroup = (umd->flag & MOD_UVWARP_INVERT_VGROUP) != 0;

  /* make sure there are UV Maps available */
  if (!CustomData_has_layer(&mesh->loop_data, CD_PROP_FLOAT2)) {
    return mesh;
  }

  if (!ELEM(nullptr, umd->object_src, umd->object_dst)) {
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

    const int shuf_indices[4] = {axis_u, axis_v, -1, 3};
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
  CustomData_validate_layer_name(&mesh->loop_data, CD_PROP_FLOAT2, umd->uvlayer_name, uvname);

  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();

  float(*mloopuv)[2] = static_cast<float(*)[2]>(CustomData_get_layer_named_for_write(
      &mesh->loop_data, CD_PROP_FLOAT2, uvname, corner_verts.size()));
  MOD_get_vgroup(ctx->object, mesh, umd->vgroup_name, &dvert, &defgrp_index);

  UVWarpData data{};
  data.faces = faces;
  data.corner_verts = corner_verts;
  data.mloopuv = mloopuv;
  data.dvert = dvert;
  data.defgrp_index = defgrp_index;
  data.warp_mat = warp_mat;
  data.invert_vgroup = invert_vgroup;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (faces.size() > 1000);
  BLI_task_parallel_range(0, faces.size(), &data, uv_warp_compute, &settings);

  mesh->runtime->is_original_bmesh = false;

  return mesh;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  walk(user_data, ob, (ID **)&umd->object_dst, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&umd->object_src, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  UVWarpModifierData *umd = (UVWarpModifierData *)md;

  MOD_depsgraph_update_object_bone_relation(
      ctx->node, umd->object_src, umd->bone_src, "UVWarp Modifier");
  MOD_depsgraph_update_object_bone_relation(
      ctx->node, umd->object_dst, umd->bone_dst, "UVWarp Modifier");

  DEG_add_depends_on_transform_relation(ctx->node, "UVWarp Modifier");
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA warp_obj_ptr;
  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, ptr, "uv_layer", &obj_data_ptr, "uv_layers", nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "center", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "axis_u", UI_ITEM_NONE, IFACE_("Axis U"), ICON_NONE);
  uiItemR(col, ptr, "axis_v", UI_ITEM_NONE, IFACE_("V"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "object_from", UI_ITEM_NONE, nullptr, ICON_NONE);
  warp_obj_ptr = RNA_pointer_get(ptr, "object_from");
  if (!RNA_pointer_is_null(&warp_obj_ptr) && RNA_enum_get(&warp_obj_ptr, "type") == OB_ARMATURE) {
    PointerRNA warp_obj_data_ptr = RNA_pointer_get(&warp_obj_ptr, "data");
    uiItemPointerR(col, ptr, "bone_from", &warp_obj_data_ptr, "bones", nullptr, ICON_NONE);
  }

  uiItemR(col, ptr, "object_to", UI_ITEM_NONE, IFACE_("To"), ICON_NONE);
  warp_obj_ptr = RNA_pointer_get(ptr, "object_to");
  if (!RNA_pointer_is_null(&warp_obj_ptr) && RNA_enum_get(&warp_obj_ptr, "type") == OB_ARMATURE) {
    PointerRNA warp_obj_data_ptr = RNA_pointer_get(&warp_obj_ptr, "data");
    uiItemPointerR(col, ptr, "bone_to", &warp_obj_data_ptr, "bones", nullptr, ICON_NONE);
  }

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void transform_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "offset", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_UVWarp, panel_draw);
  modifier_subpanel_register(
      region_type, "offset", "Transform", nullptr, transform_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_UVWarp = {
    /*idname*/ "UVWarp",
    /*name*/ N_("UVWarp"),
    /*struct_name*/ "UVWarpModifierData",
    /*struct_size*/ sizeof(UVWarpModifierData),
    /*srna*/ &RNA_UVWarpModifier,
    /*type*/ eModifierTypeType_NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_UVPROJECT, /* TODO: Use correct icon. */

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
