/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_span.hh"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh_mirror.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph_build.hh"

#include "MOD_ui_common.hh"

#include "GEO_mesh_merge_by_distance.hh"

using namespace blender;

static void init_data(ModifierData *md)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(MirrorModifierData), modifier);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->mirror_ob, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;
  if (mmd->mirror_ob != nullptr) {
    DEG_add_object_relation(ctx->node, mmd->mirror_ob, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Mirror Modifier");
  }
}

static Mesh *mirror_apply_on_axis(MirrorModifierData *mmd,
                                  Object *ob,
                                  Mesh *mesh,
                                  const int axis,
                                  const bool use_correct_order_on_merge)
{
  int *vert_merge_map = nullptr;
  int vert_merge_map_len;
  Mesh *result = mesh;
  result = BKE_mesh_mirror_apply_mirror_on_axis_for_modifier(
      mmd, ob, result, axis, use_correct_order_on_merge, &vert_merge_map, &vert_merge_map_len);

  if (vert_merge_map) {
    /* Slow - so only call if one or more merge verts are found,
     * users may leave this on and not realize there is nothing to merge - campbell */

    /* TODO(mano-wii): Polygons with all vertices merged are the ones that form duplicates.
     * Therefore the duplicate face test can be skipped. */
    if (vert_merge_map_len) {
      Mesh *tmp = result;
      result = geometry::mesh_merge_verts(
          *tmp, MutableSpan<int>{vert_merge_map, result->verts_num}, vert_merge_map_len, false);
      BKE_id_free(nullptr, tmp);
    }
    MEM_freeN(vert_merge_map);
  }

  return result;
}

static Mesh *mirrorModifier__doMirror(MirrorModifierData *mmd, Object *ob, Mesh *mesh)
{
  Mesh *result = mesh;
  const bool use_correct_order_on_merge = mmd->use_correct_order_on_merge;

  /* check which axes have been toggled and mirror accordingly */
  if (mmd->flag & MOD_MIR_AXIS_X) {
    result = mirror_apply_on_axis(mmd, ob, result, 0, use_correct_order_on_merge);
  }
  if (mmd->flag & MOD_MIR_AXIS_Y) {
    Mesh *tmp = result;
    result = mirror_apply_on_axis(mmd, ob, result, 1, use_correct_order_on_merge);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(nullptr, tmp);
    }
  }
  if (mmd->flag & MOD_MIR_AXIS_Z) {
    Mesh *tmp = result;
    result = mirror_apply_on_axis(mmd, ob, result, 2, use_correct_order_on_merge);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(nullptr, tmp);
    }
  }

  return result;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  result = mirrorModifier__doMirror(mmd, ctx->object, mesh);

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col, *sub;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PropertyRNA *prop;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  MirrorModifierData *mmd = (MirrorModifierData *)ptr->data;
  bool has_bisect = (mmd->flag &
                     (MOD_MIR_BISECT_AXIS_X | MOD_MIR_BISECT_AXIS_Y | MOD_MIR_BISECT_AXIS_Z));

  col = &layout->column(false);
  col->use_property_split_set(true);

  prop = RNA_struct_find_property(ptr, "use_axis");
  row = &col->row(true, IFACE_("Axis"));
  row->prop(ptr, prop, 0, 0, toggles_flag, IFACE_("X"), ICON_NONE);
  row->prop(ptr, prop, 1, 0, toggles_flag, IFACE_("Y"), ICON_NONE);
  row->prop(ptr, prop, 2, 0, toggles_flag, IFACE_("Z"), ICON_NONE);

  prop = RNA_struct_find_property(ptr, "use_bisect_axis");
  row = &col->row(true, IFACE_("Bisect"));
  row->prop(ptr, prop, 0, 0, toggles_flag, IFACE_("X"), ICON_NONE);
  row->prop(ptr, prop, 1, 0, toggles_flag, IFACE_("Y"), ICON_NONE);
  row->prop(ptr, prop, 2, 0, toggles_flag, IFACE_("Z"), ICON_NONE);

  prop = RNA_struct_find_property(ptr, "use_bisect_flip_axis");
  row = &col->row(true, IFACE_("Flip"));
  row->active_set(has_bisect);
  row->prop(ptr, prop, 0, 0, toggles_flag, IFACE_("X"), ICON_NONE);
  row->prop(ptr, prop, 1, 0, toggles_flag, IFACE_("Y"), ICON_NONE);
  row->prop(ptr, prop, 2, 0, toggles_flag, IFACE_("Z"), ICON_NONE);

  col->separator();

  col->prop(ptr, "mirror_object", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->prop(
      ptr, "use_clip", UI_ITEM_NONE, CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Clipping"), ICON_NONE);

  row = &col->row(true, IFACE_("Merge"));
  row->prop(ptr, "use_mirror_merge", UI_ITEM_NONE, "", ICON_NONE);
  sub = &row->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_mirror_merge"));
  sub->prop(ptr, "merge_threshold", UI_ITEM_NONE, "", ICON_NONE);

  sub = &col->row(true);
  sub->active_set(has_bisect);
  sub->prop(ptr, "bisect_threshold", UI_ITEM_NONE, IFACE_("Bisect Distance"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void data_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col, *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(true);
  row = &col->row(true, IFACE_("Mirror U"));
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_mirror_u", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_mirror_u"));
  sub->prop(ptr, "mirror_offset_u", UI_ITEM_R_SLIDER, "", ICON_NONE);
  row->decorator(ptr, "mirror_offset_u", 0);

  row = &col->row(true, IFACE_("V"));
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_mirror_v", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_mirror_v"));
  sub->prop(ptr, "mirror_offset_v", UI_ITEM_R_SLIDER, "", ICON_NONE);
  row->decorator(ptr, "mirror_offset_v", 0);

  col = &layout->column(true);
  col->prop(ptr, "offset_u", UI_ITEM_R_SLIDER, IFACE_("Offset U"), ICON_NONE);
  col->prop(ptr, "offset_v", UI_ITEM_R_SLIDER, IFACE_("V"), ICON_NONE);

  layout->prop(ptr, "use_mirror_vertex_groups", UI_ITEM_NONE, IFACE_("Vertex Groups"), ICON_NONE);
  layout->prop(ptr, "use_mirror_udim", UI_ITEM_NONE, IFACE_("Flip UDIM"), ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Mirror, panel_draw);
  modifier_subpanel_register(region_type, "data", "Data", nullptr, data_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Mirror = {
    /*idname*/ "Mirror",
    /*name*/ N_("Mirror"),
    /*struct_name*/ "MirrorModifierData",
    /*struct_size*/ sizeof(MirrorModifierData),
    /*srna*/ &RNA_MirrorModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_MIRROR,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
