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
 * along with this program; if not, write to the Free Software  Foundation,
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
#include "BLI_listbase.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BKE_action.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph_query.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  amd->deformflag = ARM_DEF_VGROUP;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const ArmatureModifierData *amd = (const ArmatureModifierData *)md;
#endif
  ArmatureModifierData *tamd = (ArmatureModifierData *)target;

  modifier_copyData_generic(md, target, flag);
  tamd->prevCos = NULL;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  /* ask for vertexgroups */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  return !amd->object;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  walk(userData, ob, &amd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  if (amd->object != NULL) {
    /* If not using envelopes, create relations to individual bones for more rigging flexibility. */
    if ((amd->deformflag & ARM_DEF_ENVELOPE) == 0 && (amd->object->pose != NULL) &&
        ELEM(ctx->object->type, OB_MESH, OB_LATTICE, OB_GPENCIL)) {
      /* If neither vertex groups nor envelopes are used, the modifier has no bone dependencies. */
      if ((amd->deformflag & ARM_DEF_VGROUP) != 0) {
        /* Enumerate groups that match existing bones. */
        LISTBASE_FOREACH (bDeformGroup *, dg, &ctx->object->defbase) {
          if (BKE_pose_channel_find_name(amd->object->pose, dg->name) != NULL) {
            /* Can't check BONE_NO_DEFORM because it can be animated. */
            DEG_add_bone_relation(
                ctx->node, amd->object, dg->name, DEG_OB_COMP_BONE, "Armature Modifier");
          }
        }
      }
    }
    /* Otherwise require the whole pose to be complete. */
    else {
      DEG_add_object_relation(ctx->node, amd->object, DEG_OB_COMP_EVAL_POSE, "Armature Modifier");
    }

    DEG_add_object_relation(ctx->node, amd->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
  }
  DEG_add_modifier_to_transform_relation(ctx->node, "Armature Modifier");
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  armature_deform_verts(amd->object,
                        ctx->object,
                        mesh,
                        vertexCos,
                        NULL,
                        numVerts,
                        amd->deformflag,
                        (float(*)[3])amd->prevCos,
                        amd->defgrp_name,
                        NULL);

  /* free cache */
  if (amd->prevCos) {
    MEM_freeN(amd->prevCos);
    amd->prevCos = NULL;
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *em,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, em, mesh, NULL, numVerts, false, false);

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  armature_deform_verts(amd->object,
                        ctx->object,
                        mesh_src,
                        vertexCos,
                        NULL,
                        numVerts,
                        amd->deformflag,
                        (float(*)[3])amd->prevCos,
                        amd->defgrp_name,
                        NULL);

  /* free cache */
  if (amd->prevCos) {
    MEM_freeN(amd->prevCos);
    amd->prevCos = NULL;
  }

  if (mesh_src != mesh) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformMatricesEM(ModifierData *md,
                             const ModifierEvalContext *ctx,
                             struct BMEditMesh *em,
                             Mesh *mesh,
                             float (*vertexCos)[3],
                             float (*defMats)[3][3],
                             int numVerts)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, em, mesh, NULL, numVerts, false, false);

  armature_deform_verts(amd->object,
                        ctx->object,
                        mesh_src,
                        vertexCos,
                        defMats,
                        numVerts,
                        amd->deformflag,
                        NULL,
                        amd->defgrp_name,
                        NULL);

  if (mesh_src != mesh) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           float (*vertexCos)[3],
                           float (*defMats)[3][3],
                           int numVerts)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);

  armature_deform_verts(amd->object,
                        ctx->object,
                        mesh_src,
                        vertexCos,
                        defMats,
                        numVerts,
                        amd->deformflag,
                        NULL,
                        amd->defgrp_name,
                        NULL);

  if (mesh_src != mesh) {
    BKE_id_free(NULL, mesh_src);
  }
}

ModifierTypeInfo modifierType_Armature = {
    /* name */ "Armature",
    /* structName */ "ArmatureModifierData",
    /* structSize */ sizeof(ArmatureModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsLattice |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ deformMatrices,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ deformMatricesEM,
    /* applyModifier */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
