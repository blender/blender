/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void initData(GpencilModifierData *md)
{
  OutlineGpencilModifierData *gpmd = (OutlineGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(OutlineGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void free_old_strokes(Depsgraph *depsgraph, Object *ob, bGPdata *gpd)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  /* Free old strokes. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_TAG) {
        BLI_remlink(&gpf->strokes, gps);
        BKE_gpencil_free_stroke(gps);
      }
    }
  }
}

static void convert_stroke(GpencilModifierData *md,
                           Object *ob,
                           bGPDlayer *gpl,
                           bGPDframe *gpf,
                           bGPDstroke *gps,
                           float viewmat[4][4],
                           float diff_mat[4][4])
{
  OutlineGpencilModifierData *mmd = (OutlineGpencilModifierData *)md;
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool keep = (mmd->flag & GP_OUTLINE_KEEP_SHAPE) != 0;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_OUTLINE_INVERT_LAYER,
                                      mmd->flag & GP_OUTLINE_INVERT_PASS,
                                      mmd->flag & GP_OUTLINE_INVERT_LAYERPASS,
                                      mmd->flag & GP_OUTLINE_INVERT_MATERIAL))
  {
    return;
  }

  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
  const bool is_stroke = (gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0;
  /* Only strokes type, no fill strokes. */
  if (!is_stroke) {
    return;
  }

  /* Duplicate the stroke to apply any layer thickness change. */
  bGPDstroke *gps_duplicate = BKE_gpencil_stroke_duplicate(gps, true, false);

  /* Apply layer thickness change. */
  gps_duplicate->thickness += gpl->line_change;
  /* Apply object scale to thickness. */
  gps_duplicate->thickness *= mat4_to_scale(ob->object_to_world);
  CLAMP_MIN(gps_duplicate->thickness, 1.0f);

  /* Stroke. */
  const float ovr_thickness = keep ? mmd->thickness : 0.0f;
  bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
      viewmat, gpd, gpl, gps_duplicate, mmd->subdiv, diff_mat, ovr_thickness);
  gps_perimeter->flag &= ~GP_STROKE_SELECT;
  gps_perimeter->runtime.gps_orig = gps->runtime.gps_orig;

  /* Assign material. */
  if (mmd->outline_material) {
    Material *ma = mmd->outline_material;
    int mat_idx = BKE_gpencil_material_find_index_by_name_prefix(ob, ma->id.name + 2);
    if (mat_idx > -1) {
      gps_perimeter->mat_nr = mat_idx;
    }
    else {
      gps_perimeter->mat_nr = gps->mat_nr;
    }
  }
  else {
    gps_perimeter->mat_nr = gps->mat_nr;
  }

  /* Sample stroke. */
  if (mmd->sample_length > 0.0f) {
    BKE_gpencil_stroke_sample(gpd, gps_perimeter, mmd->sample_length, false, 0);
  }
  /* Set stroke thickness. */
  gps_perimeter->thickness = mmd->thickness;

  /* Set pressure constant. */
  int orig_idx = -1;
  float min_distance = FLT_MAX;
  bGPDspoint *pt;
  for (int i = 0; i < gps_perimeter->totpoints; i++) {
    pt = &gps_perimeter->points[i];
    pt->pressure = 1.0f;
    pt->runtime.pt_orig = NULL;
    /* If any target object is defined, find the nearest point. */
    if (mmd->object) {
      float wpt[3];
      mul_v3_m4v3(wpt, diff_mat, &pt->x);
      float dist = len_squared_v3v3(wpt, mmd->object->loc);
      if (dist < min_distance) {
        min_distance = dist;
        orig_idx = i;
      }
    }
  }

  if (orig_idx > 0) {
    BKE_gpencil_stroke_start_set(gps_perimeter, orig_idx);
    BKE_gpencil_stroke_geometry_update(gpd, gps_perimeter);
  }

  /* Add perimeter stroke to frame. */
  BLI_insertlinkafter(&gpf->strokes, gps, gps_perimeter);

  /* Free Temp stroke. */
  BKE_gpencil_free_stroke(gps_duplicate);

  /* Tag original stroke to be removed. */
  gps->flag |= GP_STROKE_TAG;
}

static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  /* Calc camera view matrix. */
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  /* Ensure the camera is the right one. */
  BKE_scene_camera_switch_update(scene);

  if (!scene->camera) {
    return;
  }
  Object *cam_ob = scene->camera;
  float viewmat[4][4];
  invert_m4_m4(viewmat, cam_ob->object_to_world);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    /* Prepare transform matrix. */
    float diff_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);

    LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
      gps->flag &= ~GP_STROKE_TAG;
      convert_stroke(md, ob, gpl, gpf, gps, viewmat, diff_mat);
    }
  }

  /* Delete original strokes. */
  free_old_strokes(depsgraph, ob, gpd);
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = ob->data;
  int oldframe = (int)DEG_get_ctime(depsgraph);

  /* Calc camera view matrix. */
  if (!scene->camera) {
    return;
  }
  Object *cam_ob = scene->camera;
  float viewmat[4][4];

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      scene->r.cfra = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph);
      /* Ensure the camera is the right one. */
      BKE_scene_camera_switch_update(scene);
      invert_m4_m4(viewmat, cam_ob->object_to_world);

      /* Prepare transform matrix. */
      float diff_mat[4][4];
      BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);

      /* Compute all strokes of this frame. */
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        convert_stroke(md, ob, gpl, gpf, gps, viewmat, diff_mat);
      }
    }
  }

  /* Delete original strokes. */
  free_old_strokes(depsgraph, ob, gpd);

  /* Return frame state and DB to original state. */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  OutlineGpencilModifierData *mmd = (OutlineGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&mmd->outline_material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int UNUSED(mode))
{
  OutlineGpencilModifierData *lmd = (OutlineGpencilModifierData *)md;
  if (ctx->scene->camera) {
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_TRANSFORM, "Outline Modifier");
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_PARAMETERS, "Outline Modifier");
  }
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Outline Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Outline Modifier");
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "thickness", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_keep_shape", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "subdivision", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "sample_length", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "outline_material", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "object", 0, NULL, ICON_NONE);

  Scene *scene = CTX_data_scene(C);
  if (scene->camera == NULL) {
    uiItemL(layout, IFACE_("Outline requires an active camera"), ICON_ERROR);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Outline, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Outline = {
    /*name*/ N_("Outline"),
    /*structName*/ "OutlineGpencilModifierData",
    /*structSize*/ sizeof(OutlineGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ NULL,
    /*generateStrokes*/ generateStrokes,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
