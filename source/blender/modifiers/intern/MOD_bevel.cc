/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_curveprofile_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_curveprofile.h"
#include "BKE_deform.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "BLO_read_write.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

static void init_data(ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(bmd, modifier));

  MEMCPY_STRUCT_AFTER(bmd, DNA_struct_default_get(BevelModifierData), modifier);

  bmd->custom_profile = BKE_curveprofile_add(PROF_PRESET_LINE);
}

static void copy_data(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  const BevelModifierData *bmd_src = (const BevelModifierData *)md_src;
  BevelModifierData *bmd_dst = (BevelModifierData *)md_dst;

  BKE_modifier_copydata_generic(md_src, md_dst, flag);
  bmd_dst->custom_profile = BKE_curveprofile_copy(bmd_src->custom_profile);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (bmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/*
 * This calls the new bevel code (added since 2.64)
 */
static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  BMesh *bm;
  BMIter iter;
  BMEdge *e;
  BMVert *v;
  float weight, weight2;
  int vgroup = -1;
  const MDeformVert *dvert = nullptr;
  BevelModifierData *bmd = (BevelModifierData *)md;
  const float threshold = cosf(bmd->bevel_angle + 0.000000175f);
  const bool do_clamp = !(bmd->flags & MOD_BEVEL_OVERLAP_OK);
  const int offset_type = bmd->val_flags;
  const int profile_type = bmd->profile_type;
  const float value = bmd->value;
  const int mat = CLAMPIS(bmd->mat, -1, ctx->object->totcol - 1);
  const bool loop_slide = (bmd->flags & MOD_BEVEL_EVEN_WIDTHS) == 0;
  const bool mark_seam = (bmd->edge_flags & MOD_BEVEL_MARK_SEAM);
  const bool mark_sharp = (bmd->edge_flags & MOD_BEVEL_MARK_SHARP);
  bool harden_normals = (bmd->flags & MOD_BEVEL_HARDEN_NORMALS);
  const int face_strength_mode = bmd->face_str_mode;
  const int miter_outer = bmd->miter_outer;
  const int miter_inner = bmd->miter_inner;
  const float spread = bmd->spread;
  const bool invert_vgroup = (bmd->flags & MOD_BEVEL_INVERT_VGROUP) != 0;

  BMeshCreateParams create_params{};
  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = true;
  convert_params.calc_vert_normal = true;
  convert_params.add_key_index = false;
  convert_params.use_shapekey = false;
  convert_params.active_shapekey = 0;
  convert_params.cd_mask_extra.vmask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.emask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.pmask = CD_MASK_ORIGINDEX;

  bm = BKE_mesh_to_bmesh_ex(mesh, &create_params, &convert_params);

  if ((bmd->lim_flags & MOD_BEVEL_VGROUP) && bmd->defgrp_name[0]) {
    MOD_get_vgroup(ctx->object, mesh, bmd->defgrp_name, &dvert, &vgroup);
  }

  const int bweight_offset_vert = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
  const int bweight_offset_edge = CustomData_get_offset_named(
      &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");

  if (bmd->affect_type == MOD_BEVEL_AFFECT_VERTICES) {
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
        weight = bweight_offset_vert == -1 ? 0.0f : BM_ELEM_CD_GET_FLOAT(v, bweight_offset_vert);
        if (weight == 0.0f) {
          continue;
        }
      }
      else if (vgroup != -1) {
        weight = invert_vgroup ?
                     1.0f -
                         BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup) :
                     BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup);
        /* Check is against 0.5 rather than != 0.0 because cascaded bevel modifiers will
         * interpolate weights for newly created vertices, and may cause unexpected "selection" */
        if (weight < 0.5f) {
          continue;
        }
      }
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }
  }
  else if (bmd->lim_flags & MOD_BEVEL_ANGLE) {
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      /* check for 1 edge having 2 face users */
      BMLoop *l_a, *l_b;
      if (BM_edge_loop_pair(e, &l_a, &l_b)) {
        if (dot_v3v3(l_a->f->no, l_b->f->no) < threshold) {
          BM_elem_flag_enable(e, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
        }
      }
    }
  }
  else {
    /* crummy, is there a way just to operator on all? - campbell */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_edge_is_manifold(e)) {
        if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
          weight = bweight_offset_edge == -1 ? 0.0f : BM_ELEM_CD_GET_FLOAT(e, bweight_offset_edge);
          if (weight == 0.0f) {
            continue;
          }
        }
        else if (vgroup != -1) {
          weight = invert_vgroup ?
                       1.0f - BKE_defvert_array_find_weight_safe(
                                  dvert, BM_elem_index_get(e->v1), vgroup) :
                       BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v1), vgroup);
          weight2 = invert_vgroup ? 1.0f - BKE_defvert_array_find_weight_safe(
                                               dvert, BM_elem_index_get(e->v2), vgroup) :
                                    BKE_defvert_array_find_weight_safe(
                                        dvert, BM_elem_index_get(e->v2), vgroup);
          if (weight < 0.5f || weight2 < 0.5f) {
            continue;
          }
        }
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
      }
    }
  }

  Object *ob = ctx->object;

  if (harden_normals && (ob->type == OB_MESH) && !(((Mesh *)ob->data)->flag & ME_AUTOSMOOTH)) {
    BKE_modifier_set_error(ob, md, "Enable 'Auto Smooth' in Object Data Properties");
    harden_normals = false;
  }

  BM_mesh_bevel(bm,
                value,
                offset_type,
                profile_type,
                bmd->res,
                bmd->profile,
                bmd->affect_type,
                bmd->lim_flags & MOD_BEVEL_WEIGHT,
                do_clamp,
                dvert,
                vgroup,
                mat,
                loop_slide,
                mark_seam,
                mark_sharp,
                harden_normals,
                face_strength_mode,
                miter_outer,
                miter_inner,
                spread,
                mesh->smoothresh,
                bmd->custom_profile,
                bmd->vmesh_method);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);

  /* Make sure we never alloc'd these. */
  BLI_assert(bm->vtoolflagpool == nullptr && bm->etoolflagpool == nullptr &&
             bm->ftoolflagpool == nullptr);

  BM_mesh_free(bm);

  return result;
}

static bool depends_on_normals(ModifierData * /*md*/)
{
  return true;
}

static void free_data(ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;
  BKE_curveprofile_free(bmd->custom_profile);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  BevelModifierData *bmd = (BevelModifierData *)md;
  return (bmd->value == 0.0f);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  bool edge_bevel = RNA_enum_get(ptr, "affect") != MOD_BEVEL_AFFECT_VERTICES;

  uiItemR(layout, ptr, "affect", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "offset_type", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (RNA_enum_get(ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    uiItemR(col, ptr, "width_pct", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "width", UI_ITEM_NONE, IFACE_("Amount"), ICON_NONE);
  }

  uiItemR(layout, ptr, "segments", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "limit_method", UI_ITEM_NONE, nullptr, ICON_NONE);
  int limit_method = RNA_enum_get(ptr, "limit_method");
  if (limit_method == MOD_BEVEL_ANGLE) {
    sub = uiLayoutColumn(col, false);
    uiLayoutSetActive(sub, edge_bevel);
    uiItemR(col, ptr, "angle_limit", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (limit_method == MOD_BEVEL_VGROUP) {
    modifier_vgroup_ui(col, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);
  }

  modifier_panel_end(layout, ptr);
}

static void profile_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  int profile_type = RNA_enum_get(ptr, "profile_type");
  int miter_inner = RNA_enum_get(ptr, "miter_inner");
  int miter_outer = RNA_enum_get(ptr, "miter_outer");
  bool edge_bevel = RNA_enum_get(ptr, "affect") != MOD_BEVEL_AFFECT_VERTICES;

  uiItemR(layout, ptr, "profile_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (ELEM(profile_type, MOD_BEVEL_PROFILE_SUPERELLIPSE, MOD_BEVEL_PROFILE_CUSTOM)) {
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(
        row,
        profile_type == MOD_BEVEL_PROFILE_SUPERELLIPSE ||
            (profile_type == MOD_BEVEL_PROFILE_CUSTOM && edge_bevel &&
             !((miter_inner == MOD_BEVEL_MITER_SHARP) && (miter_outer == MOD_BEVEL_MITER_SHARP))));
    uiItemR(row,
            ptr,
            "profile",
            UI_ITEM_R_SLIDER,
            (profile_type == MOD_BEVEL_PROFILE_SUPERELLIPSE) ? IFACE_("Shape") :
                                                               IFACE_("Miter Shape"),
            ICON_NONE);

    if (profile_type == MOD_BEVEL_PROFILE_CUSTOM) {
      uiLayout *sub = uiLayoutColumn(layout, false);
      uiLayoutSetPropDecorate(sub, false);
      uiTemplateCurveProfile(sub, ptr, "custom_profile");
    }
  }
}

static void geometry_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool edge_bevel = RNA_enum_get(ptr, "affect") != MOD_BEVEL_AFFECT_VERTICES;

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, edge_bevel);
  uiItemR(row, ptr, "miter_outer", UI_ITEM_NONE, IFACE_("Miter Outer"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, edge_bevel);
  uiItemR(row, ptr, "miter_inner", UI_ITEM_NONE, IFACE_("Inner"), ICON_NONE);
  if (RNA_enum_get(ptr, "miter_inner") == BEVEL_MITER_ARC) {
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, edge_bevel);
    uiItemR(row, ptr, "spread", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemS(layout);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, edge_bevel);
  uiItemR(row, ptr, "vmesh_method", UI_ITEM_NONE, IFACE_("Intersections"), ICON_NONE);
  uiItemR(layout, ptr, "use_clamp_overlap", UI_ITEM_NONE, nullptr, ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, edge_bevel);
  uiItemR(row, ptr, "loop_slide", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void shading_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool edge_bevel = RNA_enum_get(ptr, "affect") != MOD_BEVEL_AFFECT_VERTICES;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "harden_normals", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumnWithHeading(layout, true, IFACE_("Mark"));
  uiLayoutSetActive(col, edge_bevel);
  uiItemR(col, ptr, "mark_seam", UI_ITEM_NONE, IFACE_("Seam"), ICON_NONE);
  uiItemR(col, ptr, "mark_sharp", UI_ITEM_NONE, IFACE_("Sharp"), ICON_NONE);

  uiItemR(layout, ptr, "material", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "face_strength_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Bevel, panel_draw);
  modifier_subpanel_register(
      region_type, "profile", "Profile", nullptr, profile_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "geometry", "Geometry", nullptr, geometry_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "shading", "Shading", nullptr, shading_panel_draw, panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const BevelModifierData *bmd = (const BevelModifierData *)md;

  BLO_write_struct(writer, BevelModifierData, bmd);

  if (bmd->custom_profile) {
    BKE_curveprofile_blend_write(writer, bmd->custom_profile);
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  BLO_read_data_address(reader, &bmd->custom_profile);
  if (bmd->custom_profile) {
    BKE_curveprofile_blend_read(reader, bmd->custom_profile);
  }
}

ModifierTypeInfo modifierType_Bevel = {
    /*idname*/ "Bevel",
    /*name*/ N_("Bevel"),
    /*struct_name*/ "BevelModifierData",
    /*struct_size*/ sizeof(BevelModifierData),
    /*srna*/ &RNA_BevelModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_BEVEL,
    /*copy_data*/ copy_data,
    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,
    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ depends_on_normals,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
};
