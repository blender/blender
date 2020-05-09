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

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_shrinkwrap.h"

#include "DEG_depsgraph_query.h"

#include "MOD_util.h"

static bool dependsOnNormals(ModifierData *md);

static void initData(ModifierData *md)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
  smd->shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
  smd->shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
  smd->keepDist = 0.0f;

  smd->target = NULL;
  smd->auxTarget = NULL;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  if ((smd->shrinkType == MOD_SHRINKWRAP_PROJECT) &&
      (smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)) {
    /* XXX Really? These should always be present, always... */
    r_cddata_masks->vmask |= CD_MASK_MVERT;
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
  else if (smd->auxTarget && smd->auxTarget->type != OB_MESH) {
    return true;
  }
  return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  walk(userData, ob, &smd->target, IDWALK_CB_NOP);
  walk(userData, ob, &smd->auxTarget, IDWALK_CB_NOP);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  ShrinkwrapModifierData *swmd = (ShrinkwrapModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

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
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, numVerts);

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
  ShrinkwrapModifierData *swmd = (ShrinkwrapModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *mesh_src = NULL;

  if ((swmd->vgroup_name[0] != '\0') || (swmd->shrinkType == MOD_SHRINKWRAP_PROJECT)) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);
  }

  struct MDeformVert *dvert = NULL;
  int defgrp_index = -1;
  if (swmd->vgroup_name[0] != '\0') {
    MOD_get_vgroup(ctx->object, mesh_src, swmd->vgroup_name, &dvert, &defgrp_index);
  }

  shrinkwrapModifier_deform(
      swmd, ctx, scene, ctx->object, mesh_src, dvert, defgrp_index, vertexCos, numVerts);

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
  DEG_add_modifier_to_transform_relation(ctx->node, "Shrinkwrap Modifier");
}

static bool dependsOnNormals(ModifierData *md)
{
  ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

  if (smd->target && smd->shrinkType == MOD_SHRINKWRAP_PROJECT) {
    return (smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL);
  }

  return false;
}

ModifierTypeInfo modifierType_Shrinkwrap = {
    /* name */ "Shrinkwrap",
    /* structName */ "ShrinkwrapModifierData",
    /* structSize */ sizeof(ShrinkwrapModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
