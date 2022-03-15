/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_key.h"
#include "BKE_particle.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.h"

#include "UI_resources.h"

static void deformVerts(ModifierData *UNUSED(md),
                        const ModifierEvalContext *ctx,
                        Mesh *UNUSED(mesh),
                        float (*vertexCos)[3],
                        int numVerts)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->block.first) {
    int deformedVerts_tot;
    BKE_key_evaluate_object_ex(
        ctx->object, &deformedVerts_tot, (float *)vertexCos, sizeof(*vertexCos) * numVerts);
  }
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           float (*vertexCos)[3],
                           float (*defMats)[3][3],
                           int numVerts)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);
  float scale[3][3];

  (void)vertexCos; /* unused */

  if (kb && kb->totelem == numVerts && kb != key->refkey) {
    int a;

    if (ctx->object->shapeflag & OB_SHAPE_LOCK) {
      scale_m3_fl(scale, 1);
    }
    else {
      scale_m3_fl(scale, kb->curval);
    }

    for (a = 0; a < numVerts; a++) {
      copy_m3_m3(defMats[a], scale);
    }
  }

  deformVerts(md, ctx, mesh, vertexCos, numVerts);
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *UNUSED(editData),
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->type == KEY_RELATIVE) {
    deformVerts(md, ctx, mesh, vertexCos, numVerts);
  }
}

static void deformMatricesEM(ModifierData *UNUSED(md),
                             const ModifierEvalContext *ctx,
                             struct BMEditMesh *UNUSED(editData),
                             Mesh *UNUSED(mesh),
                             float (*vertexCos)[3],
                             float (*defMats)[3][3],
                             int numVerts)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);
  float scale[3][3];

  (void)vertexCos; /* unused */

  if (kb && kb->totelem == numVerts && kb != key->refkey) {
    int a;
    scale_m3_fl(scale, kb->curval);

    for (a = 0; a < numVerts; a++) {
      copy_m3_m3(defMats[a], scale);
    }
  }
}

ModifierTypeInfo modifierType_ShapeKey = {
    /* name */ "ShapeKey",
    /* structName */ "ShapeKeyModifierData",
    /* structSize */ sizeof(ShapeKeyModifierData),
    /* srna */ &RNA_Modifier,
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /* icon */ ICON_DOT,

    /* copyData */ NULL,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ deformMatrices,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ deformMatricesEM,
    /* modifyMesh */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ NULL,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
