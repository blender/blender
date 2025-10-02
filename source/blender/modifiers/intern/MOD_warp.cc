/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "BKE_action.hh" /* BKE_pose_channel_find_name */
#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "RE_texture.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WarpModifierData), modifier);

  wmd->curfalloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const WarpModifierData *wmd = (const WarpModifierData *)md;
  WarpModifierData *twmd = (WarpModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  twmd->curfalloff = BKE_curvemapping_copy(wmd->curfalloff);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (wmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  /* ask for UV coordinates if we need them */
  if (wmd->texmapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }
}

static void matrix_from_obj_pchan(float mat[4][4],
                                  const float obinv[4][4],
                                  Object *ob,
                                  const char *bonename)
{
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bonename);
  if (pchan) {
    float mat_bone_world[4][4];
    mul_m4_m4m4(mat_bone_world, ob->object_to_world().ptr(), pchan->pose_mat);
    mul_m4_m4m4(mat, obinv, mat_bone_world);
  }
  else {
    mul_m4_m4m4(mat, obinv, ob->object_to_world().ptr());
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  if (wmd->texture) {
    return BKE_texture_dependsOnTime(wmd->texture);
  }

  return false;
}

static void free_data(ModifierData *md)
{
  WarpModifierData *wmd = (WarpModifierData *)md;
  BKE_curvemapping_free(wmd->curfalloff);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  return !(wmd->object_from && wmd->object_to);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  walk(user_data, ob, (ID **)&wmd->texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&wmd->object_from, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&wmd->object_to, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&wmd->map_object, IDWALK_CB_NOP);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "texture");
  walk(user_data, ob, md, &ptr, prop);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WarpModifierData *wmd = (WarpModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->object_from != nullptr && wmd->object_to != nullptr) {
    MOD_depsgraph_update_object_bone_relation(
        ctx->node, wmd->object_from, wmd->bone_from, "Warp Modifier");
    MOD_depsgraph_update_object_bone_relation(
        ctx->node, wmd->object_to, wmd->bone_to, "Warp Modifier");
    need_transform_relation = true;
  }

  if (wmd->texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &wmd->texture->id, "Warp Modifier");

    if ((wmd->texmapping == MOD_DISP_MAP_OBJECT) && wmd->map_object != nullptr) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->map_object, wmd->map_bone, "Warp Modifier");
      need_transform_relation = true;
    }
    else if (wmd->texmapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Warp Modifier");
  }
}

static void warpModifier_do(WarpModifierData *wmd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            float (*vertexCos)[3],
                            int verts_num)
{
  Object *ob = ctx->object;
  float obinv[4][4];
  float mat_from[4][4];
  float mat_from_inv[4][4];
  float mat_to[4][4];
  float mat_unit[4][4];
  float mat_final[4][4];

  float tmat[4][4];

  const float falloff_radius_sq = square_f(wmd->falloff_radius);
  float strength = wmd->strength;
  float fac = 1.0f, weight;
  int i;
  int defgrp_index;
  const MDeformVert *dvert, *dv = nullptr;
  const bool invert_vgroup = (wmd->flag & MOD_WARP_INVERT_VGROUP) != 0;
  float (*tex_co)[3] = nullptr;

  if (!(wmd->object_from && wmd->object_to)) {
    return;
  }

  MOD_get_vgroup(ob, mesh, wmd->defgrp_name, &dvert, &defgrp_index);
  if (dvert == nullptr) {
    defgrp_index = -1;
  }

  if (wmd->curfalloff == nullptr) { /* should never happen, but bad lib linking could cause it */
    wmd->curfalloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  }

  if (wmd->curfalloff) {
    BKE_curvemapping_init(wmd->curfalloff);
  }

  invert_m4_m4(obinv, ob->object_to_world().ptr());

  /* Checks that the objects/bones are available. */
  matrix_from_obj_pchan(mat_from, obinv, wmd->object_from, wmd->bone_from);
  matrix_from_obj_pchan(mat_to, obinv, wmd->object_to, wmd->bone_to);

  invert_m4_m4(tmat, mat_from);  // swap?
  mul_m4_m4m4(mat_final, tmat, mat_to);

  invert_m4_m4(mat_from_inv, mat_from);

  unit_m4(mat_unit);

  if (strength < 0.0f) {
    float loc[3];
    strength = -strength;

    /* inverted location is not useful, just use the negative */
    copy_v3_v3(loc, mat_final[3]);
    invert_m4(mat_final);
    negate_v3_v3(mat_final[3], loc);
  }
  weight = strength;

  Tex *tex_target = wmd->texture;
  if (mesh != nullptr && tex_target != nullptr) {
    tex_co = MEM_malloc_arrayN<float[3]>(size_t(verts_num), __func__);
    MOD_get_texture_coords((MappingInfoModifierData *)wmd, ctx, ob, mesh, vertexCos, tex_co);

    MOD_init_texture((MappingInfoModifierData *)wmd, ctx);
  }

  for (i = 0; i < verts_num; i++) {
    float *co = vertexCos[i];

    if (wmd->falloff_type == eWarp_Falloff_None ||
        ((fac = len_squared_v3v3(co, mat_from[3])) < falloff_radius_sq &&
         (fac = (wmd->falloff_radius - sqrtf(fac)) / wmd->falloff_radius)))
    {
      /* skip if no vert group found */
      if (defgrp_index != -1) {
        dv = &dvert[i];
        weight = (invert_vgroup ? (1.0f - BKE_defvert_find_weight(dv, defgrp_index)) :
                                  BKE_defvert_find_weight(dv, defgrp_index)) *
                 strength;
        if (weight <= 0.0f) {
          continue;
        }
      }

      /* closely match PROP_SMOOTH and similar */
      switch (wmd->falloff_type) {
        case eWarp_Falloff_None:
          fac = 1.0f;
          break;
        case eWarp_Falloff_Curve:
          fac = BKE_curvemapping_evaluateF(wmd->curfalloff, 0, fac);
          break;
        case eWarp_Falloff_Sharp:
          fac = fac * fac;
          break;
        case eWarp_Falloff_Smooth:
          fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
          break;
        case eWarp_Falloff_Root:
          fac = sqrtf(fac);
          break;
        case eWarp_Falloff_Linear:
          /* pass */
          break;
        case eWarp_Falloff_Const:
          fac = 1.0f;
          break;
        case eWarp_Falloff_Sphere:
          fac = sqrtf(2 * fac - fac * fac);
          break;
        case eWarp_Falloff_InvSquare:
          fac = fac * (2.0f - fac);
          break;
      }

      fac *= weight;

      if (tex_co) {
        TexResult texres;
        BKE_texture_get_value(tex_target, tex_co[i], &texres, false);
        fac *= texres.tin;
      }

      if (fac != 0.0f) {
        /* into the 'from' objects space */
        mul_m4_v3(mat_from_inv, co);

        if (fac == 1.0f) {
          mul_m4_v3(mat_final, co);
        }
        else {
          if (wmd->flag & MOD_WARP_VOLUME_PRESERVE) {
            /* interpolate the matrix for nicer locations */
            blend_m4_m4m4(tmat, mat_unit, mat_final, fac);
            mul_m4_v3(tmat, co);
          }
          else {
            float tvec[3];
            mul_v3_m4v3(tvec, mat_final, co);
            interp_v3_v3v3(co, co, tvec, fac);
          }
        }

        /* out of the 'from' objects space */
        mul_m4_v3(mat_from, co);
      }
    }
  }

  if (tex_co) {
    MEM_freeN(tex_co);
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  WarpModifierData *wmd = (WarpModifierData *)md;
  warpModifier_do(
      wmd, ctx, mesh, reinterpret_cast<float (*)[3]>(positions.data()), positions.size());
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  col = &layout->column(true);
  col->prop(ptr, "object_from", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  PointerRNA from_obj_ptr = RNA_pointer_get(ptr, "object_from");
  if (!RNA_pointer_is_null(&from_obj_ptr) && RNA_enum_get(&from_obj_ptr, "type") == OB_ARMATURE) {

    PointerRNA from_obj_data_ptr = RNA_pointer_get(&from_obj_ptr, "data");
    col->prop_search(
        ptr, "bone_from", &from_obj_data_ptr, "bones", IFACE_("Bone"), ICON_BONE_DATA);
  }

  col = &layout->column(true);
  col->prop(ptr, "object_to", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  PointerRNA to_obj_ptr = RNA_pointer_get(ptr, "object_to");
  if (!RNA_pointer_is_null(&to_obj_ptr) && RNA_enum_get(&to_obj_ptr, "type") == OB_ARMATURE) {
    PointerRNA to_obj_data_ptr = RNA_pointer_get(&to_obj_ptr, "data");
    col->prop_search(ptr, "bone_to", &to_obj_data_ptr, "bones", IFACE_("Bone"), ICON_BONE_DATA);
  }

  layout->prop(ptr, "use_volume_preserve", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void falloff_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_falloff = (RNA_enum_get(ptr, "falloff_type") != eWarp_Falloff_None);

  layout->use_property_split_set(true);

  layout->prop(ptr, "falloff_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (use_falloff) {
    layout->prop(ptr, "falloff_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (use_falloff && RNA_enum_get(ptr, "falloff_type") == eWarp_Falloff_Curve) {
    uiTemplateCurveMapping(layout, ptr, "falloff_curve", 0, false, false, false, false, false);
  }
}

static void texture_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int texture_coords = RNA_enum_get(ptr, "texture_coords");

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "texture_coords", UI_ITEM_NONE, IFACE_("Coordinates"), ICON_NONE);
  if (texture_coords == MOD_DISP_MAP_OBJECT) {
    col->prop(ptr, "texture_coords_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
    PointerRNA texture_coords_obj_ptr = RNA_pointer_get(ptr, "texture_coords_object");
    if (!RNA_pointer_is_null(&texture_coords_obj_ptr) &&
        (RNA_enum_get(&texture_coords_obj_ptr, "type") == OB_ARMATURE))
    {
      PointerRNA texture_coords_obj_data_ptr = RNA_pointer_get(&texture_coords_obj_ptr, "data");
      col->prop_search(ptr,
                       "texture_coords_bone",
                       &texture_coords_obj_data_ptr,
                       "bones",
                       IFACE_("Bone"),
                       ICON_NONE);
    }
  }
  else if (texture_coords == MOD_DISP_MAP_UV && RNA_enum_get(&ob_ptr, "type") == OB_MESH) {
    PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
    col->prop_search(ptr, "uv_layer", &obj_data_ptr, "uv_layers", std::nullopt, ICON_GROUP_UVS);
  }
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Warp, panel_draw);
  modifier_subpanel_register(
      region_type, "falloff", "Falloff", nullptr, falloff_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "texture", "Texture", nullptr, texture_panel_draw, panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const WarpModifierData *wmd = (const WarpModifierData *)md;

  BLO_write_struct(writer, WarpModifierData, wmd);

  if (wmd->curfalloff) {
    BKE_curvemapping_blend_write(writer, wmd->curfalloff);
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  WarpModifierData *wmd = (WarpModifierData *)md;

  BLO_read_struct(reader, CurveMapping, &wmd->curfalloff);
  if (wmd->curfalloff) {
    BKE_curvemapping_blend_read(reader, wmd->curfalloff);
  }
}

ModifierTypeInfo modifierType_Warp = {
    /*idname*/ "Warp",
    /*name*/ N_("Warp"),
    /*struct_name*/ "WarpModifierData",
    /*struct_size*/ sizeof(WarpModifierData),
    /*srna*/ &RNA_WarpModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_WARP,
    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ foreach_tex_link,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
