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

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_deform.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subsurf.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_types.h" /* For subdivide operator UI. */

#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

typedef struct MultiresRuntimeData {
  /* Cached subdivision surface descriptor, with topology and settings. */
  struct Subdiv *subdiv;
} MultiresRuntimeData;

static void initData(ModifierData *md)
{
  MultiresModifierData *mmd = (MultiresModifierData *)md;

  mmd->lvl = 0;
  mmd->sculptlvl = 0;
  mmd->renderlvl = 0;
  mmd->totlvl = 0;
  mmd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
  mmd->quality = 4;
  mmd->flags |= (eMultiresModifierFlag_UseCrease | eMultiresModifierFlag_ControlEdges);
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  BKE_modifier_copydata_generic(md_src, md_dst, flag);
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)runtime_data_v;
  if (runtime_data->subdiv != NULL) {
    BKE_subdiv_free(runtime_data->subdiv);
  }
  MEM_freeN(runtime_data);
}

static void freeData(ModifierData *md)
{
  MultiresModifierData *mmd = (MultiresModifierData *)md;
  freeRuntimeData(mmd->modifier.runtime);
}

static MultiresRuntimeData *multires_ensure_runtime(MultiresModifierData *mmd)
{
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)mmd->modifier.runtime;
  if (runtime_data == NULL) {
    runtime_data = MEM_callocN(sizeof(*runtime_data), "subsurf runtime");
    mmd->modifier.runtime = runtime_data;
  }
  return runtime_data;
}

/* Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology. */
static Subdiv *subdiv_descriptor_ensure(MultiresModifierData *mmd,
                                        const SubdivSettings *subdiv_settings,
                                        const Mesh *mesh)
{
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)mmd->modifier.runtime;
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(runtime_data->subdiv, subdiv_settings, mesh);
  runtime_data->subdiv = subdiv;
  return subdiv;
}

/* Subdivide into fully qualified mesh. */

static Mesh *multires_as_mesh(MultiresModifierData *mmd,
                              const ModifierEvalContext *ctx,
                              Mesh *mesh,
                              Subdiv *subdiv)
{
  Mesh *result = mesh;
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
  const bool ignore_control_edges = (ctx->flag & MOD_APPLY_TO_BASE_MESH);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Object *object = ctx->object;
  SubdivToMeshSettings mesh_settings;
  BKE_multires_subdiv_mesh_settings_init(&mesh_settings,
                                         scene,
                                         object,
                                         mmd,
                                         use_render_params,
                                         ignore_simplify,
                                         ignore_control_edges);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
  result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  return result;
}

/* Subdivide into CCG. */

static void multires_ccg_settings_init(SubdivToCCGSettings *settings,
                                       const MultiresModifierData *mmd,
                                       const ModifierEvalContext *ctx,
                                       Mesh *mesh)
{
  const bool has_mask = CustomData_has_layer(&mesh->ldata, CD_GRID_PAINT_MASK);
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Object *object = ctx->object;
  const int level = multires_get_level(scene, object, mmd, use_render_params, ignore_simplify);
  settings->resolution = (1 << level) + 1;
  settings->need_normal = true;
  settings->need_mask = has_mask;
}

static Mesh *multires_as_ccg(MultiresModifierData *mmd,
                             const ModifierEvalContext *ctx,
                             Mesh *mesh,
                             Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToCCGSettings ccg_settings;
  multires_ccg_settings_init(&ccg_settings, mmd, ctx, mesh);
  if (ccg_settings.resolution < 3) {
    return result;
  }
  BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
  result = BKE_subdiv_to_ccg_mesh(subdiv, &ccg_settings, mesh);
  return result;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result = mesh;
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(md, "Disabled, built without OpenSubdiv");
  return result;
#endif
  MultiresModifierData *mmd = (MultiresModifierData *)md;
  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  if (subdiv_settings.level == 0) {
    return result;
  }
  BKE_subdiv_settings_validate_for_mesh(&subdiv_settings, mesh);
  MultiresRuntimeData *runtime_data = multires_ensure_runtime(mmd);
  Subdiv *subdiv = subdiv_descriptor_ensure(mmd, &subdiv_settings, mesh);
  if (subdiv == NULL) {
    /* Happens on bad topology, ut also on empty input mesh. */
    return result;
  }
  /* NOTE: Orco needs final coordinates on CPU side, which are expected to be
   * accessible via MVert. For this reason we do not evaluate multires to
   * grids when orco is requested. */
  const bool for_orco = (ctx->flag & MOD_APPLY_ORCO) != 0;
  /* Needed when rendering or baking will in sculpt mode. */
  const bool for_render = (ctx->flag & MOD_APPLY_RENDER) != 0;

  if ((ctx->object->mode & OB_MODE_SCULPT) && !for_orco && !for_render) {
    /* NOTE: CCG takes ownership over Subdiv. */
    result = multires_as_ccg(mmd, ctx, mesh, subdiv);
    result->runtime.subdiv_ccg_tot_level = mmd->totlvl;
    /* TODO(sergey): Usually it is sculpt stroke's update variants which
     * takes care of this, but is possible that we need this before the
     * stroke: i.e. when exiting blender right after stroke is done.
     * Annoying and not so much black-boxed as far as sculpting goes, and
     * surely there is a better way of solving this. */
    if (ctx->object->sculpt != NULL) {
      SculptSession *sculpt_session = ctx->object->sculpt;
      sculpt_session->subdiv_ccg = result->runtime.subdiv_ccg;
      sculpt_session->multires.active = true;
      sculpt_session->multires.modifier = mmd;
      sculpt_session->multires.level = mmd->sculptlvl;
      sculpt_session->totvert = mesh->totvert;
      sculpt_session->totpoly = mesh->totpoly;
      sculpt_session->mvert = NULL;
      sculpt_session->mpoly = NULL;
      sculpt_session->mloop = NULL;
    }
    /* NOTE: CCG becomes an owner of Subdiv descriptor, so can not share
     * this pointer. Not sure if it's needed, but might have a second look
     * on the ownership model here. */
    runtime_data->subdiv = NULL;
    // BKE_subdiv_stats_print(&subdiv->stats);
  }
  else {
    result = multires_as_mesh(mmd, ctx, mesh, subdiv);
    // BKE_subdiv_stats_print(&subdiv->stats);
    if (subdiv != runtime_data->subdiv) {
      BKE_subdiv_free(subdiv);
    }
  }
  return result;
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *UNUSED(ctx),
                           Mesh *mesh,
                           float (*vertex_cos)[3],
                           float (*deform_matrices)[3][3],
                           int num_verts)

{
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(md, "Disabled, built without OpenSubdiv");
  return;
#endif

  /* Subsurf does not require extra space mapping, keep matrices as is. */
  (void)deform_matrices;

  MultiresModifierData *mmd = (MultiresModifierData *)md;
  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  if (subdiv_settings.level == 0) {
    return;
  }
  BKE_subdiv_settings_validate_for_mesh(&subdiv_settings, mesh);
  MultiresRuntimeData *runtime_data = multires_ensure_runtime(mmd);
  Subdiv *subdiv = subdiv_descriptor_ensure(mmd, &subdiv_settings, mesh);
  if (subdiv == NULL) {
    /* Happens on bad topology, ut also on empty input mesh. */
    return;
  }
  BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
  BKE_subdiv_deform_coarse_vertices(subdiv, mesh, vertex_cos, num_verts);
  if (subdiv != runtime_data->subdiv) {
    BKE_subdiv_free(subdiv);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *col, *split, *col2;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  /**
   * Changing some of the properties can not be done once there is an
   * actual displacement stored for this multires modifier. This check
   * will disallow changes for those properties.
   * This check is a bit stupif but it should be sufficient for the usual
   * multires usage. It might become less strict and only disallow
   * modifications if there is CD_MDISPS layer, or if there is actual
   * non-zero displacement, but such checks will be too slow to be done
   * on every redraw.
   */
  bool has_displacement = RNA_int_get(&ptr, "total_levels") != 0;
  MultiresModifierData *mmd = (MultiresModifierData *)ptr.data;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(col, !has_displacement);
  uiItemR(col, &ptr, "subdivision_type", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &ptr, "sculpt_levels", 0, IFACE_("Levels Sculpt"), ICON_NONE);
  uiItemR(col, &ptr, "levels", 0, IFACE_("Viewport"), ICON_NONE);
  uiItemR(col, &ptr, "render_levels", 0, IFACE_("Render"), ICON_NONE);
  uiItemR(layout, &ptr, "show_only_control_edges", 0, NULL, ICON_NONE);

  uiItemS(layout);

  split = uiLayoutSplit(layout, 0.5f, false);
  uiLayoutSetEnabled(split, RNA_enum_get(&ob_ptr, "mode") != OB_MODE_EDIT);
  col = uiLayoutColumn(split, false);
  col2 = uiLayoutColumn(split, false);

  uiItemO(col, IFACE_("Unsubdivide"), ICON_NONE, "OBJECT_OT_multires_unsubdivide");

  row = uiLayoutRow(col2, true);
  PointerRNA op_ptr;
  uiItemFullO(row,
              "OBJECT_OT_multires_subdivide",
              IFACE_("Subdivide"),
              ICON_NONE,
              NULL,
              WM_OP_EXEC_DEFAULT,
              0,
              &op_ptr);
  RNA_enum_set(&op_ptr, "mode", MULTIRES_SUBDIVIDE_CATMULL_CLARK);
  RNA_string_set(&op_ptr, "modifier", ((ModifierData *)mmd)->name);
  uiItemFullO(row,
              "OBJECT_OT_multires_subdivide",
              IFACE_("Simple"),
              ICON_NONE, /* TODO: Needs icon, remove text */
              NULL,
              WM_OP_EXEC_DEFAULT,
              0,
              &op_ptr);
  RNA_enum_set(&op_ptr, "mode", MULTIRES_SUBDIVIDE_SIMPLE);
  RNA_string_set(&op_ptr, "modifier", ((ModifierData *)mmd)->name);
  uiItemFullO(row,
              "OBJECT_OT_multires_subdivide",
              IFACE_("Linear"),
              ICON_NONE, /* TODO: Needs icon, remove text */
              NULL,
              WM_OP_EXEC_DEFAULT,
              0,
              &op_ptr);
  RNA_enum_set(&op_ptr, "mode", MULTIRES_SUBDIVIDE_LINEAR);
  RNA_string_set(&op_ptr, "modifier", ((ModifierData *)mmd)->name);

  uiItemL(col, "", ICON_NONE);
  uiItemO(col2, IFACE_("Delete Higher"), ICON_NONE, "OBJECT_OT_multires_higher_levels_delete");

  uiItemO(col, IFACE_("Reshape"), ICON_NONE, "OBJECT_OT_multires_reshape");
  uiItemO(col2, IFACE_("Apply Base"), ICON_NONE, "OBJECT_OT_multires_base_apply");

  uiItemS(layout);

  if (mmd->totlvl == 0) {
    uiItemO(
        layout, IFACE_("Rebuild Subdivisions"), ICON_NONE, "OBJECT_OT_multires_rebuild_subdiv");
  }

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, false);
  if (RNA_boolean_get(&ptr, "is_external")) {
    uiItemO(row, IFACE_("Pack External"), ICON_NONE, "OBJECT_OT_multires_external_pack");
    row = uiLayoutRow(col, false);
    uiItemR(row, &ptr, "filepath", 0, "", ICON_NONE);
  }
  else {
    uiItemO(col, IFACE_("Save External..."), ICON_NONE, "OBJECT_OT_multires_external_save");
  }

  modifier_panel_end(layout, &ptr);
}

static void advanced_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  bool has_displacement = RNA_int_get(&ptr, "total_levels") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(col, !has_displacement);
  uiItemR(col, &ptr, "quality", 0, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "uv_smooth", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(col, !has_displacement);
  uiItemR(col, &ptr, "use_creases", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Multires, panel_draw);
  modifier_subpanel_register(
      region_type, "advanced", "Advanced", NULL, advanced_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Multires = {
    /* name */ "Multires",
    /* structName */ "MultiresModifierData",
    /* structSize */ sizeof(MultiresModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_RequiresOriginalData,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ deformMatrices,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ freeRuntimeData,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
