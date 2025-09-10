/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.h"

#include "BLT_translation.hh"

#include "DNA_key_types.h"
#include "DNA_object_types.h"

#include "BKE_key.hh"

#include "RNA_prototypes.hh"

#include "MOD_modifiertypes.hh"

#include "UI_resources.hh"

static void deform_verts(ModifierData * /*md*/,
                         const ModifierEvalContext *ctx,
                         Mesh * /*mesh*/,
                         blender::MutableSpan<blender::float3> positions)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->block.first) {
    int deformedVerts_tot;
    BKE_key_evaluate_object_ex(ctx->object,
                               &deformedVerts_tot,
                               reinterpret_cast<float *>(positions.data()),
                               sizeof(blender::float3) * positions.size(),
                               nullptr);
  }
}

static void deform_matrices(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions,
                            blender::MutableSpan<blender::float3x3> matrices)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);

  if (kb && kb->totelem == positions.size() && kb != key->refkey) {
    float scale[3][3];
    int a;

    if (ctx->object->shapeflag & OB_SHAPE_LOCK) {
      scale_m3_fl(scale, 1);
    }
    else {
      scale_m3_fl(scale, kb->curval);
    }

    for (a = 0; a < positions.size(); a++) {
      copy_m3_m3(matrices[a].ptr(), scale);
    }
  }

  deform_verts(md, ctx, mesh, positions);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            const BMEditMesh * /*em*/,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions)
{
  Key *key = BKE_key_from_object(ctx->object);

  if (key && key->type == KEY_RELATIVE) {
    deform_verts(md, ctx, mesh, positions);
  }
}

static void deform_matrices_EM(ModifierData * /*md*/,
                               const ModifierEvalContext *ctx,
                               const BMEditMesh * /*em*/,
                               Mesh * /*mesh*/,
                               blender::MutableSpan<blender::float3> /*positions*/,
                               blender::MutableSpan<blender::float3x3> matrices)
{
  Key *key = BKE_key_from_object(ctx->object);
  KeyBlock *kb = BKE_keyblock_from_object(ctx->object);

  if (kb && kb->totelem == matrices.size() && kb != key->refkey) {
    float scale[3][3];
    scale_m3_fl(scale, kb->curval);

    for (int a = 0; a < matrices.size(); a++) {
      copy_m3_m3(matrices[a].ptr(), scale);
    }
  }
}

ModifierTypeInfo modifierType_ShapeKey = {
    /*idname*/ "ShapeKey",
    /*name*/ N_("ShapeKey"),
    /*struct_name*/ "ShapeKeyModifierData",
    /*struct_size*/ sizeof(ShapeKeyModifierData),
    /*srna*/ &RNA_Modifier,
    /*type*/ ModifierTypeType::OnlyDeform,
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
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
