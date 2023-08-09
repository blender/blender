/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_key.h"
#include "BKE_particle.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.hh"

#include "UI_resources.hh"

static void deform_verts(ModifierData * /*md*/,
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

static void deform_matrices(ModifierData *md,
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

  deform_verts(md, ctx, mesh, vertexCos, verts_num);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            BMEditMesh * /*em*/,
                            Mesh *mesh,
                            float (*vertexCos)[3],
                            int verts_num)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->type == KEY_RELATIVE) {
    deform_verts(md, ctx, mesh, vertexCos, verts_num);
  }
}

static void deform_matrices_EM(ModifierData * /*md*/,
                               const ModifierEvalContext *ctx,
                               BMEditMesh * /*em*/,
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
    /*struct_name*/ "ShapeKeyModifierData",
    /*struct_size*/ sizeof(ShapeKeyModifierData),
    /*srna*/ &RNA_Modifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_DOT,

    /*copy_data*/ nullptr,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ deform_matrices,
    /*deform_verts_EM*/ deform_verts_EM,
    /*deform_matrices_EM*/ deform_matrices_EM,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ nullptr,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ nullptr,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
