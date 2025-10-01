/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.hh"
#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  HookModifierData *hmd = (HookModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(hmd, modifier));

  MEMCPY_STRUCT_AFTER(hmd, DNA_struct_default_get(HookModifierData), modifier);

  hmd->curfalloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const HookModifierData *hmd = (const HookModifierData *)md;
  HookModifierData *thmd = (HookModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  thmd->curfalloff = BKE_curvemapping_copy(hmd->curfalloff);

  thmd->indexar = static_cast<int *>(MEM_dupallocN(hmd->indexar));
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  HookModifierData *hmd = (HookModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (hmd->name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
  if (hmd->indexar != nullptr) {
    /* TODO: check which origindex are actually needed? */
    r_cddata_masks->vmask |= CD_MASK_ORIGINDEX;
    r_cddata_masks->emask |= CD_MASK_ORIGINDEX;
    r_cddata_masks->pmask |= CD_MASK_ORIGINDEX;
  }
}

static void free_data(ModifierData *md)
{
  HookModifierData *hmd = (HookModifierData *)md;

  BKE_curvemapping_free(hmd->curfalloff);

  MEM_SAFE_FREE(hmd->indexar);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  HookModifierData *hmd = (HookModifierData *)md;

  return !hmd->object;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  HookModifierData *hmd = (HookModifierData *)md;

  walk(user_data, ob, (ID **)&hmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  HookModifierData *hmd = (HookModifierData *)md;
  if (hmd->object != nullptr) {
    if (hmd->subtarget[0]) {
      DEG_add_bone_relation(
          ctx->node, hmd->object, hmd->subtarget, DEG_OB_COMP_BONE, "Hook Modifier");
    }
    DEG_add_object_relation(ctx->node, hmd->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
  }
  /* We need our own transformation as well. */
  DEG_add_depends_on_transform_relation(ctx->node, "Hook Modifier");
}

struct HookData_cb {
  blender::MutableSpan<blender::float3> positions;

  /**
   * When anything other than -1, use deform groups.
   * This is not the same as checking `dvert` for nullptr when we have edit-meshes.
   */
  int defgrp_index;

  CurveMapping *curfalloff;

  char falloff_type;
  float falloff;
  float falloff_sq;
  float fac_orig;

  uint use_falloff : 1;
  uint use_uniform : 1;

  float cent[3];

  float mat_uniform[3][3];
  float mat[4][4];

  bool invert_vgroup;
};

static BLI_bitmap *hook_index_array_to_bitmap(HookModifierData *hmd, const int verts_num)
{
  BLI_bitmap *indexar_used = BLI_BITMAP_NEW(verts_num, __func__);
  int i;
  int *index_pt;
  for (i = 0, index_pt = hmd->indexar; i < hmd->indexar_num; i++, index_pt++) {
    const int j = *index_pt;
    if (j < verts_num) {
      BLI_BITMAP_ENABLE(indexar_used, i);
    }
  }
  return indexar_used;
}

static float hook_falloff(const HookData_cb *hd, const float len_sq)
{
  BLI_assert(hd->falloff_sq);
  if (len_sq > hd->falloff_sq) {
    return 0.0f;
  }
  if (len_sq > 0.0f) {
    float fac;

    if (hd->falloff_type == eHook_Falloff_Const) {
      fac = 1.0f;
      goto finally;
    }
    else if (hd->falloff_type == eHook_Falloff_InvSquare) {
      /* Avoid `sqrt` below. */
      fac = 1.0f - (len_sq / hd->falloff_sq);
      goto finally;
    }

    fac = 1.0f - (sqrtf(len_sq) / hd->falloff);

    /* closely match PROP_SMOOTH and similar */
    switch (hd->falloff_type) {
#if 0
      case eHook_Falloff_None:
        fac = 1.0f;
        break;
#endif
      case eHook_Falloff_Curve:
        fac = BKE_curvemapping_evaluateF(hd->curfalloff, 0, fac);
        break;
      case eHook_Falloff_Sharp:
        fac = fac * fac;
        break;
      case eHook_Falloff_Smooth:
        fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
        break;
      case eHook_Falloff_Root:
        fac = sqrtf(fac);
        break;
      case eHook_Falloff_Linear:
        /* pass */
        break;
#if 0
      case eHook_Falloff_Const:
        fac = 1.0f;
        break;
#endif
      case eHook_Falloff_Sphere:
        fac = sqrtf(2 * fac - fac * fac);
        break;
#if 0
      case eHook_Falloff_InvSquare:
        fac = fac * (2.0f - fac);
        break;
#endif
    }

  finally:
    return fac * hd->fac_orig;
  }
  else {
    return hd->fac_orig;
  }
}

static void hook_co_apply(HookData_cb *hd, int j, const MDeformVert *dv)
{
  float *co = hd->positions[j];
  float fac;

  if (hd->use_falloff) {
    float len_sq;

    if (hd->use_uniform) {
      float co_uniform[3];
      mul_v3_m3v3(co_uniform, hd->mat_uniform, co);
      len_sq = len_squared_v3v3(hd->cent, co_uniform);
    }
    else {
      len_sq = len_squared_v3v3(hd->cent, co);
    }

    fac = hook_falloff(hd, len_sq);
  }
  else {
    fac = hd->fac_orig;
  }

  if (fac) {
    if (dv != nullptr) {
      fac *= hd->invert_vgroup ? 1.0f - BKE_defvert_find_weight(dv, hd->defgrp_index) :
                                 BKE_defvert_find_weight(dv, hd->defgrp_index);
    }

    if (fac) {
      float co_tmp[3];
      mul_v3_m4v3(co_tmp, hd->mat, co);
      interp_v3_v3v3(co, co, co_tmp, fac);
    }
  }
}

static void deformVerts_do(HookModifierData *hmd,
                           const ModifierEvalContext * /*ctx*/,
                           Object *ob,
                           Mesh *mesh,
                           const BMEditMesh *em,
                           blender::MutableSpan<blender::float3> positions)
{
  Object *ob_target = hmd->object;
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob_target->pose, hmd->subtarget);
  float dmat[4][4];
  int i, *index_pt;
  const MDeformVert *dvert;
  HookData_cb hd;
  const bool invert_vgroup = (hmd->flag & MOD_HOOK_INVERT_VGROUP) != 0;

  if (hmd->curfalloff == nullptr) {
    /* should never happen, but bad lib linking could cause it */
    hmd->curfalloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  }

  if (hmd->curfalloff) {
    BKE_curvemapping_init(hmd->curfalloff);
  }

  /* Generic data needed for applying per-vertex calculations (initialize all members) */
  hd.positions = positions;

  MOD_get_vgroup(ob, mesh, hmd->name, &dvert, &hd.defgrp_index);
  int cd_dvert_offset = -1;

  if (hd.defgrp_index != -1) {
    /* Edit-mesh. */
    if (em != nullptr) {
      cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
      if (cd_dvert_offset == -1) {
        hd.defgrp_index = -1;
      }
    }
    else {
      /* Regular mesh. */
      if (dvert == nullptr) {
        hd.defgrp_index = -1;
      }
    }
  }

  hd.curfalloff = hmd->curfalloff;

  hd.falloff_type = hmd->falloff_type;
  hd.falloff = (hmd->falloff_type == eHook_Falloff_None) ? 0.0f : hmd->falloff;
  hd.falloff_sq = square_f(hd.falloff);
  hd.fac_orig = hmd->force;

  hd.use_falloff = (hd.falloff_sq != 0.0f);
  hd.use_uniform = (hmd->flag & MOD_HOOK_UNIFORM_SPACE) != 0;

  hd.invert_vgroup = invert_vgroup;

  if (hd.use_uniform) {
    copy_m3_m4(hd.mat_uniform, hmd->parentinv);
    mul_v3_m3v3(hd.cent, hd.mat_uniform, hmd->cent);
  }
  else {
    unit_m3(hd.mat_uniform); /* unused */
    copy_v3_v3(hd.cent, hmd->cent);
  }

  /* get world-space matrix of target, corrected for the space the verts are in */
  if (hmd->subtarget[0] && pchan) {
    /* bone target if there's a matching pose-channel */
    mul_m4_m4m4(dmat, ob_target->object_to_world().ptr(), pchan->pose_mat);
  }
  else {
    /* just object target */
    copy_m4_m4(dmat, ob_target->object_to_world().ptr());
  }
  invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
  mul_m4_series(hd.mat, ob->world_to_object().ptr(), dmat, hmd->parentinv);
  /* --- done with 'hd' init --- */

  /* Regarding index range checking below.
   *
   * This should always be true and I don't generally like
   * "paranoid" style code like this, but old files can have
   * indices that are out of range because old blender did
   * not correct them on exit edit-mode. - zr
   */

  if (hmd->force == 0.0f) {
    /* do nothing, avoid annoying checks in the loop */
  }
  else if (hmd->indexar) { /* vertex indices? */
    const int *origindex_ar;
    /* if mesh is present and has original index data, use it */
    if (mesh && (origindex_ar = static_cast<const int *>(
                     CustomData_get_layer(&mesh->vert_data, CD_ORIGINDEX))))
    {
      int verts_orig_num = positions.size();
      if (ob->type == OB_MESH) {
        const Mesh *me_orig = static_cast<const Mesh *>(ob->data);
        verts_orig_num = me_orig->verts_num;
      }
      BLI_bitmap *indexar_used = hook_index_array_to_bitmap(hmd, verts_orig_num);
      for (i = 0; i < positions.size(); i++) {
        int i_orig = origindex_ar[i];
        BLI_assert(i_orig < verts_orig_num);
        if (BLI_BITMAP_TEST(indexar_used, i_orig)) {
          hook_co_apply(&hd, i, dvert ? &dvert[i] : nullptr);
        }
      }
      MEM_freeN(indexar_used);
    }
    else { /* missing mesh or ORIGINDEX */
      if ((em != nullptr) && (hd.defgrp_index != -1)) {
        BLI_assert(em->bm->totvert == positions.size());
        BLI_bitmap *indexar_used = hook_index_array_to_bitmap(hmd, positions.size());
        BMIter iter;
        BMVert *v;
        BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
          if (BLI_BITMAP_TEST(indexar_used, i)) {
            const MDeformVert *dv = static_cast<const MDeformVert *>(
                BM_ELEM_CD_GET_VOID_P(v, cd_dvert_offset));
            hook_co_apply(&hd, i, dv);
          }
        }
        MEM_freeN(indexar_used);
      }
      else {
        for (i = 0, index_pt = hmd->indexar; i < hmd->indexar_num; i++, index_pt++) {
          const int j = *index_pt;
          if (j < positions.size()) {
            hook_co_apply(&hd, j, dvert ? &dvert[j] : nullptr);
          }
        }
      }
    }
  }
  else if (hd.defgrp_index != -1) { /* vertex group hook */
    if (em != nullptr) {
      BLI_assert(em->bm->totvert == positions.size());
      BMIter iter;
      BMVert *v;
      BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        const MDeformVert *dv = static_cast<const MDeformVert *>(
            BM_ELEM_CD_GET_VOID_P(v, cd_dvert_offset));
        hook_co_apply(&hd, i, dv);
      }
    }
    else {
      BLI_assert(dvert != nullptr);
      for (i = 0; i < positions.size(); i++) {
        hook_co_apply(&hd, i, &dvert[i]);
      }
    }
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  HookModifierData *hmd = (HookModifierData *)md;
  deformVerts_do(hmd, ctx, ctx->object, mesh, nullptr, positions);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            const BMEditMesh *em,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions)
{
  HookModifierData *hmd = (HookModifierData *)md;

  deformVerts_do(hmd,
                 ctx,
                 ctx->object,
                 mesh,
                 mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH ? em : nullptr,
                 positions);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA hook_object_ptr = RNA_pointer_get(ptr, "object");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!RNA_pointer_is_null(&hook_object_ptr) &&
      RNA_enum_get(&hook_object_ptr, "type") == OB_ARMATURE)
  {
    PointerRNA hook_object_data_ptr = RNA_pointer_get(&hook_object_ptr, "data");
    col->prop_search(ptr, "subtarget", &hook_object_data_ptr, "bones", IFACE_("Bone"), ICON_NONE);
  }
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  layout->prop(ptr, "strength", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  if (RNA_enum_get(&ob_ptr, "mode") == OB_MODE_EDIT) {
    row = &layout->row(true);
    row->op("OBJECT_OT_hook_reset", IFACE_("Reset"), ICON_NONE);
    row->op("OBJECT_OT_hook_recenter", IFACE_("Recenter"), ICON_NONE);
    row = &layout->row(true);
    row->op("OBJECT_OT_hook_select", IFACE_("Select"), ICON_NONE);
    row->op("OBJECT_OT_hook_assign", IFACE_("Assign"), ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
}

static void falloff_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_falloff = RNA_enum_get(ptr, "falloff_type") != eWarp_Falloff_None;

  layout->use_property_split_set(true);

  layout->prop(ptr, "falloff_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);

  row = &layout->row(false);
  row->active_set(use_falloff);
  row->prop(ptr, "falloff_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "use_falloff_uniform", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (RNA_enum_get(ptr, "falloff_type") == eWarp_Falloff_Curve) {
    uiTemplateCurveMapping(layout, ptr, "falloff_curve", 0, false, false, false, false, false);
  }
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Hook, panel_draw);
  modifier_subpanel_register(
      region_type, "falloff", "Falloff", nullptr, falloff_panel_draw, panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const HookModifierData *hmd = (const HookModifierData *)md;

  BLO_write_struct(writer, HookModifierData, hmd);

  if (hmd->curfalloff) {
    BKE_curvemapping_blend_write(writer, hmd->curfalloff);
  }

  BLO_write_int32_array(writer, hmd->indexar_num, hmd->indexar);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  HookModifierData *hmd = (HookModifierData *)md;

  BLO_read_struct(reader, CurveMapping, &hmd->curfalloff);
  if (hmd->curfalloff) {
    BKE_curvemapping_blend_read(reader, hmd->curfalloff);
  }

  BLO_read_int32_array(reader, hmd->indexar_num, &hmd->indexar);
}

ModifierTypeInfo modifierType_Hook = {
    /*idname*/ "Hook",
    /*name*/ N_("Hook"),
    /*struct_name*/ "HookModifierData",
    /*struct_size*/ sizeof(HookModifierData),
    /*srna*/ &RNA_HookModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_HOOK,
    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ deform_verts_EM,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
