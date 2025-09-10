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
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "MOD_solidify_util.hh"

static void init_data(ModifierData *md)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SolidifyModifierData), modifier);
}

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->defgrp_name[0] != '\0' || smd->shell_defgrp_name[0] != '\0' ||
      smd->rim_defgrp_name[0] != '\0')
  {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  switch (smd->mode) {
    case MOD_SOLIDIFY_MODE_EXTRUDE:
      return MOD_solidify_extrude_modifyMesh(md, ctx, mesh);
    case MOD_SOLIDIFY_MODE_NONMANIFOLD:
      return MOD_solidify_nonmanifold_modifyMesh(md, ctx, mesh);
    default:
      BLI_assert_unreachable();
  }
  return mesh;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");
  bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

  layout->use_property_split_set(true);

  layout->prop(ptr, "solidify_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    layout->prop(
        ptr, "nonmanifold_thickness_mode", UI_ITEM_NONE, IFACE_("Thickness Mode"), ICON_NONE);
    layout->prop(ptr, "nonmanifold_boundary_mode", UI_ITEM_NONE, IFACE_("Boundary"), ICON_NONE);
  }

  layout->prop(ptr, "thickness", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    layout->prop(ptr, "nonmanifold_merge_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    layout->prop(ptr, "use_even_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  col = &layout->column(false, CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"));
  col->prop(ptr, "use_rim", UI_ITEM_NONE, CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Fill"), ICON_NONE);
  sub = &col->column(false);
  sub->active_set(RNA_boolean_get(ptr, "use_rim"));
  sub->prop(ptr, "use_rim_only", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);
  row = &layout->row(false);
  row->active_set(has_vertex_group);
  row->prop(ptr, "thickness_vertex_group", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    row = &layout->row(false);
    row->active_set(has_vertex_group);
    row->prop(ptr, "use_flat_faces", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
}

static void normals_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "use_flip_normals", UI_ITEM_NONE, IFACE_("Flip"), ICON_NONE);
  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    col->prop(ptr, "use_quality_normals", UI_ITEM_NONE, IFACE_("High Quality"), ICON_NONE);
  }
}

static void materials_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "material_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col = &layout->column(true);
  col->active_set(RNA_boolean_get(ptr, "use_rim"));
  col->prop(ptr,
            "material_offset_rim",
            UI_ITEM_NONE,
            CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
            ICON_NONE);
}

static void edge_data_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");

  layout->use_property_split_set(true);

  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    uiLayout *col;
    col = &layout->column(true);
    col->prop(ptr, "edge_crease_inner", UI_ITEM_NONE, IFACE_("Crease Inner"), ICON_NONE);
    col->prop(ptr, "edge_crease_outer", UI_ITEM_NONE, IFACE_("Outer"), ICON_NONE);
    col->prop(ptr,
              "edge_crease_rim",
              UI_ITEM_NONE,
              CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
              ICON_NONE);
  }
  layout->prop(ptr, "bevel_convex", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
}

static void clamp_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "thickness_clamp", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row = &col->row(false);
  row->active_set(RNA_float_get(ptr, "thickness_clamp") > 0.0f);
  row->prop(ptr, "use_thickness_angle_clamp", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void vertex_group_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop_search(
      ptr, "shell_vertex_group", &ob_ptr, "vertex_groups", IFACE_("Shell"), ICON_NONE);
  col->prop_search(ptr,
                   "rim_vertex_group",
                   &ob_ptr,
                   "vertex_groups",
                   CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
                   ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Solidify, panel_draw);
  modifier_subpanel_register(
      region_type, "normals", "Normals", nullptr, normals_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "materials", "Materials", nullptr, materials_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "edge_data", "Edge Data", nullptr, edge_data_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "clamp", "Thickness Clamp", nullptr, clamp_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "vertex_groups",
                             "Output Vertex Groups",
                             nullptr,
                             vertex_group_panel_draw,
                             panel_type);
}

ModifierTypeInfo modifierType_Solidify = {
    /*idname*/ "Solidify",
    /*name*/ N_("Solidify"),
    /*struct_name*/ "SolidifyModifierData",
    /*struct_size*/ sizeof(SolidifyModifierData),
    /*srna*/ &RNA_SolidifyModifier,
    /*type*/ ModifierTypeType::Constructive,

    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SOLIDIFY,

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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
