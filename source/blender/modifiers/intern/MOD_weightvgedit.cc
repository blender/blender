/* SPDX-FileCopyrightText: 2011 by Bastien Montagne. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"

#include "BLT_translation.hh"

#include "DNA_color_types.h" /* CurveMapping. */
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "BKE_colortools.hh" /* CurveMapping. */
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h" /* Texture masking. */

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"
#include "MOD_weightvg_util.hh"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void init_data(ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WeightVGEditModifierData), modifier);

  wmd->cmap_curve = BKE_curvemapping_add(1, 0.0, 0.0, 1.0, 1.0);
  BKE_curvemapping_init(wmd->cmap_curve);
}

static void free_data(ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  BKE_curvemapping_free(wmd->cmap_curve);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const WeightVGEditModifierData *wmd = (const WeightVGEditModifierData *)md;
  WeightVGEditModifierData *twmd = (WeightVGEditModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  twmd->cmap_curve = BKE_curvemapping_copy(wmd->cmap_curve);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  /* We need vertex groups! */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;

  /* Ask for UV coordinates if we need them. */
  if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  if (wmd->mask_texture) {
    return BKE_texture_dependsOnTime(wmd->mask_texture);
  }
  return false;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  walk(user_data, ob, (ID **)&wmd->mask_texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&wmd->mask_tex_map_obj, IDWALK_CB_NOP);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "mask_texture");
  walk(user_data, ob, md, &ptr, prop);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->mask_texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &wmd->mask_texture->id, "WeightVGEdit Modifier");

    if (wmd->mask_tex_map_obj != nullptr && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->mask_tex_map_obj, wmd->mask_tex_map_bone, "WeightVGEdit Modifier");
      need_transform_relation = true;
    }
    else if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "WeightVGEdit Modifier");
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
  /* If no vertex group, bypass. */
  return (wmd->defgrp_name[0] == '\0');
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BLI_assert(mesh != nullptr);

  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  MDeformWeight **dw = nullptr;
  float *org_w; /* Array original weights. */
  float *new_w; /* Array new weights. */
  int i;
  const bool invert_vgroup_mask = (wmd->edit_flags & MOD_WVG_EDIT_INVERT_VGROUP_MASK) != 0;

  /* Flags. */
  const bool do_add = (wmd->edit_flags & MOD_WVG_EDIT_ADD2VG) != 0;
  const bool do_rem = (wmd->edit_flags & MOD_WVG_EDIT_REMFVG) != 0;
  /* Only do weight-preview in Object, Sculpt and Pose modes! */
#if 0
  const bool do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview);
#endif

  /* Get number of verts. */
  const int verts_num = mesh->verts_num;

  /* Check if we can just return the original mesh.
   * Must have verts and therefore verts assigned to vgroups to do anything useful!
   */
  if ((verts_num == 0) || BLI_listbase_is_empty(&mesh->vertex_group_names)) {
    return mesh;
  }

  /* Get vgroup idx from its name. */
  const int defgrp_index = BKE_id_defgroup_name_index(&mesh->id, wmd->defgrp_name);
  if (defgrp_index == -1) {
    return mesh;
  }

  const bool has_mdef = !mesh->deform_verts().is_empty();
  /* If no vertices were ever added to an object's vgroup, dvert might be nullptr. */
  if (!has_mdef) {
    /* If this modifier is not allowed to add vertices, just return. */
    if (!do_add) {
      return mesh;
    }
  }

  MDeformVert *dvert = mesh->deform_verts_for_write().data();

  /* Ultimate security check. */
  if (!dvert) {
    return mesh;
  }

  /* Get org weights, assuming 0.0 for vertices not in given vgroup. */
  org_w = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);
  new_w = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);
  dw = MEM_malloc_arrayN<MDeformWeight *>(size_t(verts_num), __func__);
  for (i = 0; i < verts_num; i++) {
    dw[i] = BKE_defvert_find_index(&dvert[i], defgrp_index);
    if (dw[i]) {
      org_w[i] = new_w[i] = dw[i]->weight;
    }
    else {
      org_w[i] = new_w[i] = wmd->default_weight;
    }
  }

  /* Do mapping. */
  const bool do_invert_mapping = (wmd->edit_flags & MOD_WVG_INVERT_FALLOFF) != 0;
  const bool do_normalize = (wmd->edit_flags & MOD_WVG_EDIT_WEIGHTS_NORMALIZE) != 0;
  if (do_invert_mapping || wmd->falloff_type != MOD_WVG_MAPPING_NONE) {
    RNG *rng = nullptr;

    if (wmd->falloff_type == MOD_WVG_MAPPING_RANDOM) {
      rng = BLI_rng_new_srandom(BLI_ghashutil_strhash(ctx->object->id.name + 2));
    }

    weightvg_do_map(verts_num, new_w, wmd->falloff_type, do_invert_mapping, wmd->cmap_curve, rng);

    if (rng) {
      BLI_rng_free(rng);
    }
  }

  /* Do masking. */
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  weightvg_do_mask(ctx,
                   verts_num,
                   nullptr,
                   org_w,
                   new_w,
                   ctx->object,
                   mesh,
                   wmd->mask_constant,
                   wmd->mask_defgrp_name,
                   scene,
                   wmd->mask_texture,
                   wmd->mask_tex_use_channel,
                   wmd->mask_tex_mapping,
                   wmd->mask_tex_map_obj,
                   wmd->mask_tex_map_bone,
                   wmd->mask_tex_uvlayer_name,
                   invert_vgroup_mask);

  /* Update/add/remove from vgroup. */
  weightvg_update_vg(dvert,
                     defgrp_index,
                     dw,
                     verts_num,
                     nullptr,
                     org_w,
                     do_add,
                     wmd->add_threshold,
                     do_rem,
                     wmd->rem_threshold,
                     do_normalize);

  /* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
  if (do_prev) {
    DM_update_weight_mcol(ob, dm, 0, org_w, 0, nullptr);
  }
#endif

  /* Freeing stuff. */
  MEM_freeN(org_w);
  MEM_freeN(new_w);
  MEM_freeN(dw);

  mesh->runtime->is_original_bmesh = false;

  /* Return the vgroup-modified mesh. */
  return mesh;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *col, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  col = &layout->column(true);
  col->prop_search(ptr, "vertex_group", &ob_ptr, "vertex_groups", std::nullopt, ICON_GROUP_VERTEX);

  layout->prop(ptr, "default_weight", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  col = &layout->column(false, IFACE_("Group Add"));
  row = &col->row(true);
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_add", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_add"));
  sub->use_property_split_set(false);
  sub->prop(ptr, "add_threshold", UI_ITEM_R_SLIDER, IFACE_("Threshold"), ICON_NONE);
  row->decorator(ptr, "add_threshold", 0);

  col = &layout->column(false, IFACE_("Group Remove"));
  row = &col->row(true);
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_remove", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_remove"));
  sub->use_property_split_set(false);
  sub->prop(ptr, "remove_threshold", UI_ITEM_R_SLIDER, IFACE_("Threshold"), ICON_NONE);
  row->decorator(ptr, "remove_threshold", 0);

  layout->prop(ptr, "normalize", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void falloff_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  row = &layout->row(true);
  row->prop(ptr, "falloff_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
  sub = &row->row(true);
  sub->use_property_split_set(false);
  row->prop(ptr, "invert_falloff", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  if (RNA_enum_get(ptr, "falloff_type") == MOD_WVG_MAPPING_CURVE) {
    uiTemplateCurveMapping(layout, ptr, "map_curve", 0, false, false, false, false, false);
  }
}

static void influence_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  weightvg_ui_common(C, &ob_ptr, ptr, layout);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_WeightVGEdit, panel_draw);
  modifier_subpanel_register(
      region_type, "falloff", "Falloff", nullptr, falloff_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "influence", "Influence", nullptr, influence_panel_draw, panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const WeightVGEditModifierData *wmd = (const WeightVGEditModifierData *)md;

  BLO_write_struct(writer, WeightVGEditModifierData, wmd);

  if (wmd->cmap_curve) {
    BKE_curvemapping_blend_write(writer, wmd->cmap_curve);
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

  BLO_read_struct(reader, CurveMapping, &wmd->cmap_curve);
  if (wmd->cmap_curve) {
    BKE_curvemapping_blend_read(reader, wmd->cmap_curve);
  }
}

ModifierTypeInfo modifierType_WeightVGEdit = {
    /*idname*/ "VertexWeightEdit",
    /*name*/ N_("VertexWeightEdit"),
    /*struct_name*/ "WeightVGEditModifierData",
    /*struct_size*/ sizeof(WeightVGEditModifierData),
    /*srna*/ &RNA_VertexWeightEditModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_VERTEX_WEIGHT,

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
