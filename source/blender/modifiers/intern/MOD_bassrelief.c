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

#include "BLI_memarena.h"
#include "BLI_utildefines.h"
#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_bassrelief.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "BLI_math.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "MEM_guardedalloc.h"

#define DEBUG_VIS_COLORS

static bool dependsOnNormals(ModifierData *md);

static void initData(ModifierData *md)
{
  BassReliefModifierData *smd = (BassReliefModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(BassReliefModifierData), modifier);
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  BassReliefModifierData *smd = (BassReliefModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  BassReliefModifierData *smd = (BassReliefModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */

  bool ok = smd->target && smd->target->type == OB_MESH;
  ok = ok || smd->collection;

  return !ok;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  BassReliefModifierData *smd = (BassReliefModifierData *)md;

  walk(userData, ob, (ID **)&smd->target, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&smd->collection, IDWALK_CB_NOP);
}

#ifndef DEBUG_VIS_COLORS
static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  BassReliefModifierData *swmd = (BassReliefModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

  if (swmd->rayShrinkRatio == 0.0f) {
    swmd->rayShrinkRatio = 1.0f;
  }

  if (ELEM(ctx->object->type, OB_MESH, OB_LATTICE) ||
      (swmd->shrinkType == MOD_SHRINKWRAP_PROJECT)) {
    /* mesh_src is needed for vgroups, but also used as ShrinkwrapCalcData.vert when projecting.
     * Avoid time-consuming mesh conversion for curves when not projecting. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);
  }

  struct MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  MOD_get_vgroup(ctx->object, mesh_src, swmd->vgroup_name, &dvert, &defgrp_index);

  shrinkwrapModifier_deform(
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, numVerts, NULL);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  BassReliefModifierData *swmd = (BassReliefModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

  if (swmd->rayShrinkRatio == 0.0f) {
    swmd->rayShrinkRatio = 1.0f;
  }

  if ((swmd->vgroup_name[0] != '\0') || (swmd->shrinkType == MOD_SHRINKWRAP_PROJECT)) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);
  }

  /* TODO(Campbell): use edit-mode data only (remove this line). */
  if (mesh_src != NULL) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  struct MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  if (swmd->vgroup_name[0] != '\0') {
    MOD_get_vgroup(ctx->object, mesh_src, swmd->vgroup_name, &dvert, &defgrp_index);
  }

  shrinkwrapModifier_deform(
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, numVerts, NULL);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}
#else
static Mesh *modifyMeshDebug(struct ModifierData *md,
                             const struct ModifierEvalContext *ctx,
                             struct Mesh *mesh)
{
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  CustomData_duplicate_referenced_layers(&mesh->vdata, mesh->totvert);
  // XXX BKE_mesh_update_customdata_pointers(mesh, false);

  MPropCol *colors[MAX_BASSRELIEF_DEBUG_COLORS];
  char name[MAX_CUSTOMDATA_LAYER_NAME];

  for (int i = 0; i < MAX_BASSRELIEF_DEBUG_COLORS; i++) {
    sprintf(name, "debug%d", i + 1);
    colors[i] = (MPropCol *)CustomData_get_layer_named_for_write(
        &mesh->vdata, CD_PROP_COLOR, name, mesh->totvert);
  }

  float(*cos)[3] = MEM_malloc_arrayN(mesh->totvert, sizeof(float) * 3, __func__);
  float(*vert_cos)[3] = BKE_mesh_vert_positions_for_write(mesh);

  for (int i = 0; i < mesh->totvert; i++) {
    copy_v3_v3(cos[i], vert_cos[i]);
  }

  BassReliefModifierData *swmd = (BassReliefModifierData *)md;

  if (swmd->rayShrinkRatio == 0.0f) {
    swmd->rayShrinkRatio = 1.0f;
  }

  const struct MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  if (swmd->vgroup_name[0] != '\0') {
    MOD_get_vgroup(ctx->object, mesh, swmd->vgroup_name, &dvert, &defgrp_index);
  }

  bassReliefModifier_deform(
      swmd, ctx, scene, ctx->object, mesh, dvert, defgrp_index, cos, mesh->totvert, colors);

  vert_cos = BKE_mesh_vert_positions_for_write(mesh);
  for (int i = 0; i < mesh->totvert; i++) {
    copy_v3_v3(vert_cos[i], cos[i]);
  }

  // BKE_mesh_calc_normals(mesh);
  BKE_mesh_normals_tag_dirty(mesh);

  MEM_freeN(cos);
  return mesh;
}
#endif

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  BassReliefModifierData *smd = (BassReliefModifierData *)md;
  CustomData_MeshMasks mask = {0};

  mask.vmask |= CD_MASK_NORMAL;
  mask.lmask |= CD_MASK_NORMAL | CD_MASK_CUSTOMLOOPNORMAL;

  if (smd->target != NULL) {
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_TRANSFORM, "Bass Relief Modifier");
    DEG_add_object_relation(ctx->node, smd->target, DEG_OB_COMP_GEOMETRY, "Bass Relief Modifier");
    DEG_add_customdata_mask(ctx->node, smd->target, &mask);
    DEG_add_special_eval_flag(ctx->node, &smd->target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
  }

  if (smd->collection != NULL) {
    DEG_add_collection_geometry_relation(ctx->node, smd->collection, "Bass Relief Modifier");
  }

  DEG_add_depends_on_transform_relation(ctx->node, "Bass Relief Modifier");
}

static bool dependsOnNormals(ModifierData *md)
{
  return true;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "project_limit", 0, IFACE_("Limit"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRowWithHeading(col, true, IFACE_("Axis"));
  uiItemR(row, ptr, "use_project_x", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, ptr, "use_project_y", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, ptr, "use_project_z", toggles_flag, NULL, ICON_NONE);

  uiItemR(col, ptr, "use_negative_direction", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_positive_direction", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "cull_face", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(
      col, RNA_boolean_get(ptr, "use_negative_direction") && RNA_enum_get(ptr, "cull_face") != 0);
  uiItemR(col, ptr, "use_invert_cull", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "target", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "collection", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "offset", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "ray_shrink_ratio", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_normal_optimizer", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "detail_scale", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "optimizer_steps", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "boundary_smooth_falloff", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "boundary_smooth_steps", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);
  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_BassRelief, panel_draw);
}

ModifierTypeInfo modifierType_BassRelief = {
    /* name */ "Bass Relief",
    /* structName */ "BassReliefModifierData",
    /* structSize */ sizeof(BassReliefModifierData),
    /* srna */ &RNA_BassReliefModifier,
#ifndef DEBUG_VIS_COLORS
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */
    eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
#else
    /* type */ eModifierTypeType_Constructive,
    eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping,
#endif
    /* icon */ ICON_MOD_SHRINKWRAP,

    /* copyData */ BKE_modifier_copydata_generic,
#ifndef DEBUG_VIS_COLORS
    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
#else
    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMeshDebug,
#endif
    /* modifyGeometrySet */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,

};
