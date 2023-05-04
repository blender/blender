/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "BLO_read_write.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void initData(ModifierData *md)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(amd, modifier));

  MEMCPY_STRUCT_AFTER(amd, DNA_struct_default_get(ArmatureModifierData), modifier);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const ArmatureModifierData *amd = (const ArmatureModifierData *)md;
#endif
  ArmatureModifierData *tamd = (ArmatureModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);
  tamd->vert_coords_prev = nullptr;
}

static void requiredDataMask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* ask for vertexgroups */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static bool isDisabled(const Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !amd->object || amd->object->type != OB_ARMATURE;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  walk(userData, ob, (ID **)&amd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  if (amd->object != nullptr) {
    /* If not using envelopes,
     * create relations to individual bones for more rigging flexibility. */
    if ((amd->deformflag & ARM_DEF_ENVELOPE) == 0 && (amd->object->pose != nullptr) &&
        ELEM(ctx->object->type, OB_MESH, OB_LATTICE, OB_GPENCIL_LEGACY))
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

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_armature_deform_coords_with_mesh(amd->object,
                                       ctx->object,
                                       vertexCos,
                                       nullptr,
                                       verts_num,
                                       amd->deformflag,
                                       amd->vert_coords_prev,
                                       amd->defgrp_name,
                                       mesh);

  /* free cache */
  MEM_SAFE_FREE(amd->vert_coords_prev);
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          BMEditMesh *em,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  if (mesh != nullptr) {
    deformVerts(md, ctx, mesh, vertexCos, verts_num);
    return;
  }

  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_armature_deform_coords_with_editmesh(amd->object,
                                           ctx->object,
                                           vertexCos,
                                           nullptr,
                                           verts_num,
                                           amd->deformflag,
                                           amd->vert_coords_prev,
                                           amd->defgrp_name,
                                           em);

  /* free cache */
  MEM_SAFE_FREE(amd->vert_coords_prev);
}

static void deformMatricesEM(ModifierData *md,
                             const ModifierEvalContext *ctx,
                             BMEditMesh *em,
                             Mesh * /*mesh*/,
                             float (*vertexCos)[3],
                             float (*defMats)[3][3],
                             int verts_num)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  BKE_armature_deform_coords_with_editmesh(amd->object,
                                           ctx->object,
                                           vertexCos,
                                           defMats,
                                           verts_num,
                                           amd->deformflag,
                                           nullptr,
                                           amd->defgrp_name,
                                           em);
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           float (*vertexCos)[3],
                           float (*defMats)[3][3],
                           int verts_num)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;
  Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, mesh, nullptr, verts_num, false);

  BKE_armature_deform_coords_with_mesh(amd->object,
                                       ctx->object,
                                       vertexCos,
                                       defMats,
                                       verts_num,
                                       amd->deformflag,
                                       nullptr,
                                       amd->defgrp_name,
                                       mesh_src);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    BKE_id_free(nullptr, mesh_src);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", 0, nullptr, ICON_NONE);
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_deform_preserve_volume", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_multi_modifier", 0, nullptr, ICON_NONE);

  col = uiLayoutColumnWithHeading(layout, true, IFACE_("Bind To"));
  uiItemR(col, ptr, "use_vertex_groups", 0, IFACE_("Vertex Groups"), ICON_NONE);
  uiItemR(col, ptr, "use_bone_envelopes", 0, IFACE_("Bone Envelopes"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Armature, panel_draw);
}

static void blendRead(BlendDataReader * /*reader*/, ModifierData *md)
{
  ArmatureModifierData *amd = (ArmatureModifierData *)md;

  amd->vert_coords_prev = nullptr;
}

ModifierTypeInfo modifierType_Armature = {
    /*name*/ N_("Armature"),
    /*structName*/ "ArmatureModifierData",
    /*structSize*/ sizeof(ArmatureModifierData),
    /*srna*/ &RNA_ArmatureModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_ARMATURE,

    /*copyData*/ copyData,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ deformMatrices,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ deformMatricesEM,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ nullptr,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ blendRead,
};
