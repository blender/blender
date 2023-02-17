/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_shrinkwrap.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static bool dependsOnNormals(ModifierData *md);

static void initData(ModifierData *md)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(ShrinkwrapModifierData), modifier);
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
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

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  walk(userData, ob, (ID **)&smd->target, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&smd->auxTarget, IDWALK_CB_NOP);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  ShrinkwrapModifierData *swmd = (ShrinkwrapModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

  if (ELEM(ctx->object->type, OB_MESH, OB_LATTICE) ||
      (swmd->shrinkType == MOD_SHRINKWRAP_PROJECT)) {
    /* mesh_src is needed for vgroups, but also used as ShrinkwrapCalcData.vert when projecting.
     * Avoid time-consuming mesh conversion for curves when not projecting. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, verts_num, false);
  }

  const MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  MOD_get_vgroup(ctx->object, mesh_src, swmd->vgroup_name, &dvert, &defgrp_index);

  shrinkwrapModifier_deform(
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, verts_num);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  ShrinkwrapModifierData *swmd = (ShrinkwrapModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

  if ((swmd->vgroup_name[0] != '\0') || (swmd->shrinkType == MOD_SHRINKWRAP_PROJECT)) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, verts_num, false);
  }

  /* TODO(@ideasman42): use edit-mode data only (remove this line). */
  if (mesh_src != NULL) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  const MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  if (swmd->vgroup_name[0] != '\0') {
    MOD_get_vgroup(ctx->object, mesh_src, swmd->vgroup_name, &dvert, &defgrp_index);
  }

  shrinkwrapModifier_deform(
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, verts_num);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
  CustomData_MeshMasks mask = {0};

  if (BKE_shrinkwrap_needs_normals(smd->shrinkType, smd->shrinkMode)) {
    mask.vmask |= CD_MASK_NORMAL;
    mask.lmask |= CD_MASK_NORMAL | CD_MASK_CUSTOMLOOPNORMAL;
  }

  if (smd->target != NULL) {
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, smd->target, &mask);
    if (smd->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &smd->target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  if (smd->auxTarget != NULL) {
    DEG_add_object_relation(
        ctx->node, smd->auxTarget, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(
        ctx->node, smd->auxTarget, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, smd->auxTarget, &mask);
    if (smd->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &smd->auxTarget->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Shrinkwrap Modifier");
}

static bool dependsOnNormals(ModifierData *md)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  if (smd->target && smd->shrinkType == MOD_SHRINKWRAP_PROJECT) {
    return (smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL);
  }

  return false;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  int wrap_method = RNA_enum_get(ptr, "wrap_method");

  uiItemR(layout, ptr, "wrap_method", 0, NULL, ICON_NONE);

  if (ELEM(wrap_method,
           MOD_SHRINKWRAP_PROJECT,
           MOD_SHRINKWRAP_NEAREST_SURFACE,
           MOD_SHRINKWRAP_TARGET_PROJECT)) {
    uiItemR(layout, ptr, "wrap_mode", 0, NULL, ICON_NONE);
  }

  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "project_limit", 0, IFACE_("Limit"), ICON_NONE);
    uiItemR(layout, ptr, "subsurf_levels", 0, NULL, ICON_NONE);

    col = uiLayoutColumn(layout, false);
    row = uiLayoutRowWithHeading(col, true, IFACE_("Axis"));
    uiItemR(row, ptr, "use_project_x", toggles_flag, NULL, ICON_NONE);
    uiItemR(row, ptr, "use_project_y", toggles_flag, NULL, ICON_NONE);
    uiItemR(row, ptr, "use_project_z", toggles_flag, NULL, ICON_NONE);

    uiItemR(col, ptr, "use_negative_direction", 0, NULL, ICON_NONE);
    uiItemR(col, ptr, "use_positive_direction", 0, NULL, ICON_NONE);

    uiItemR(layout, ptr, "cull_face", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
    col = uiLayoutColumn(layout, false);
    uiLayoutSetActive(col,
                      RNA_boolean_get(ptr, "use_negative_direction") &&
                          RNA_enum_get(ptr, "cull_face") != 0);
    uiItemR(col, ptr, "use_invert_cull", 0, NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "target", 0, NULL, ICON_NONE);
  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "auxiliary_target", 0, NULL, ICON_NONE);
  }
  uiItemR(layout, ptr, "offset", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Shrinkwrap, panel_draw);
}

ModifierTypeInfo modifierType_Shrinkwrap = {
    /*name*/ N_("Shrinkwrap"),
    /*structName*/ "ShrinkwrapModifierData",
    /*structSize*/ sizeof(ShrinkwrapModifierData),
    /*srna*/ &RNA_ShrinkwrapModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SHRINKWRAP,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ NULL,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ NULL,
    /*modifyMesh*/ NULL,
    /*modifyGeometrySet*/ NULL,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ NULL,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ NULL,
    /*dependsOnNormals*/ dependsOnNormals,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*freeRuntimeData*/ NULL,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ NULL,
    /*blendRead*/ NULL,
};
