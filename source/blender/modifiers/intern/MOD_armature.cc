/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(amd, modifier));

  MEMCPY_STRUCT_AFTER(amd, DNA_struct_default_get(ArmatureModifierData), modifier);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const ArmatureModifierData *amd = (const ArmatureModifierData *)md;
#endif
  ArmatureModifierData *tamd = (ArmatureModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);
  tamd->vert_coords_prev = nullptr;
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* Ask for vertex-groups. */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !amd->object || amd->object->type != OB_ARMATURE;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  walk(user_data, ob, (ID **)&amd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  if (amd->object != nullptr) {
    /* If not using envelopes,
     * create relations to individual bones for more rigging flexibility. */
    if ((amd->deformflag & ARM_DEF_ENVELOPE) == 0 && (amd->object->pose != nullptr) &&
        ELEM(ctx->object->type, OB_MESH, OB_LATTICE))
    {
      /* If neither vertex groups nor envelopes are used, the modifier has no bone dependencies. */
      if ((amd->deformflag & ARM_DEF_VGROUP) != 0) {
        /* Enumerate groups that match existing bones. */
        const ListBase *defbase = BKE_object_defgroup_list(ctx->object);
        LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
          if (BKE_pose_channel_find_name(amd->object->pose, dg->name) != nullptr) {
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
  DEG_add_depends_on_transform_relation(ctx->node, "Armature Modifier");
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  std::optional<blender::Span<blender::float3>> vert_coords_prev;
  if (amd->vert_coords_prev) {
    vert_coords_prev = {reinterpret_cast<blender::float3 *>(amd->vert_coords_prev),
                        positions.size()};
  }

  /* if next modifier needs original vertices */
  MOD_previous_vcos_store(md, reinterpret_cast<float (*)[3]>(positions.data()));

  BKE_armature_deform_coords_with_mesh(*amd->object,
                                       *ctx->object,
                                       positions,
                                       vert_coords_prev,
                                       std::nullopt,
                                       amd->deformflag,
                                       amd->defgrp_name,
                                       mesh);

  /* free cache */
  MEM_SAFE_FREE(amd->vert_coords_prev);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            const BMEditMesh *em,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions)
{
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    deform_verts(md, ctx, mesh, positions);
    return;
  }

  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  std::optional<blender::Span<blender::float3>> vert_coords_prev;
  if (amd->vert_coords_prev) {
    vert_coords_prev = {reinterpret_cast<blender::float3 *>(amd->vert_coords_prev),
                        positions.size()};
  }

  /* if next modifier needs original vertices */
  MOD_previous_vcos_store(md, reinterpret_cast<float (*)[3]>(positions.data()));

  BKE_armature_deform_coords_with_editmesh(*amd->object,
                                           *ctx->object,
                                           positions,
                                           vert_coords_prev,
                                           std::nullopt,
                                           amd->deformflag,
                                           amd->defgrp_name,
                                           *em);

  /* free cache */
  MEM_SAFE_FREE(amd->vert_coords_prev);
}

static void deform_matrices_EM(ModifierData *md,
                               const ModifierEvalContext *ctx,
                               const BMEditMesh *em,
                               Mesh * /*mesh*/,
                               blender::MutableSpan<blender::float3> positions,
                               blender::MutableSpan<blender::float3x3> matrices)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  BKE_armature_deform_coords_with_editmesh(*amd->object,
                                           *ctx->object,
                                           positions,
                                           std::nullopt,
                                           matrices,
                                           amd->deformflag,
                                           amd->defgrp_name,
                                           *em);
}

static void deform_matrices(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions,
                            blender::MutableSpan<blender::float3x3> matrices)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  BKE_armature_deform_coords_with_mesh(*amd->object,
                                       *ctx->object,
                                       positions,
                                       std::nullopt,
                                       matrices,
                                       amd->deformflag,
                                       amd->defgrp_name,
                                       mesh);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  col = &layout->column(true);
  col->prop(ptr, "use_deform_preserve_volume", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "use_multi_modifier", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(true, IFACE_("Bind To"));
  col->prop(ptr, "use_vertex_groups", UI_ITEM_NONE, IFACE_("Vertex Groups"), ICON_NONE);
  col->prop(ptr, "use_bone_envelopes", UI_ITEM_NONE, IFACE_("Bone Envelopes"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Armature, panel_draw);
}

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  amd->vert_coords_prev = nullptr;
}

ModifierTypeInfo modifierType_Armature = {
    /*idname*/ "Armature",
    /*name*/ N_("Armature"),
    /*struct_name*/ "ArmatureModifierData",
    /*struct_size*/ sizeof(ArmatureModifierData),
    /*srna*/ &RNA_ArmatureModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_ARMATURE,

    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ deform_matrices,
    /*deform_verts_EM*/ deform_verts_EM,
    /*deform_matrices_EM*/ deform_matrices_EM,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
