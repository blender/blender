/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_geometry_set.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_gpencil_legacy_lineart.h" /* Needed for line art cache functions. */
#include "MOD_grease_pencil_util.hh"
#include "MOD_lineart.h"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.hh"

namespace blender {

static void get_lineart_modifier_limits(const Object &ob, GreasePencilLineartLimitInfo &info)
{
  bool is_first = true;
  LISTBASE_FOREACH (const ModifierData *, md, &ob.modifiers) {
    if (md->type == eModifierType_GreasePencilLineart) {
      const auto *lmd = reinterpret_cast<const GreasePencilLineartModifierData *>(md);
      if (is_first || (lmd->flags & MOD_LINEART_USE_CACHE)) {
        info.min_level = std::min<int>(info.min_level, lmd->level_start);
        info.max_level = std::max<int>(
            info.max_level, lmd->use_multiple_levels ? lmd->level_end : lmd->level_start);
        info.edge_types |= lmd->edge_types;
        info.shadow_selection = std::max(info.shadow_selection, lmd->shadow_selection);
        info.silhouette_selection = std::max(info.silhouette_selection, lmd->silhouette_selection);
        is_first = false;
      }
    }
  }
}

static void set_lineart_modifier_limits(GreasePencilLineartModifierData &lmd,
                                        const GreasePencilLineartLimitInfo &info,
                                        const bool is_first_lineart)
{
  BLI_assert(lmd.modifier.type == eModifierType_GreasePencilLineart);
  if (is_first_lineart || lmd.flags & MOD_LINEART_USE_CACHE) {
    lmd.level_start_override = info.min_level;
    lmd.level_end_override = info.max_level;
    lmd.edge_types_override = info.edge_types;
    lmd.shadow_selection_override = info.shadow_selection;
    lmd.shadow_use_silhouette_override = info.silhouette_selection;
  }
  else {
    lmd.level_start_override = lmd.level_start;
    lmd.level_end_override = lmd.level_end;
    lmd.edge_types_override = lmd.edge_types;
    lmd.shadow_selection_override = lmd.shadow_selection;
    lmd.shadow_use_silhouette_override = lmd.silhouette_selection;
  }
}

static bool is_first_lineart(const GreasePencilLineartModifierData &md)
{
  if (md.modifier.type != eModifierType_GreasePencilLineart) {
    return false;
  }
  ModifierData *imd = md.modifier.prev;
  while (imd != nullptr) {
    if (imd->type == eModifierType_GreasePencilLineart) {
      return false;
    }
    imd = imd->prev;
  }
  return true;
}

static bool is_last_line_art(const GreasePencilLineartModifierData &md)
{
  if (md.modifier.type != eModifierType_GreasePencilLineart) {
    return false;
  }
  ModifierData *imd = md.modifier.next;
  while (imd != nullptr) {
    if (imd->type == eModifierType_GreasePencilLineart) {
      return false;
    }
    imd = imd->next;
  }
  return true;
}

static GreasePencilLineartModifierData *get_first_lineart_modifier(const Object &ob)
{
  LISTBASE_FOREACH (ModifierData *, i_md, &ob.modifiers) {
    if (i_md->type == eModifierType_GreasePencilLineart) {
      return reinterpret_cast<GreasePencilLineartModifierData *>(i_md);
    }
  }
  return nullptr;
}

static void init_data(ModifierData *md)
{
  GreasePencilLineartModifierData *gpmd = (GreasePencilLineartModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilLineartModifierData), modifier);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  BKE_modifier_copydata_generic(md, target, flag);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  GreasePencilLineartModifierData *lmd = (GreasePencilLineartModifierData *)md;

  if (lmd->target_layer[0] == '\0' || !lmd->target_material) {
    return true;
  }
  if (lmd->source_type == LINEART_SOURCE_OBJECT && !lmd->source_object) {
    return true;
  }
  if (lmd->source_type == LINEART_SOURCE_COLLECTION && !lmd->source_collection) {
    return true;
  }
  /* Preventing calculation in depsgraph when baking frames. */
  if (lmd->flags & MOD_LINEART_IS_BAKED) {
    return true;
  }

  return false;
}

static void add_this_collection(Collection &collection,
                                const ModifierUpdateDepsgraphContext *ctx,
                                const int mode)
{
  bool default_add = true;
  /* Do not do nested collection usage check, this is consistent with lineart calculation, because
   * collection usage doesn't have a INHERIT mode. This might initially be derived from the fact
   * that an object can be inside multiple collections, but might be irrelevant now with the way
   * objects are iterated. Keep this logic for now. */
  if (collection.lineart_usage & COLLECTION_LRT_EXCLUDE) {
    default_add = false;
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (&collection, ob, mode) {
    if (ELEM(ob->type, OB_MESH, OB_MBALL, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
      if ((ob->lineart.usage == OBJECT_LRT_INHERIT && default_add) ||
          ob->lineart.usage != OBJECT_LRT_EXCLUDE)
      {
        DEG_add_object_relation(ctx->node, ob, DEG_OB_COMP_GEOMETRY, "Line Art Modifier");
        DEG_add_object_relation(ctx->node, ob, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
      }
    }
    if (ob->type == OB_EMPTY && (ob->transflag & OB_DUPLICOLLECTION)) {
      if (!ob->instance_collection) {
        continue;
      }
      add_this_collection(*ob->instance_collection, ctx, mode);
    }
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");

  GreasePencilLineartModifierData *lmd = (GreasePencilLineartModifierData *)md;

  /* Always add whole master collection because line art will need the whole scene for
   * visibility computation. Line art exclusion is handled inside #add_this_collection. */

  /* Do we need to distinguish DAG_EVAL_VIEWPORT or DAG_EVAL_RENDER here? */

  add_this_collection(*ctx->scene->master_collection, ctx, DAG_EVAL_VIEWPORT);

  if (lmd->calculation_flags & MOD_LINEART_USE_CUSTOM_CAMERA && lmd->source_camera) {
    DEG_add_object_relation(
        ctx->node, lmd->source_camera, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
    DEG_add_object_relation(
        ctx->node, lmd->source_camera, DEG_OB_COMP_PARAMETERS, "Line Art Modifier");
  }
  else if (ctx->scene->camera) {
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_PARAMETERS, "Line Art Modifier");
  }
  if (lmd->light_contour_object) {
    DEG_add_object_relation(
        ctx->node, lmd->light_contour_object, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  GreasePencilLineartModifierData *lmd = (GreasePencilLineartModifierData *)md;

  walk(user_data, ob, (ID **)&lmd->source_collection, IDWALK_CB_NOP);

  walk(user_data, ob, (ID **)&lmd->source_object, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&lmd->source_camera, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&lmd->light_contour_object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  const int source_type = RNA_enum_get(ptr, "source_type");
  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (!is_first_lineart(*static_cast<const GreasePencilLineartModifierData *>(ptr->data))) {
    uiItemR(layout, ptr, "use_cache", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "source_type", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (source_type == LINEART_SOURCE_OBJECT) {
    uiItemR(layout, ptr, "source_object", UI_ITEM_NONE, nullptr, ICON_OBJECT_DATA);
  }
  else if (source_type == LINEART_SOURCE_COLLECTION) {
    uiLayout *sub = uiLayoutRow(layout, true);
    uiItemR(sub, ptr, "source_collection", UI_ITEM_NONE, nullptr, ICON_OUTLINER_COLLECTION);
    uiItemR(sub, ptr, "use_invert_collection", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }
  else {
    /* Source is Scene. */
  }

  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemPointerR(col, ptr, "target_layer", &obj_data_ptr, "layers", nullptr, ICON_GREASEPENCIL);
  uiItemPointerR(
      col, ptr, "target_material", &obj_data_ptr, "materials", nullptr, ICON_GREASEPENCIL);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "thickness", UI_ITEM_R_SLIDER, IFACE_("Line Thickness"), ICON_NONE);
  uiItemR(col, ptr, "opacity", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void edge_types_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));
  const bool has_light = RNA_pointer_get(ptr, "light_contour_object").data != nullptr;

  uiLayoutSetEnabled(layout, !is_baked);

  uiLayoutSetPropSep(layout, true);

  uiLayout *sub = uiLayoutRow(layout, false);
  uiLayoutSetActive(sub, has_light);
  uiItemR(sub,
          ptr,
          "shadow_region_filtering",
          UI_ITEM_NONE,
          IFACE_("Illumination Filtering"),
          ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);

  sub = uiLayoutRowWithHeading(col, false, IFACE_("Create"));
  uiItemR(sub, ptr, "use_contour", UI_ITEM_NONE, "", ICON_NONE);

  uiLayout *entry = uiLayoutRow(sub, true);
  uiLayoutSetActive(entry, RNA_boolean_get(ptr, "use_contour"));
  uiItemR(entry, ptr, "silhouette_filtering", UI_ITEM_NONE, "", ICON_NONE);

  const int silhouette_filtering = RNA_enum_get(ptr, "silhouette_filtering");
  if (silhouette_filtering != LINEART_SILHOUETTE_FILTER_NONE) {
    uiItemR(entry, ptr, "use_invert_silhouette", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }

  sub = uiLayoutRow(col, false);
  if (use_cache && !is_first) {
    uiItemR(sub, ptr, "use_crease", UI_ITEM_NONE, IFACE_("Crease (Angle Cached)"), ICON_NONE);
  }
  else {
    uiItemR(sub, ptr, "use_crease", UI_ITEM_NONE, "", ICON_NONE);
    uiItemR(sub,
            ptr,
            "crease_threshold",
            UI_ITEM_R_SLIDER | UI_ITEM_R_FORCE_BLANK_DECORATE,
            nullptr,
            ICON_NONE);
  }

  uiItemR(col, ptr, "use_intersection", UI_ITEM_NONE, IFACE_("Intersections"), ICON_NONE);
  uiItemR(col, ptr, "use_material", UI_ITEM_NONE, IFACE_("Material Borders"), ICON_NONE);
  uiItemR(col, ptr, "use_edge_mark", UI_ITEM_NONE, IFACE_("Edge Marks"), ICON_NONE);
  uiItemR(col, ptr, "use_loose", UI_ITEM_NONE, IFACE_("Loose"), ICON_NONE);

  entry = uiLayoutColumn(col, false);
  uiLayoutSetActive(entry, has_light);

  sub = uiLayoutRow(entry, false);
  uiItemR(sub, ptr, "use_light_contour", UI_ITEM_NONE, IFACE_("Light Contour"), ICON_NONE);

  uiItemR(entry,
          ptr,
          "use_shadow",
          UI_ITEM_NONE,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_GPENCIL, "Cast Shadow"),
          ICON_NONE);

  uiItemL(layout, IFACE_("Options"), ICON_NONE);

  sub = uiLayoutColumn(layout, false);
  if (use_cache && !is_first) {
    uiItemL(sub, IFACE_("Type overlapping cached"), ICON_INFO);
  }
  else {
    uiItemR(sub,
            ptr,
            "use_overlap_edge_type_support",
            UI_ITEM_NONE,
            IFACE_("Allow Overlapping Types"),
            ICON_NONE);
  }
}

static void options_light_reference_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool has_light = RNA_pointer_get(ptr, "light_contour_object").data != nullptr;
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, "Cached from the first line art modifier.", ICON_INFO);
    return;
  }

  uiItemR(layout, ptr, "light_contour_object", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayout *remaining = uiLayoutColumn(layout, false);
  uiLayoutSetActive(remaining, has_light);

  uiItemR(remaining, ptr, "shadow_camera_size", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayout *col = uiLayoutColumn(remaining, true);
  uiItemR(col, ptr, "shadow_camera_near", UI_ITEM_NONE, IFACE_("Near"), ICON_NONE);
  uiItemR(col, ptr, "shadow_camera_far", UI_ITEM_NONE, IFACE_("Far"), ICON_NONE);
}

static void options_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *row = uiLayoutRowWithHeading(layout, false, IFACE_("Custom Camera"));
  uiItemR(row, ptr, "use_custom_camera", UI_ITEM_NONE, "", ICON_NONE);
  uiLayout *subrow = uiLayoutRow(row, true);
  uiLayoutSetActive(subrow, RNA_boolean_get(ptr, "use_custom_camera"));
  uiLayoutSetPropSep(subrow, true);
  uiItemR(subrow, ptr, "source_camera", UI_ITEM_NONE, "", ICON_OBJECT_DATA);

  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemR(col,
          ptr,
          "use_edge_overlap",
          UI_ITEM_NONE,
          IFACE_("Overlapping Edges As Contour"),
          ICON_NONE);
  uiItemR(col, ptr, "use_object_instances", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_clip_plane_boundaries", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_crease_on_smooth", UI_ITEM_NONE, IFACE_("Crease On Smooth"), ICON_NONE);
  uiItemR(col, ptr, "use_crease_on_sharp", UI_ITEM_NONE, IFACE_("Crease On Sharp"), ICON_NONE);
  uiItemR(col,
          ptr,
          "use_back_face_culling",
          UI_ITEM_NONE,
          IFACE_("Force Backface Culling"),
          ICON_NONE);
}

static void occlusion_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  const bool use_multiple_levels = RNA_boolean_get(ptr, "use_multiple_levels");
  const bool show_in_front = RNA_boolean_get(&ob_ptr, "show_in_front");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (!show_in_front) {
    uiItemL(layout, TIP_("Object is not in front"), ICON_INFO);
  }

  layout = uiLayoutColumn(layout, false);
  uiLayoutSetActive(layout, show_in_front);

  uiItemR(layout, ptr, "use_multiple_levels", UI_ITEM_NONE, IFACE_("Range"), ICON_NONE);

  if (use_multiple_levels) {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "level_start", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "level_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "level_start", UI_ITEM_NONE, IFACE_("Level"), ICON_NONE);
  }
}

static bool anything_showing_through(PointerRNA *ptr)
{
  const bool use_multiple_levels = RNA_boolean_get(ptr, "use_multiple_levels");
  const int level_start = RNA_int_get(ptr, "level_start");
  const int level_end = RNA_int_get(ptr, "level_end");
  if (use_multiple_levels) {
    return std::max(level_start, level_end) > 0;
  }
  return level_start > 0;
}

static void material_mask_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool show_in_front = RNA_boolean_get(&ob_ptr, "show_in_front");

  uiLayoutSetEnabled(layout, !is_baked);
  uiLayoutSetActive(layout, show_in_front && anything_showing_through(ptr));

  uiItemR(layout, ptr, "use_material_mask", UI_ITEM_NONE, IFACE_("Material Mask"), ICON_NONE);
}

static void material_mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  uiLayoutSetEnabled(layout, !is_baked);
  uiLayoutSetActive(layout, anything_showing_through(ptr));

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetEnabled(layout, RNA_boolean_get(ptr, "use_material_mask"));

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayout *sub = uiLayoutRowWithHeading(col, true, IFACE_("Masks"));

  PropertyRNA *prop = RNA_struct_find_property(ptr, "use_material_mask_bits");
  for (int i = 0; i < 8; i++) {
    uiItemFullR(sub, ptr, prop, i, 0, UI_ITEM_R_TOGGLE, " ", ICON_NONE);
    if (i == 3) {
      sub = uiLayoutRow(col, true);
    }
  }

  uiItemR(layout, ptr, "use_material_mask_match", UI_ITEM_NONE, IFACE_("Exact Match"), ICON_NONE);
}

static void intersection_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  uiLayoutSetEnabled(layout, !is_baked);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_intersection"));

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayout *sub = uiLayoutRowWithHeading(col, true, IFACE_("Collection Masks"));

  PropertyRNA *prop = RNA_struct_find_property(ptr, "use_intersection_mask");
  for (int i = 0; i < 8; i++) {
    uiItemFullR(sub, ptr, prop, i, 0, UI_ITEM_R_TOGGLE, " ", ICON_NONE);
    if (i == 3) {
      sub = uiLayoutRow(col, true);
    }
  }

  uiItemR(layout, ptr, "use_intersection_match", UI_ITEM_NONE, IFACE_("Exact Match"), ICON_NONE);
}

static void face_mark_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));

  if (!use_cache || is_first) {
    uiLayoutSetEnabled(layout, !is_baked);
    uiItemR(layout, ptr, "use_face_mark", UI_ITEM_NONE, IFACE_("Face Mark Filtering"), ICON_NONE);
  }
  else {
    uiItemL(layout, IFACE_("Face Mark Filtering"), ICON_NONE);
  }
}

static void face_mark_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_mark = RNA_boolean_get(ptr, "use_face_mark");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));

  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, use_mark);

  uiItemR(layout, ptr, "use_face_mark_invert", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_face_mark_boundaries", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_face_mark_keep_contour", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void chaining_panel_draw(const bContext * /*C*/, Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));
  const bool is_geom = RNA_boolean_get(ptr, "use_geometry_space_chain");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *col = uiLayoutColumnWithHeading(layout, true, IFACE_("Chain"));
  uiItemR(col, ptr, "use_fuzzy_intersections", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_fuzzy_all", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_loose_edge_chain", UI_ITEM_NONE, IFACE_("Loose Edges"), ICON_NONE);
  uiItemR(
      col, ptr, "use_loose_as_contour", UI_ITEM_NONE, IFACE_("Loose Edges As Contour"), ICON_NONE);
  uiItemR(col, ptr, "use_detail_preserve", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_geometry_space_chain", UI_ITEM_NONE, IFACE_("Geometry Space"), ICON_NONE);

  uiItemR(layout,
          ptr,
          "chaining_image_threshold",
          UI_ITEM_NONE,
          is_geom ? IFACE_("Geometry Threshold") : nullptr,
          ICON_NONE);

  uiItemR(layout, ptr, "smooth_tolerance", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "split_angle", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

static void vgroup_panel_draw(const bContext * /*C*/, Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = is_first_lineart(
      *static_cast<const GreasePencilLineartModifierData *>(ptr->data));

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *col = uiLayoutColumn(layout, true);

  uiLayout *row = uiLayoutRow(col, true);

  uiItemR(
      row, ptr, "source_vertex_group", UI_ITEM_NONE, IFACE_("Filter Source"), ICON_GROUP_VERTEX);
  uiItemR(row, ptr, "invert_source_vertex_group", UI_ITEM_R_TOGGLE, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(col, ptr, "use_output_vertex_group_match_by_name", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemPointerR(col, ptr, "vertex_group", &ob_ptr, "vertex_groups", IFACE_("Target"), ICON_NONE);
}

static void bake_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);

  if (is_baked) {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, false);
    uiItemL(col, TIP_("Modifier has baked data"), ICON_NONE);
    uiItemR(
        col, ptr, "is_baked", UI_ITEM_R_TOGGLE, IFACE_("Continue Without Clearing"), ICON_NONE);
  }

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(col, !is_baked);
  uiItemO(col, nullptr, ICON_NONE, "OBJECT_OT_lineart_bake_strokes");
  uiItemO(col, nullptr, ICON_NONE, "OBJECT_OT_lineart_bake_strokes_all");

  col = uiLayoutColumn(layout, false);
  uiItemO(col, nullptr, ICON_NONE, "OBJECT_OT_lineart_clear");
  uiItemO(col, nullptr, ICON_NONE, "OBJECT_OT_lineart_clear_all");
}

static void composition_panel_draw(const bContext * /*C*/, Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool show_in_front = RNA_boolean_get(&ob_ptr, "show_in_front");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "overscan", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_image_boundary_trimming", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (show_in_front) {
    uiItemL(layout, TIP_("Object is shown in front"), ICON_ERROR);
  }

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, !show_in_front);

  uiItemR(col, ptr, "stroke_depth_offset", UI_ITEM_R_SLIDER, IFACE_("Depth Offset"), ICON_NONE);
  uiItemR(col,
          ptr,
          "use_offset_towards_custom_camera",
          UI_ITEM_NONE,
          IFACE_("Towards Custom Camera"),
          ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_GreasePencilLineart, panel_draw);

  modifier_subpanel_register(
      region_type, "edge_types", "Edge Types", nullptr, edge_types_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "light_reference",
                             "Light Reference",
                             nullptr,
                             options_light_reference_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "geometry", "Geometry Processing", nullptr, options_panel_draw, panel_type);
  PanelType *occlusion_panel = modifier_subpanel_register(
      region_type, "occlusion", "Occlusion", nullptr, occlusion_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "material_mask",
                             "",
                             material_mask_panel_draw_header,
                             material_mask_panel_draw,
                             occlusion_panel);
  modifier_subpanel_register(
      region_type, "intersection", "Intersection", nullptr, intersection_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "face_mark", "", face_mark_panel_draw_header, face_mark_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "chaining", "Chaining", nullptr, chaining_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "vgroup", "Vertex Weight Transfer", nullptr, vgroup_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "composition", "Composition", nullptr, composition_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "bake", "Bake", nullptr, bake_panel_draw, panel_type);
}

static void generate_strokes(ModifierData &md,
                             const ModifierEvalContext &ctx,
                             GreasePencil &grease_pencil,
                             GreasePencilLineartModifierData &first_lineart)
{
  auto &lmd = reinterpret_cast<GreasePencilLineartModifierData &>(md);

  bke::greasepencil::TreeNode *node = grease_pencil.find_node_by_name(lmd.target_layer);
  if (!node || !node->is_layer()) {
    return;
  }

  LineartCache *local_lc = first_lineart.shared_cache;

  if (!(lmd.flags & MOD_LINEART_USE_CACHE)) {
    MOD_lineart_compute_feature_lines_v3(
        ctx.depsgraph, lmd, &local_lc, !(ctx.object->dtx & OB_DRAW_IN_FRONT));
    MOD_lineart_destroy_render_data_v3(&lmd);
  }
  MOD_lineart_chain_clear_picked_flag(local_lc);
  lmd.cache = local_lc;

  const int current_frame = grease_pencil.runtime->eval_frame;

  /* Ensure we have a frame in the selected layer to put line art result in. */
  bke::greasepencil::Layer &layer = node->as_layer();

  bke::greasepencil::Drawing &drawing = [&]() -> bke::greasepencil::Drawing & {
    if (bke::greasepencil::Drawing *drawing = grease_pencil.get_editable_drawing_at(layer,
                                                                                    current_frame))
    {
      return *drawing;
    }
    grease_pencil.insert_blank_frame(layer, current_frame, 0, BEZT_KEYTYPE_KEYFRAME);
    return *grease_pencil.get_editable_drawing_at(layer, current_frame);
  }();

  const float4x4 &mat = ctx.object->world_to_object();

  MOD_lineart_gpencil_generate_v3(
      lmd.cache,
      mat,
      ctx.depsgraph,
      drawing,
      lmd.source_type,
      lmd.source_object,
      lmd.source_collection,
      lmd.level_start,
      lmd.use_multiple_levels ? lmd.level_end : lmd.level_start,
      lmd.target_material ? BKE_object_material_index_get(ctx.object, lmd.target_material) : 0,
      lmd.edge_types,
      lmd.mask_switches,
      lmd.material_mask_bits,
      lmd.intersection_mask,
      float(lmd.thickness) / 1000.0f,
      lmd.opacity,
      lmd.shadow_selection,
      lmd.silhouette_selection,
      lmd.source_vertex_group,
      lmd.vgname,
      lmd.flags,
      lmd.calculation_flags);

  if (!(lmd.flags & MOD_LINEART_USE_CACHE)) {
    /* Clear local cache. */
    if (local_lc != first_lineart.shared_cache) {
      MOD_lineart_clear_cache(&local_lc);
    }
    /* Restore the original cache pointer so the modifiers below still have access to the "global"
     * cache. */
    lmd.cache = first_lineart.shared_cache;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  auto mmd = reinterpret_cast<GreasePencilLineartModifierData *>(md);

  GreasePencilLineartModifierData *first_lineart = get_first_lineart_modifier(*ctx->object);
  BLI_assert(first_lineart);

  bool is_first_lineart = (mmd == first_lineart);

  if (is_first_lineart) {
    mmd->shared_cache = MOD_lineart_init_cache();
    get_lineart_modifier_limits(*ctx->object, mmd->shared_cache->LimitInfo);
  }
  set_lineart_modifier_limits(*mmd, first_lineart->shared_cache->LimitInfo, is_first_lineart);

  generate_strokes(*md, *ctx, grease_pencil, *first_lineart);

  if (is_last_line_art(*mmd)) {
    MOD_lineart_clear_cache(&first_lineart->shared_cache);
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *lmd = reinterpret_cast<const GreasePencilLineartModifierData *>(md);

  BLO_write_struct(writer, GreasePencilLineartModifierData, lmd);
}
}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilLineart = {
    /*idname*/ "Lineart Modifier",
    /*name*/ N_("Lineart"),
    /*struct_name*/ "GreasePencilLineartModifierData",
    /*struct_size*/ sizeof(GreasePencilLineartModifierData),
    /*srna*/ &RNA_GreasePencilLineartModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil,
    /*icon*/ ICON_MOD_LINEART,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ blender::is_disabled,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ nullptr,
};
