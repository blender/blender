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
 * along with this program; if not, write to the Free Software  Foundation,
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

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_colortools.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_material.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
  TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->factor = 0.5f;
  gpmd->layername[0] = '\0';
  gpmd->materialname[0] = '\0';
  ARRAY_SET_ITEMS(gpmd->rgb, 1.0f, 1.0f, 1.0f);
  gpmd->modify_color = GP_MODIFY_COLOR_BOTH;

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

  if (tgmd->curve_intensity != NULL) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = NULL;
  }

  BKE_gpencil_modifier_copyData_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* tint strokes */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
  const bool use_curve = (mmd->flag & GP_TINT_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->materialname,
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

  /* if factor > 1.0, affect the strength of the stroke */
  if (mmd->factor > 1.0f) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->strength += mmd->factor - 1.0f;
      CLAMP(pt->strength, 0.0f, 1.0f);
    }
  }

  /* Apply to Vertex Color. */
  float mixfac = mmd->factor;

  CLAMP(mixfac, 0.0, 1.0f);
  /* Fill */
  if (mmd->modify_color != GP_MODIFY_COLOR_STROKE) {
    /* If not using Vertex Color, use the material color. */
    if ((gp_style != NULL) && (gps->vert_color_fill[3] == 0.0f) &&
        (gp_style->fill_rgba[3] > 0.0f)) {
      copy_v4_v4(gps->vert_color_fill, gp_style->fill_rgba);
      gps->vert_color_fill[3] = 1.0f;
    }

    interp_v3_v3v3(gps->vert_color_fill, gps->vert_color_fill, mmd->rgb, mixfac);
  }

  /* Stroke */
  if (mmd->modify_color != GP_MODIFY_COLOR_FILL) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      /* If not using Vertex Color, use the material color. */
      if ((gp_style != NULL) && (pt->vert_color[3] == 0.0f) && (gp_style->stroke_rgba[3] > 0.0f)) {
        copy_v4_v4(pt->vert_color, gp_style->stroke_rgba);
        pt->vert_color[3] = 1.0f;
      }

      /* Custom curve to modulate value. */
      float mixvalue = mixfac;
      if (use_curve) {
        float value = (float)i / (gps->totpoints - 1);
        mixvalue *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
      }

      interp_v3_v3v3(pt->vert_color, pt->vert_color, mmd->rgb, mixvalue);
    }
  }
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }
}

static void freeData(GpencilModifierData *md)
{
  TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
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
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
};
