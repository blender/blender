/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_key.h"
#include "BKE_particle.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.hh"

#include "UI_resources.h"

static void deformVerts(ModifierData * /*md*/,
                        const ModifierEvalContext *ctx,
                        Mesh * /*mesh*/,
                        float (*vertexCos)[3],
                        int verts_num)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->block.first) {
    int deformedVerts_tot;
    BKE_key_evaluate_object_ex(ctx->object,
                               &deformedVerts_tot,
                               (float *)vertexCos,
                               sizeof(*vertexCos) * verts_num,
                               nullptr);
  }
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           float (*vertexCos)[3],
                           float (*defMats)[3][3],
                           int verts_num)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);

  (void)vertexCos; /* unused */

  if (kb && kb->totelem == verts_num && kb != key->refkey) {
    float scale[3][3];
    int a;

    if (ctx->object->shapeflag & OB_SHAPE_LOCK) {
      scale_m3_fl(scale, 1);
    }
    else {
      scale_m3_fl(scale, kb->curval);
    }

    for (a = 0; a < verts_num; a++) {
      copy_m3_m3(defMats[a], scale);
    }
  }

  deformVerts(md, ctx, mesh, vertexCos, verts_num);
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          BMEditMesh * /*editData*/,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->type == KEY_RELATIVE) {
    deformVerts(md, ctx, mesh, vertexCos, verts_num);
  }
}

static void deformMatricesEM(ModifierData * /*md*/,
                             const ModifierEvalContext *ctx,
                             BMEditMesh * /*editData*/,
                             Mesh * /*mesh*/,
                             float (*vertexCos)[3],
                             float (*defMats)[3][3],
                             int verts_num)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);

  (void)vertexCos; /* unused */

  if (kb && kb->totelem == verts_num && kb != key->refkey) {
    float scale[3][3];
    scale_m3_fl(scale, kb->curval);

    for (int a = 0; a < verts_num; a++) {
      copy_m3_m3(defMats[a], scale);
    }
  }
}

ModifierTypeInfo modifierType_ShapeKey = {
    /*idname*/ "ShapeKey",
    /*name*/ N_("ShapeKey"),
    /*structName*/ "ShapeKeyModifierData",
    /*structSize*/ sizeof(ShapeKeyModifierData),
    /*srna*/ &RNA_Modifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_DOT,

    /*copyData*/ nullptr,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ deformMatrices,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ deformMatricesEM,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ nullptr,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ nullptr,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
