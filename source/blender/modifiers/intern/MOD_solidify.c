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
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_particle.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "MOD_solidify_util.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

static bool dependsOnNormals(ModifierData *md)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  /* even when we calculate our own normals,
   * the vertex normals are used as a fallback
   * if manifold is enabled vertex normals are not used */
  return smd->mode == MOD_SOLIDIFY_MODE_EXTRUDE;
}

static void initData(ModifierData *md)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;
  smd->offset = 0.01f;
  smd->offset_fac = -1.0f;
  smd->flag = MOD_SOLIDIFY_RIM;
  smd->mode = MOD_SOLIDIFY_MODE_EXTRUDE;
  smd->nonmanifold_offset_mode = MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS;
  smd->nonmanifold_boundary_mode = MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE;
  smd->merge_tolerance = 0.0001f;
  smd->bevel_convex = 0.0f;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->defgrp_name[0] != '\0' || smd->shell_defgrp_name[0] != '\0' ||
      smd->rim_defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  switch (smd->mode) {
    case MOD_SOLIDIFY_MODE_EXTRUDE:
      return MOD_solidify_extrude_modifyMesh(md, ctx, mesh);
    case MOD_SOLIDIFY_MODE_NONMANIFOLD:
      return MOD_solidify_nonmanifold_modifyMesh(md, ctx, mesh);
    default:
      BLI_assert(0);
  }
  return mesh;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int solidify_mode = RNA_enum_get(&ptr, "solidify_mode");
  bool has_vertex_group = RNA_string_length(&ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "solidify_mode", 0, NULL, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    uiItemR(layout, &ptr, "nonmanifold_thickness_mode", 0, IFACE_("Thickness Mode"), ICON_NONE);
    uiItemR(layout, &ptr, "nonmanifold_boundary_mode", 0, IFACE_("Boundary"), ICON_NONE);
  }

  uiItemR(layout, &ptr, "thickness", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "offset", 0, NULL, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    uiItemR(layout, &ptr, "nonmanifold_merge_threshold", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, &ptr, "use_even_offset", 0, NULL, ICON_NONE);
  }

  col = uiLayoutColumnWithHeading(layout, false, "Rim");
  uiItemR(col, &ptr, "use_rim", 0, IFACE_("Fill"), ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_rim"));
  uiItemR(sub, &ptr, "use_rim_only", 0, NULL, ICON_NONE);

  uiItemS(layout);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);
  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, has_vertex_group);
  uiItemR(row, &ptr, "thickness_vertex_group", 0, IFACE_("Factor"), ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, has_vertex_group);
    uiItemR(row, &ptr, "use_flat_faces", 0, NULL, ICON_NONE);
  }

  modifier_panel_end(layout, &ptr);
}

static void normals_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int solidify_mode = RNA_enum_get(&ptr, "solidify_mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "use_flip_normals", 0, NULL, ICON_NONE);
  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    uiItemR(layout, &ptr, "use_quality_normals", 0, IFACE_("High Quality"), ICON_NONE);
  }
}

static void materials_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "material_offset", 0, NULL, ICON_NONE);
  col = uiLayoutColumn(layout, true);
  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_rim"));
  uiItemR(col, &ptr, "material_offset_rim", 0, IFACE_("Rim"), ICON_NONE);
}

static void edge_data_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int solidify_mode = RNA_enum_get(&ptr, "solidify_mode");

  uiLayoutSetPropSep(layout, true);

  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    uiItemR(layout, &ptr, "edge_crease_inner", 0, IFACE_("Inner"), ICON_NONE);
    uiItemR(layout, &ptr, "edge_crease_outer", 0, IFACE_("Outer"), ICON_NONE);
    uiItemR(layout, &ptr, "edge_crease_rim", 0, IFACE_("Rim"), ICON_NONE);
  }
  uiItemR(layout, &ptr, "bevel_convex", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void clamp_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "thickness_clamp", 0, NULL, ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, RNA_float_get(&ptr, "thickness_clamp") > 0.0f);
  uiItemR(row, &ptr, "use_thickness_angle_clamp", 0, NULL, ICON_NONE);
}

static void vertex_group_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemPointerR(col, &ptr, "shell_vertex_group", &ob_ptr, "vertex_groups", "Shell", ICON_NONE);
  uiItemPointerR(col, &ptr, "rim_vertex_group", &ob_ptr, "vertex_groups", "Rim", ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Solidify, panel_draw);
  modifier_subpanel_register(
      region_type, "normals", "Normals", NULL, normals_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "materials", "Materials", NULL, materials_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "edge_data", "Edge Data", NULL, edge_data_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "clamp", "Thickness Clamp", NULL, clamp_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "vertex_groups",
                             "Output Vertex Groups",
                             NULL,
                             vertex_group_panel_draw,
                             panel_type);
}

ModifierTypeInfo modifierType_Solidify = {
    /* name */ "Solidify",
    /* structName */ "SolidifyModifierData",
    /* structSize */ sizeof(SolidifyModifierData),
    /* type */ eModifierTypeType_Constructive,

    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode |
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
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
