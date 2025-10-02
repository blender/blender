/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_curve.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  CurveModifierData *cmd = (CurveModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cmd, modifier));

  MEMCPY_STRUCT_AFTER(cmd, DNA_struct_default_get(CurveModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  CurveModifierData *cmd = (CurveModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (cmd->name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  CurveModifierData *cmd = (CurveModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the curve is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !cmd->object || cmd->object->type != OB_CURVES_LEGACY;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  CurveModifierData *cmd = (CurveModifierData *)md;

  walk(user_data, ob, (ID **)&cmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  CurveModifierData *cmd = (CurveModifierData *)md;
  if (cmd->object != nullptr) {
    /* TODO(sergey): Need to do the same eval_flags trick for path
     * as happening in legacy depsgraph callback.
     */
    /* TODO(sergey): Currently path is evaluated as a part of modifier stack,
     * might be changed in the future.
     */
    DEG_add_object_relation(ctx->node, cmd->object, DEG_OB_COMP_TRANSFORM, "Curve Modifier");
    DEG_add_object_relation(ctx->node, cmd->object, DEG_OB_COMP_GEOMETRY, "Curve Modifier");
    DEG_add_special_eval_flag(ctx->node, &cmd->object->id, DAG_EVAL_NEED_CURVE_PATH);
  }

  DEG_add_depends_on_transform_relation(ctx->node, "Curve Modifier");
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  CurveModifierData *cmd = (CurveModifierData *)md;

  const MDeformVert *dvert = nullptr;
  int defgrp_index = -1;
  MOD_get_vgroup(ctx->object, mesh, cmd->name, &dvert, &defgrp_index);

  /* Silly that defaxis and BKE_curve_deform_coords are off by 1
   * but leave for now to save having to call do_versions */
  const int defaxis = std::clamp(cmd->defaxis - 1, 0, 5);
  BKE_curve_deform_coords(cmd->object,
                          ctx->object,
                          reinterpret_cast<float (*)[3]>(positions.data()),
                          positions.size(),
                          dvert,
                          defgrp_index,
                          cmd->flag,
                          defaxis);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            const BMEditMesh *em,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions)
{
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    deform_verts(md, ctx, mesh, positions);
    return;
  }

  CurveModifierData *cmd = (CurveModifierData *)md;
  bool use_dverts = false;
  int defgrp_index = -1;

  if (ctx->object->type == OB_MESH && cmd->name[0] != '\0') {
    defgrp_index = BKE_object_defgroup_name_index(ctx->object, cmd->name);
    if (defgrp_index != -1) {
      use_dverts = true;
    }
  }

  const int defaxis = std::clamp(cmd->defaxis - 1, 0, 5);
  if (use_dverts) {
    BKE_curve_deform_coords_with_editmesh(cmd->object,
                                          ctx->object,
                                          reinterpret_cast<float (*)[3]>(positions.data()),
                                          positions.size(),
                                          defgrp_index,
                                          cmd->flag,
                                          defaxis,
                                          em);
  }
  else {
    BKE_curve_deform_coords(cmd->object,
                            ctx->object,
                            reinterpret_cast<float (*)[3]>(positions.data()),
                            positions.size(),
                            nullptr,
                            defgrp_index,
                            cmd->flag,
                            defaxis);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "object", UI_ITEM_NONE, IFACE_("Curve Object"), ICON_NONE);
  layout->prop(ptr, "deform_axis", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Curve, panel_draw);
}

ModifierTypeInfo modifierType_Curve = {
    /*idname*/ "Curve",
    /*name*/ N_("Curve"),
    /*struct_name*/ "CurveModifierData",
    /*struct_size*/ sizeof(CurveModifierData),
    /*srna*/ &RNA_CurveModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_CURVE,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ deform_verts_EM,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
