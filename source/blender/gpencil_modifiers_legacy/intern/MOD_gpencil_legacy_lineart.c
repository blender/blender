/* SPDX-FileCopyrightText: 2017 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_modifier.h"
#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_legacy_lineart.h"
#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "lineart/MOD_lineart.h"

static void initData(GpencilModifierData *md)
{
  LineartGpencilModifierData *gpmd = (LineartGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(LineartGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void generate_strokes_actual(
    GpencilModifierData *md, Depsgraph *depsgraph, Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  if (G.debug_value == 4000) {
    printf("LRT: Generating from modifier.\n");
  }

  MOD_lineart_gpencil_generate(
      lmd->cache,
      depsgraph,
      ob,
      gpl,
      gpf,
      lmd->source_type,
      lmd->source_type == LRT_SOURCE_OBJECT ? (void *)lmd->source_object :
                                              (void *)lmd->source_collection,
      lmd->level_start,
      lmd->use_multiple_levels ? lmd->level_end : lmd->level_start,
      lmd->target_material ? BKE_gpencil_object_material_index_get(ob, lmd->target_material) : 0,
      lmd->edge_types,
      lmd->mask_switches,
      lmd->material_mask_bits,
      lmd->intersection_mask,
      lmd->thickness,
      lmd->opacity,
      lmd->shadow_selection,
      lmd->silhouette_selection,
      lmd->source_vertex_group,
      lmd->vgname,
      lmd->flags,
      lmd->calculation_flags);
}

static bool isModifierDisabled(GpencilModifierData *md)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  if ((lmd->target_layer[0] == '\0') || (lmd->target_material == NULL)) {
    return true;
  }

  if (lmd->source_type == LRT_SOURCE_OBJECT && !lmd->source_object) {
    return true;
  }

  if (lmd->source_type == LRT_SOURCE_COLLECTION && !lmd->source_collection) {
    return true;
  }

  /* Preventing calculation in depsgraph when baking frames. */
  if (lmd->flags & LRT_GPENCIL_IS_BAKED) {
    return true;
  }

  return false;
}
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  bGPdata *gpd = ob->data;

  /* Guard early, don't trigger calculation when no grease-pencil frame is present.
   * Probably should disable in the #isModifierDisabled() function
   * but we need additional argument for depsgraph and `gpd`. */
  bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
  if (gpl == NULL) {
    return;
  }
  /* Need to call this or we don't get active frame (user may haven't selected any one). */
  BKE_gpencil_frame_active_set(depsgraph, gpd);
  bGPDframe *gpf = gpl->actframe;
  if (gpf == NULL) {
    return;
  }

  /* Check all parameters required are filled. */
  if (isModifierDisabled(md)) {
    return;
  }

  LineartCache *local_lc = gpd->runtime.lineart_cache;
  if (!gpd->runtime.lineart_cache) {
    MOD_lineart_compute_feature_lines(
        depsgraph, lmd, &gpd->runtime.lineart_cache, !(ob->dtx & OB_DRAW_IN_FRONT));
    MOD_lineart_destroy_render_data(lmd);
  }
  else {
    if (!(lmd->flags & LRT_GPENCIL_USE_CACHE)) {
      MOD_lineart_compute_feature_lines(depsgraph, lmd, &local_lc, !(ob->dtx & OB_DRAW_IN_FRONT));
      MOD_lineart_destroy_render_data(lmd);
    }
    MOD_lineart_chain_clear_picked_flag(local_lc);
    lmd->cache = local_lc;
  }

  generate_strokes_actual(md, depsgraph, ob, gpl, gpf);

  if (!(lmd->flags & LRT_GPENCIL_USE_CACHE)) {
    /* Clear local cache. */
    if (local_lc != gpd->runtime.lineart_cache) {
      MOD_lineart_clear_cache(&local_lc);
    }
    /* Restore the original cache pointer so the modifiers below still have access to the "global"
     * cache. */
    lmd->cache = gpd->runtime.lineart_cache;
  }
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
  if (gpl == NULL) {
    return;
  }
  bGPDframe *gpf = gpl->actframe;
  if (gpf == NULL) {
    return;
  }

  if (!gpd->runtime.lineart_cache) {
    /* Only calculate for this modifier, thus no need to get maximum values from all line art
     * modifiers in the stack. */
    lmd->edge_types_override = lmd->edge_types;
    lmd->level_end_override = lmd->level_end;
    lmd->shadow_selection_override = lmd->shadow_selection;

    MOD_lineart_compute_feature_lines(
        depsgraph, lmd, &gpd->runtime.lineart_cache, !(ob->dtx & OB_DRAW_IN_FRONT));
    MOD_lineart_destroy_render_data(lmd);
  }

  generate_strokes_actual(md, depsgraph, ob, gpl, gpf);

  MOD_lineart_clear_cache(&gpd->runtime.lineart_cache);
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  return isModifierDisabled(md);
}

static void add_this_collection(Collection *c,
                                const ModifierUpdateDepsgraphContext *ctx,
                                const int mode)
{
  if (!c) {
    return;
  }
  bool default_add = true;
  /* Do not do nested collection usage check, this is consistent with lineart calculation, because
   * collection usage doesn't have a INHERIT mode. This might initially be derived from the fact
   * that an object can be inside multiple collections, but might be irrelevant now with the way
   * objects are iterated. Keep this logic for now. */
  if (c->lineart_usage & COLLECTION_LRT_EXCLUDE) {
    default_add = false;
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (c, ob, mode) {
    if (ELEM(ob->type, OB_MESH, OB_MBALL, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
      if ((ob->lineart.usage == OBJECT_LRT_INHERIT && default_add) ||
          ob->lineart.usage != OBJECT_LRT_EXCLUDE)
      {
        DEG_add_object_relation(ctx->node, ob, DEG_OB_COMP_GEOMETRY, "Line Art Modifier");
        DEG_add_object_relation(ctx->node, ob, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
      }
    }
    if (ob->type == OB_EMPTY && (ob->transflag & OB_DUPLICOLLECTION)) {
      add_this_collection(ob->instance_collection, ctx, mode);
    }
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int mode)
{
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");

  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  /* Always add whole master collection because line art will need the whole scene for
   * visibility computation. Line art exclusion is handled inside #add_this_collection. */
  add_this_collection(ctx->scene->master_collection, ctx, mode);

  if (lmd->calculation_flags & LRT_USE_CUSTOM_CAMERA && lmd->source_camera) {
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

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  walk(userData, ob, (ID **)&lmd->target_material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&lmd->source_collection, IDWALK_CB_NOP);

  walk(userData, ob, (ID **)&lmd->source_object, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&lmd->source_camera, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&lmd->light_contour_object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  const int source_type = RNA_enum_get(ptr, "source_type");
  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (!BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data)) {
    uiItemR(layout, ptr, "use_cache", 0, NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "source_type", 0, NULL, ICON_NONE);

  if (source_type == LRT_SOURCE_OBJECT) {
    uiItemR(layout, ptr, "source_object", 0, NULL, ICON_OBJECT_DATA);
  }
  else if (source_type == LRT_SOURCE_COLLECTION) {
    uiLayout *sub = uiLayoutRow(layout, true);
    uiItemR(sub, ptr, "source_collection", 0, NULL, ICON_OUTLINER_COLLECTION);
    uiItemR(sub, ptr, "use_invert_collection", 0, "", ICON_ARROW_LEFTRIGHT);
  }
  else {
    /* Source is Scene. */
  }
  uiItemPointerR(layout, ptr, "target_layer", &obj_data_ptr, "layers", NULL, ICON_GREASEPENCIL);

  /* Material has to be used by grease pencil object already, it was possible to assign materials
   * without this requirement in earlier versions of blender. */
  bool material_valid = false;
  PointerRNA material_ptr = RNA_pointer_get(ptr, "target_material");
  if (!RNA_pointer_is_null(&material_ptr)) {
    Material *current_material = material_ptr.data;
    Object *ob = ob_ptr.data;
    material_valid = BKE_gpencil_object_material_index_get(ob, current_material) != -1;
  }
  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetRedAlert(row, !material_valid);
  uiItemPointerR(
      row, ptr, "target_material", &obj_data_ptr, "materials", NULL, ICON_SHADING_TEXTURE);

  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "thickness", UI_ITEM_R_SLIDER, IFACE_("Line Thickness"), ICON_NONE);
  uiItemR(col, ptr, "opacity", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void edge_types_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);
  const bool has_light = RNA_pointer_get(ptr, "light_contour_object").data != NULL;

  uiLayoutSetEnabled(layout, !is_baked);

  uiLayoutSetPropSep(layout, true);

  uiLayout *sub = uiLayoutRow(layout, false);
  uiLayoutSetActive(sub, has_light);
  uiItemR(sub, ptr, "shadow_region_filtering", 0, IFACE_("Illumination Filtering"), ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);

  sub = uiLayoutRowWithHeading(col, false, IFACE_("Create"));
  uiItemR(sub, ptr, "use_contour", 0, "", ICON_NONE);

  uiLayout *entry = uiLayoutRow(sub, true);
  uiLayoutSetActive(entry, RNA_boolean_get(ptr, "use_contour"));
  uiItemR(entry, ptr, "silhouette_filtering", 0, "", ICON_NONE);

  const int silhouette_filtering = RNA_enum_get(ptr, "silhouette_filtering");
  if (silhouette_filtering != LRT_SILHOUETTE_FILTER_NONE) {
    uiItemR(entry, ptr, "use_invert_silhouette", 0, "", ICON_ARROW_LEFTRIGHT);
  }

  sub = uiLayoutRow(col, false);
  if (use_cache && !is_first) {
    uiItemR(sub, ptr, "use_crease", 0, IFACE_("Crease (Angle Cached)"), ICON_NONE);
  }
  else {
    uiItemR(sub, ptr, "use_crease", 0, "", ICON_NONE);
    uiItemR(sub,
            ptr,
            "crease_threshold",
            UI_ITEM_R_SLIDER | UI_ITEM_R_FORCE_BLANK_DECORATE,
            NULL,
            ICON_NONE);
  }

  uiItemR(col, ptr, "use_intersection", 0, IFACE_("Intersections"), ICON_NONE);
  uiItemR(col, ptr, "use_material", 0, IFACE_("Material Borders"), ICON_NONE);
  uiItemR(col, ptr, "use_edge_mark", 0, IFACE_("Edge Marks"), ICON_NONE);
  uiItemR(col, ptr, "use_loose", 0, IFACE_("Loose"), ICON_NONE);

  entry = uiLayoutColumn(col, false);
  uiLayoutSetActive(entry, has_light);

  sub = uiLayoutRow(entry, false);
  uiItemR(sub, ptr, "use_light_contour", 0, IFACE_("Light Contour"), ICON_NONE);

  uiItemR(entry,
          ptr,
          "use_shadow",
          0,
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
            0,
            IFACE_("Allow Overlapping Types"),
            ICON_NONE);
  }
}

static void options_light_reference_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool has_light = RNA_pointer_get(ptr, "light_contour_object").data != NULL;
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, "Cached from the first line art modifier.", ICON_INFO);
    return;
  }

  uiItemR(layout, ptr, "light_contour_object", 0, NULL, ICON_NONE);

  uiLayout *remaining = uiLayoutColumn(layout, false);
  uiLayoutSetActive(remaining, has_light);

  uiItemR(remaining, ptr, "shadow_camera_size", 0, NULL, ICON_NONE);

  uiLayout *col = uiLayoutColumn(remaining, true);
  uiItemR(col, ptr, "shadow_camera_near", 0, IFACE_("Near"), ICON_NONE);
  uiItemR(col, ptr, "shadow_camera_far", 0, IFACE_("Far"), ICON_NONE);
}

static void options_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *row = uiLayoutRowWithHeading(layout, false, IFACE_("Custom Camera"));
  uiItemR(row, ptr, "use_custom_camera", 0, "", 0);
  uiLayout *subrow = uiLayoutRow(row, true);
  uiLayoutSetActive(subrow, RNA_boolean_get(ptr, "use_custom_camera"));
  uiLayoutSetPropSep(subrow, true);
  uiItemR(subrow, ptr, "source_camera", 0, "", ICON_OBJECT_DATA);

  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemR(col, ptr, "use_edge_overlap", 0, IFACE_("Overlapping Edges As Contour"), ICON_NONE);
  uiItemR(col, ptr, "use_object_instances", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_clip_plane_boundaries", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_crease_on_smooth", 0, IFACE_("Crease On Smooth"), ICON_NONE);
  uiItemR(col, ptr, "use_crease_on_sharp", 0, IFACE_("Crease On Sharp"), ICON_NONE);
  uiItemR(col, ptr, "use_back_face_culling", 0, IFACE_("Force Backface Culling"), ICON_NONE);
}

static void occlusion_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

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

  uiItemR(layout, ptr, "use_multiple_levels", 0, IFACE_("Range"), ICON_NONE);

  if (use_multiple_levels) {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "level_start", 0, NULL, ICON_NONE);
    uiItemR(col, ptr, "level_end", 0, IFACE_("End"), ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "level_start", 0, IFACE_("Level"), ICON_NONE);
  }
}

static bool anything_showing_through(PointerRNA *ptr)
{
  const bool use_multiple_levels = RNA_boolean_get(ptr, "use_multiple_levels");
  const int level_start = RNA_int_get(ptr, "level_start");
  const int level_end = RNA_int_get(ptr, "level_end");
  if (use_multiple_levels) {
    return (MAX2(level_start, level_end) > 0);
  }
  return (level_start > 0);
}

static void material_mask_panel_draw_header(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool show_in_front = RNA_boolean_get(&ob_ptr, "show_in_front");

  uiLayoutSetEnabled(layout, !is_baked);
  uiLayoutSetActive(layout, show_in_front && anything_showing_through(ptr));

  uiItemR(layout, ptr, "use_material_mask", 0, IFACE_("Material Mask"), ICON_NONE);
}

static void material_mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

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

  uiItemR(layout, ptr, "use_material_mask_match", 0, IFACE_("Exact Match"), ICON_NONE);
}

static void intersection_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

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

  uiItemR(layout, ptr, "use_intersection_match", 0, IFACE_("Exact Match"), ICON_NONE);
}

static void face_mark_panel_draw_header(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);

  if (!use_cache || is_first) {
    uiLayoutSetEnabled(layout, !is_baked);
    uiItemR(layout, ptr, "use_face_mark", 0, IFACE_("Face Mark Filtering"), ICON_NONE);
  }
  else {
    uiItemL(layout, IFACE_("Face Mark Filtering"), ICON_NONE);
  }
}

static void face_mark_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_mark = RNA_boolean_get(ptr, "use_face_mark");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);

  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, use_mark);

  uiItemR(layout, ptr, "use_face_mark_invert", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_face_mark_boundaries", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_face_mark_keep_contour", 0, NULL, ICON_NONE);
}

static void chaining_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);
  const bool is_geom = RNA_boolean_get(ptr, "use_geometry_space_chain");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *col = uiLayoutColumnWithHeading(layout, true, IFACE_("Chain"));
  uiItemR(col, ptr, "use_fuzzy_intersections", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_fuzzy_all", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_loose_edge_chain", 0, IFACE_("Loose Edges"), ICON_NONE);
  uiItemR(col, ptr, "use_loose_as_contour", 0, IFACE_("Loose Edges As Contour"), ICON_NONE);
  uiItemR(col, ptr, "use_detail_preserve", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_geometry_space_chain", 0, IFACE_("Geometry Space"), ICON_NONE);

  uiItemR(layout,
          ptr,
          "chaining_image_threshold",
          0,
          is_geom ? IFACE_("Geometry Threshold") : NULL,
          ICON_NONE);

  uiItemR(layout, ptr, "smooth_tolerance", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(layout, ptr, "split_angle", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void vgroup_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  const bool use_cache = RNA_boolean_get(ptr, "use_cache");
  const bool is_first = BKE_gpencil_is_first_lineart_in_stack(ob_ptr.data, ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  if (use_cache && !is_first) {
    uiItemL(layout, TIP_("Cached from the first line art modifier"), ICON_INFO);
    return;
  }

  uiLayout *col = uiLayoutColumn(layout, true);

  uiLayout *row = uiLayoutRow(col, true);

  uiItemR(row, ptr, "source_vertex_group", 0, IFACE_("Filter Source"), ICON_GROUP_VERTEX);
  uiItemR(row, ptr, "invert_source_vertex_group", UI_ITEM_R_TOGGLE, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(col, ptr, "use_output_vertex_group_match_by_name", 0, NULL, ICON_NONE);

  const bool match_output = RNA_boolean_get(ptr, "use_output_vertex_group_match_by_name");
  if (!match_output) {
    uiItemPointerR(
        col, ptr, "vertex_group", &ob_ptr, "vertex_groups", IFACE_("Target"), ICON_NONE);
  }
}

static void bake_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

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
  uiItemO(col, NULL, ICON_NONE, "OBJECT_OT_lineart_bake_strokes");
  uiItemO(col, NULL, ICON_NONE, "OBJECT_OT_lineart_bake_strokes_all");

  col = uiLayoutColumn(layout, false);
  uiItemO(col, NULL, ICON_NONE, "OBJECT_OT_lineart_clear");
  uiItemO(col, NULL, ICON_NONE, "OBJECT_OT_lineart_clear_all");
}

static void composition_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool show_in_front = RNA_boolean_get(&ob_ptr, "show_in_front");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "overscan", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_image_boundary_trimming", 0, NULL, ICON_NONE);

  if (show_in_front) {
    uiItemL(layout, TIP_("Object is shown in front"), ICON_ERROR);
  }

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, !show_in_front);

  uiItemR(col, ptr, "stroke_depth_offset", UI_ITEM_R_SLIDER, IFACE_("Depth Offset"), ICON_NONE);
  uiItemR(
      col, ptr, "use_offset_towards_custom_camera", 0, IFACE_("Towards Custom Camera"), ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Lineart, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "edge_types", "Edge Types", NULL, edge_types_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "light_reference",
                                     "Light Reference",
                                     NULL,
                                     options_light_reference_draw,
                                     panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "geometry", "Geometry Processing", NULL, options_panel_draw, panel_type);
  PanelType *occlusion_panel = gpencil_modifier_subpanel_register(
      region_type, "occlusion", "Occlusion", NULL, occlusion_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "material_mask",
                                     "",
                                     material_mask_panel_draw_header,
                                     material_mask_panel_draw,
                                     occlusion_panel);
  gpencil_modifier_subpanel_register(
      region_type, "intersection", "Intersection", NULL, intersection_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "face_mark", "", face_mark_panel_draw_header, face_mark_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "chaining", "Chaining", NULL, chaining_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "vgroup", "Vertex Weight Transfer", NULL, vgroup_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "composition", "Composition", NULL, composition_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "bake", "Bake", NULL, bake_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Lineart = {
    /*name*/ "Line Art",
    /*structName*/ "LineartGpencilModifierData",
    /*structSize*/ sizeof(LineartGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ NULL,
    /*generateStrokes*/ generateStrokes,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
