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

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
  TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->material = NULL;
  gpmd->object = NULL;
  gpmd->radius = 1.0f;
  gpmd->factor = 0.5f;
  ARRAY_SET_ITEMS(gpmd->rgb, 1.0f, 1.0f, 1.0f);
  gpmd->mode = GPPAINT_MODE_BOTH;

  /* Add default color ramp. */
  gpmd->colorband = BKE_colorband_add(false);
  if (gpmd->colorband) {
    BKE_colorband_init(gpmd->colorband, true);
    CBData *ramp = gpmd->colorband->data;
    ramp[0].r = ramp[0].g = ramp[0].b = ramp[0].a = 1.0f;
    ramp[0].pos = 0.0f;
    ramp[1].r = ramp[1].g = ramp[1].b = 0.0f;
    ramp[1].a = 1.0f;
    ramp[1].pos = 1.0f;

    gpmd->colorband->tot = 2;
  }

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  if (gpmd->curve_intensity) {
    CurveMapping *curve = gpmd->curve_intensity;
    BKE_curvemapping_initialize(curve);
  }
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  TintGpencilModifierData *gmd = (TintGpencilModifierData *)md;
  TintGpencilModifierData *tgmd = (TintGpencilModifierData *)target;

  MEM_SAFE_FREE(tgmd->colorband);

  if (tgmd->curve_intensity != NULL) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = NULL;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  if (gmd->colorband) {
    tgmd->colorband = MEM_dupallocN(gmd->colorband);
  }

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* deform stroke */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
  if ((mmd->type == GP_TINT_GRADIENT) && (!mmd->object)) {
    return;
  }

  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const bool use_curve = (mmd->flag & GP_TINT_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_TINT_INVERT_LAYER,
                                      mmd->flag & GP_TINT_INVERT_PASS,
                                      mmd->flag & GP_TINT_INVERT_LAYERPASS,
                                      mmd->flag & GP_TINT_INVERT_MATERIAL)) {
    return;
  }
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
  const bool is_gradient = (mmd->type == GP_TINT_GRADIENT);

  /* If factor > 1.0, affect the strength of the stroke. */
  if (mmd->factor > 1.0f) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->strength += mmd->factor - 1.0f;
      CLAMP(pt->strength, 0.0f, 1.0f);
    }
  }

  float coba_res[4];
  float matrix[4][4];
  if (is_gradient) {
    mul_m4_m4m4(matrix, mmd->object->imat, ob->obmat);
  }

  /* loop points and apply color. */
  bool fill_done = false;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    if (!fill_done) {
      /* Apply to fill. */
      if (mmd->mode != GPPAINT_MODE_STROKE) {

        /* If not using Vertex Color, use the material color. */
        if ((gp_style != NULL) && (gps->vert_color_fill[3] == 0.0f) &&
            (gp_style->fill_rgba[3] > 0.0f)) {
          copy_v4_v4(gps->vert_color_fill, gp_style->fill_rgba);
          gps->vert_color_fill[3] = 1.0f;
        }

        if (is_gradient) {
          float center[3];
          add_v3_v3v3(center, gps->boundbox_min, gps->boundbox_max);
          mul_v3_fl(center, 0.5f);
          float pt_loc[3];
          mul_v3_m4v3(pt_loc, matrix, &pt->x);
          float dist = len_v3(pt_loc);
          float mix_factor = clamp_f(dist / mmd->radius, 0.0f, 1.0f);

          BKE_colorband_evaluate(mmd->colorband, mix_factor, coba_res);
          interp_v3_v3v3(gps->vert_color_fill, gps->vert_color_fill, coba_res, mmd->factor);
          gps->vert_color_fill[3] = clamp_f(mmd->factor, 0.0f, 1.0f);
        }
        else {
          interp_v3_v3v3(gps->vert_color_fill,
                         gps->vert_color_fill,
                         mmd->rgb,
                         clamp_f(mmd->factor, 0.0f, 1.0f));
        }
        /* If no stroke, cancel loop. */
        if (mmd->mode != GPPAINT_MODE_BOTH) {
          break;
        }
      }

      fill_done = true;
    }

    /* Verify vertex group. */
    if (mmd->mode != GPPAINT_MODE_FILL) {
      float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_TINT_INVERT_VGROUP) != 0, def_nr);
      if (weight < 0.0f) {
        continue;
      }
      /* Custom curve to modulate value. */
      if (use_curve) {
        float value = (float)i / (gps->totpoints - 1);
        weight *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
      }

      /* If not using Vertex Color, use the material color. */
      if ((gp_style != NULL) && (pt->vert_color[3] == 0.0f) && (gp_style->stroke_rgba[3] > 0.0f)) {
        copy_v4_v4(pt->vert_color, gp_style->stroke_rgba);
        pt->vert_color[3] = 1.0f;
      }

      if (is_gradient) {
        /* Calc world position of point. */
        float pt_loc[3];
        mul_v3_m4v3(pt_loc, matrix, &pt->x);
        float dist = len_v3(pt_loc);

        /* Calc the factor using the distance and get mix color. */
        float mix_factor = clamp_f(dist / mmd->radius, 0.0f, 1.0f);
        BKE_colorband_evaluate(mmd->colorband, mix_factor, coba_res);

        interp_v3_v3v3(pt->vert_color,
                       pt->vert_color,
                       coba_res,
                       clamp_f(mmd->factor, 0.0f, 1.0f) * weight * coba_res[3]);
      }
      else {
        interp_v3_v3v3(
            pt->vert_color, pt->vert_color, mmd->rgb, clamp_f(mmd->factor * weight, 0.0, 1.0f));
      }
    }
  }
}

/* FIXME: Ideally we be doing this on a copy of the main depsgraph
 * (i.e. one where we don't have to worry about restoring state)
 */
static void bakeModifier(Main *bmain, Depsgraph *depsgraph, GpencilModifierData *md, Object *ob)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = ob->data;
  int oldframe = (int)DEG_get_ctime(depsgraph);

  if (mmd->object == NULL) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* apply effects on this frame
       * NOTE: this assumes that we don't want animation on non-keyframed frames
       */
      CFRA = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph, bmain);

      /* compute effects on this frame */
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }

  /* return frame state and DB to original state */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static void freeData(GpencilModifierData *md)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
  if (mmd->colorband) {
    MEM_freeN(mmd->colorband);
    mmd->colorband = NULL;
  }
  if (mmd->curve_intensity) {
    BKE_curvemapping_free(mmd->curve_intensity);
  }
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
  if (mmd->type == GP_TINT_UNIFORM) {
    return false;
  }

  return !mmd->object;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  TintGpencilModifierData *lmd = (TintGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Vertexcolor Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Vertexcolor Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Vertexcolor Modifier");
}

static void foreachObjectLink(GpencilModifierData *md,
                              Object *ob,
                              ObjectWalkFunc walk,
                              void *userData)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int tint_type = RNA_enum_get(&ptr, "tint_type");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "vertex_mode", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "factor", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "tint_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  if (tint_type == GP_TINT_UNIFORM) {
    uiItemR(layout, &ptr, "color", 0, NULL, ICON_NONE);
  }
  else {
    col = uiLayoutColumn(layout, false);
    uiLayoutSetPropSep(col, false);
    uiTemplateColorRamp(col, &ptr, "colors", true);
    uiItemS(layout);
    uiItemR(layout, &ptr, "object", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "radius", 0, NULL, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Tint, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "curve",
                                     "",
                                     gpencil_modifier_curve_header_draw,
                                     gpencil_modifier_curve_panel_draw,
                                     mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Tint = {
    /* name */ "Tint",
    /* structName */ "TintGpencilModifierData",
    /* structSize */ sizeof(TintGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
