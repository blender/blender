/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void initData(GpencilModifierData *md)
{
  MultiplyGpencilModifierData *gpmd = (MultiplyGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(MultiplyGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void minter_v3_v3v3v3_ref(
    float *result, float *prev, float *curr, float *next, float *stroke_normal)
{
  float vec[3], inter1[3], inter2[3];
  ARRAY_SET_ITEMS(inter1, 0.0f, 0.0f, 0.0f);
  ARRAY_SET_ITEMS(inter2, 0.0f, 0.0f, 0.0f);

  float minter[3];
  if (prev) {
    sub_v3_v3v3(vec, curr, prev);
    cross_v3_v3v3(inter1, stroke_normal, vec);
  }
  if (next) {
    sub_v3_v3v3(vec, next, curr);
    cross_v3_v3v3(inter2, stroke_normal, vec);
  }
  if (!prev) {
    normalize_v3(inter2);
    copy_v3_v3(result, inter2);
    return;
  }
  if (!next) {
    normalize_v3(inter1);
    copy_v3_v3(result, inter1);
    return;
  }
  interp_v3_v3v3(minter, inter1, inter2, 0.5);
  normalize_v3(minter);
  copy_v3_v3(result, minter);
}

static void duplicateStroke(Object *ob,
                            bGPDstroke *gps,
                            int count,
                            float dist,
                            float offset,
                            ListBase *results,
                            int fading,
                            float fading_center,
                            float fading_thickness,
                            float fading_opacity)
{
  bGPdata *gpd = ob->data;
  int i;
  bGPDstroke *new_gps = NULL;
  float stroke_normal[3];
  bGPDspoint *pt;
  float thickness_factor;
  float opacity_factor;

  /* Apply object scale to offset distance. */
  offset *= mat4_to_scale(ob->object_to_world);

  BKE_gpencil_stroke_normal(gps, stroke_normal);
  if (len_v3(stroke_normal) < FLT_EPSILON) {
    add_v3_fl(stroke_normal, 1);
    normalize_v3(stroke_normal);
  }

  float *t1_array = MEM_callocN(sizeof(float[3]) * gps->totpoints,
                                "duplicate_temp_result_array_1");
  float *t2_array = MEM_callocN(sizeof(float[3]) * gps->totpoints,
                                "duplicate_temp_result_array_2");

  pt = gps->points;

  for (int j = 0; j < gps->totpoints; j++) {
    float minter[3];
    if (j == 0) {
      minter_v3_v3v3v3_ref(minter, NULL, &pt[j].x, &pt[j + 1].x, stroke_normal);
    }
    else if (j == gps->totpoints - 1) {
      minter_v3_v3v3v3_ref(minter, &pt[j - 1].x, &pt[j].x, NULL, stroke_normal);
    }
    else {
      minter_v3_v3v3v3_ref(minter, &pt[j - 1].x, &pt[j].x, &pt[j + 1].x, stroke_normal);
    }
    mul_v3_fl(minter, dist);
    add_v3_v3v3(&t1_array[j * 3], &pt[j].x, minter);
    sub_v3_v3v3(&t2_array[j * 3], &pt[j].x, minter);
  }

  /* This ensures the original stroke is the last one
   * to be processed, since we duplicate its data. */
  for (i = count - 1; i >= 0; i--) {
    if (i != 0) {
      new_gps = BKE_gpencil_stroke_duplicate(gps, true, true);
      BLI_addtail(results, new_gps);
    }
    else {
      new_gps = gps;
    }

    pt = new_gps->points;

    float offset_fac = (count == 1) ? 0.5f : (i / (float)(count - 1));

    if (fading) {
      thickness_factor = interpf(1.0f - fading_thickness, 1.0f, fabsf(offset_fac - fading_center));
      opacity_factor = interpf(1.0f - fading_opacity, 1.0f, fabsf(offset_fac - fading_center));
    }

    for (int j = 0; j < new_gps->totpoints; j++) {
      float fac = interpf(1 + offset, offset, offset_fac);
      interp_v3_v3v3(&pt[j].x, &t1_array[j * 3], &t2_array[j * 3], fac);
      if (fading) {
        pt[j].pressure = gps->points[j].pressure * thickness_factor;
        pt[j].strength = gps->points[j].strength * opacity_factor;
      }
    }
  }
  /* Calc geometry data. */
  if (new_gps != NULL) {
    BKE_gpencil_stroke_geometry_update(gpd, new_gps);
  }
  MEM_freeN(t1_array);
  MEM_freeN(t2_array);
}

/* -------------------------------- */
static void generate_geometry(GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
  MultiplyGpencilModifierData *mmd = (MultiplyGpencilModifierData *)md;
  bGPDstroke *gps;
  ListBase duplicates = {0};
  for (gps = gpf->strokes.first; gps; gps = gps->next) {
    if (!is_stroke_affected_by_modifier(ob,
                                        mmd->layername,
                                        mmd->material,
                                        mmd->pass_index,
                                        mmd->layer_pass,
                                        1,
                                        gpl,
                                        gps,
                                        mmd->flag & GP_MIRROR_INVERT_LAYER,
                                        mmd->flag & GP_MIRROR_INVERT_PASS,
                                        mmd->flag & GP_MIRROR_INVERT_LAYERPASS,
                                        mmd->flag & GP_MIRROR_INVERT_MATERIAL))
    {
      continue;
    }
    if (mmd->duplications > 0) {
      duplicateStroke(ob,
                      gps,
                      mmd->duplications,
                      mmd->distance,
                      mmd->offset,
                      &duplicates,
                      mmd->flags & GP_MULTIPLY_ENABLE_FADING,
                      mmd->fading_center,
                      mmd->fading_thickness,
                      mmd->fading_opacity);
    }
  }
  if (!BLI_listbase_is_empty(&duplicates)) {
    BLI_movelisttolist(&gpf->strokes, &duplicates);
  }
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *UNUSED(depsgraph),
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      generate_geometry(md, ob, gpl, gpf);
    }
  }
}

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    generate_geometry(md, ob, gpl, gpf);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MultiplyGpencilModifierData *mmd = (MultiplyGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "duplicates", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(layout, RNA_int_get(ptr, "duplicates") > 0);
  uiItemR(col, ptr, "distance", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "offset", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void fade_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_fade", 0, NULL, ICON_NONE);
}

static void fade_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_fade"));

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "fading_center", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "fading_thickness", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "fading_opacity", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Multiply, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "fade", "", fade_header_draw, fade_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Multiply = {
    /*name*/ N_("MultipleStrokes"),
    /*structName*/ "MultiplyGpencilModifierData",
    /*structSize*/ sizeof(MultiplyGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ 0,

    /*copyData*/ copyData,

    /*deformStroke*/ NULL,
    /*generateStrokes*/ generateStrokes,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
