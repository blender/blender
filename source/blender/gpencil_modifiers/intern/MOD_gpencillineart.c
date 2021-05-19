/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "lineart/MOD_lineart.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_modifier.h"
#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"

#include "WM_api.h"
#include "WM_types.h"

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
      lmd->render_buffer,
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
      lmd->transparency_flags,
      lmd->transparency_mask,
      lmd->thickness,
      lmd->opacity,
      lmd->source_vertex_group,
      lmd->vgname,
      lmd->flags);
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

  MOD_lineart_compute_feature_lines(depsgraph, lmd);

  generate_strokes_actual(md, depsgraph, ob, gpl, gpf);

  MOD_lineart_destroy_render_data(lmd);

  WM_main_add_notifier(NA_EDITED | NC_GPENCIL, NULL);
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

  MOD_lineart_compute_feature_lines(depsgraph, lmd);

  generate_strokes_actual(md, depsgraph, ob, gpl, gpf);

  MOD_lineart_destroy_render_data(lmd);
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  return isModifierDisabled(md);
}

static void add_this_collection(Collection *c,
                                const ModifierUpdateDepsgraphContext *ctx,
                                const int mode)
{
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (c, ob, mode) {
    if (ELEM(ob->type, OB_MESH, OB_MBALL, OB_CURVE, OB_SURF, OB_FONT)) {
      if (ob->lineart.usage != OBJECT_LRT_EXCLUDE) {
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
  if (lmd->source_type == LRT_SOURCE_OBJECT && lmd->source_object) {
    DEG_add_object_relation(
        ctx->node, lmd->source_object, DEG_OB_COMP_GEOMETRY, "Line Art Modifier");
    DEG_add_object_relation(
        ctx->node, lmd->source_object, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
  }
  else {
    add_this_collection(ctx->scene->master_collection, ctx, mode);
  }
  DEG_add_object_relation(
      ctx->node, ctx->scene->camera, DEG_OB_COMP_TRANSFORM, "Line Art Modifier");
  DEG_add_object_relation(
      ctx->node, ctx->scene->camera, DEG_OB_COMP_PARAMETERS, "Line Art Modifier");
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  walk(userData, ob, (ID **)&lmd->target_material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&lmd->source_collection, IDWALK_CB_NOP);

  walk(userData, ob, (ID **)&lmd->source_object, IDWALK_CB_NOP);
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

  uiItemR(layout, ptr, "source_type", 0, NULL, ICON_NONE);

  if (source_type == LRT_SOURCE_OBJECT) {
    uiItemR(layout, ptr, "source_object", 0, NULL, ICON_OBJECT_DATA);
  }
  else if (source_type == LRT_SOURCE_COLLECTION) {
    uiItemR(layout, ptr, "source_collection", 0, NULL, ICON_OUTLINER_COLLECTION);
  }
  else {
    /* Source is Scene. */
  }

  uiLayout *col = uiLayoutColumnWithHeading(layout, true, IFACE_("Edge Types"));

  uiItemR(col, ptr, "use_contour", 0, IFACE_("Contour"), ICON_NONE);
  uiItemR(col, ptr, "use_material", 0, IFACE_("Material Borders"), ICON_NONE);
  uiItemR(col, ptr, "use_edge_mark", 0, IFACE_("Edge Marks"), ICON_NONE);
  uiItemR(col, ptr, "use_intersection", 0, IFACE_("Intersections"), ICON_NONE);
  uiItemR(col, ptr, "use_crease", 0, IFACE_("Crease"), ICON_NONE);
  uiLayout *sub = uiLayoutRow(col, true);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_crease"));
  uiLayoutSetPropSep(sub, true);

  uiItemR(sub, ptr, "crease_threshold", UI_ITEM_R_SLIDER, " ", ICON_NONE);

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
  uiItemPointerR(row,
                 ptr,
                 "target_material",
                 &obj_data_ptr,
                 "materials",
                 NULL,
                 material_valid ? ICON_SHADING_TEXTURE : ICON_ERROR);

  uiItemR(layout, ptr, "use_remove_doubles", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_edge_overlap", 0, IFACE_("Overlapping Edges As Contour"), ICON_NONE);
  uiItemR(layout, ptr, "use_object_instances", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_clip_plane_boundaries", 0, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void style_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  uiItemR(layout, ptr, "thickness", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemR(layout, ptr, "opacity", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void occlusion_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  const bool use_multiple_levels = RNA_boolean_get(ptr, "use_multiple_levels");

  uiItemR(layout, ptr, "use_multiple_levels", 0, IFACE_("Range"), ICON_NONE);

  if (use_multiple_levels) {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "level_start", 0, NULL, ICON_NONE);
    uiItemR(col, ptr, "level_end", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "level_start", 0, IFACE_("Level"), ICON_NONE);
  }
}

static void transparency_panel_draw_header(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  uiLayoutSetEnabled(layout, !is_baked);

  uiItemR(layout, ptr, "use_transparency", 0, IFACE_("Transparency"), ICON_NONE);
}

static void transparency_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");
  uiLayoutSetEnabled(layout, !is_baked);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_transparency"));

  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetPropDecorate(row, false);
  uiLayout *sub = uiLayoutRow(row, true);
  char text[2] = "0";

  PropertyRNA *prop = RNA_struct_find_property(ptr, "use_transparency_mask");
  for (int i = 0; i < 8; i++, text[0]++) {
    uiItemFullR(sub, ptr, prop, i, 0, UI_ITEM_R_TOGGLE, text, ICON_NONE);
  }
  uiItemL(row, "", ICON_BLANK1); /* Space for decorator. */

  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_transparency_match", 0, IFACE_("Match All Masks"), ICON_NONE);
}

static void chaining_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

  uiLayout *col = uiLayoutColumnWithHeading(layout, true, IFACE_("Chain"));
  uiItemR(col, ptr, "use_fuzzy_intersections", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_fuzzy_all", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "chaining_image_threshold", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "split_angle", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void vgroup_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayout *layout = panel->layout;

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetEnabled(layout, !is_baked);

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

static void baking_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const bool is_baked = RNA_boolean_get(ptr, "is_baked");

  uiLayoutSetPropSep(layout, true);

  if (is_baked) {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, false);
    uiItemL(col, IFACE_("Modifier has baked data"), ICON_NONE);
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

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Lineart, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "style", "Style", NULL, style_panel_draw, panel_type);
  PanelType *occlusion_panel = gpencil_modifier_subpanel_register(
      region_type, "occlusion", "Occlusion", NULL, occlusion_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "transparency",
                                     "",
                                     transparency_panel_draw_header,
                                     transparency_panel_draw,
                                     occlusion_panel);
  gpencil_modifier_subpanel_register(
      region_type, "chaining", "Chaining", NULL, chaining_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "vgroup", "Vertex Weight Transfer", NULL, vgroup_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "baking", "Baking", NULL, baking_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Lineart = {
    /* name. */ "Line Art",
    /* structName. */ "LineartGpencilModifierData",
    /* structSize. */ sizeof(LineartGpencilModifierData),
    /* type. */ eGpencilModifierTypeType_Gpencil,
    /* flags. */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData. */ copyData,

    /* deformStroke. */ NULL,
    /* generateStrokes. */ generateStrokes,
    /* bakeModifier. */ bakeModifier,
    /* remapTime. */ NULL,

    /* initData. */ initData,
    /* freeData. */ NULL,
    /* isDisabled. */ isDisabled,
    /* updateDepsgraph. */ updateDepsgraph,
    /* dependsOnTime. */ NULL,
    /* foreachIDLink. */ foreachIDLink,
    /* foreachTexLink. */ NULL,
    /* panelRegister. */ panelRegister,
};
