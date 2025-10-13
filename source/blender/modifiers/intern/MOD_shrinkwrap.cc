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
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_shrinkwrap.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_query.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(ShrinkwrapModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  if (!smd->target || smd->target->type != OB_MESH) {
    return true;
  }
  if (smd->auxTarget && smd->auxTarget->type != OB_MESH) {
    return true;
  }
  return false;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  walk(user_data, ob, (ID **)&smd->target, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&smd->auxTarget, IDWALK_CB_NOP);
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  ShrinkwrapModifierData *swmd = (ShrinkwrapModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  const MDeformVert *dvert = nullptr;
  int defgrp_index = -1;
  MOD_get_vgroup(ctx->object, mesh, swmd->vgroup_name, &dvert, &defgrp_index);

  shrinkwrapModifier_deform(swmd,
                            ctx,
                            scene,
                            ctx->object,
                            mesh,
                            dvert,
                            defgrp_index,
                            reinterpret_cast<float (*)[3]>(positions.data()),
                            positions.size());
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
  if (smd->target != nullptr) {
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    if (smd->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &smd->target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  if (smd->auxTarget != nullptr) {
    DEG_add_object_relation(
        ctx->node, smd->auxTarget, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(
        ctx->node, smd->auxTarget, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    if (smd->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &smd->auxTarget->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Shrinkwrap Modifier");
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  int wrap_method = RNA_enum_get(ptr, "wrap_method");

  layout->prop(ptr, "wrap_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (ELEM(wrap_method,
           MOD_SHRINKWRAP_PROJECT,
           MOD_SHRINKWRAP_NEAREST_SURFACE,
           MOD_SHRINKWRAP_TARGET_PROJECT))
  {
    layout->prop(ptr, "wrap_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    layout->prop(ptr, "project_limit", UI_ITEM_NONE, IFACE_("Limit"), ICON_NONE);
    layout->prop(ptr, "subsurf_levels", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &layout->column(false);
    row = &col->row(true, IFACE_("Axis"));
    row->prop(ptr, "use_project_x", toggles_flag, std::nullopt, ICON_NONE);
    row->prop(ptr, "use_project_y", toggles_flag, std::nullopt, ICON_NONE);
    row->prop(ptr, "use_project_z", toggles_flag, std::nullopt, ICON_NONE);

    col->prop(ptr, "use_negative_direction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "use_positive_direction", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    layout->prop(ptr, "cull_face", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
    col = &layout->column(false);
    col->active_set(RNA_boolean_get(ptr, "use_negative_direction") &&
                    RNA_enum_get(ptr, "cull_face") != 0);
    col->prop(ptr, "use_invert_cull", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    layout->prop(ptr, "auxiliary_target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  layout->prop(ptr, "offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Shrinkwrap, panel_draw);
}

ModifierTypeInfo modifierType_Shrinkwrap = {
    /*idname*/ "Shrinkwrap",
    /*name*/ N_("Shrinkwrap"),
    /*struct_name*/ "ShrinkwrapModifierData",
    /*struct_size*/ sizeof(ShrinkwrapModifierData),
    /*srna*/ &RNA_ShrinkwrapModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SHRINKWRAP,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
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
