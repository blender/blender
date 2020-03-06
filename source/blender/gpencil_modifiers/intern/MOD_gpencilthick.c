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

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
  ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->thickness = 2;
  gpmd->layername[0] = '\0';
  gpmd->materialname[0] = '\0';
  gpmd->vgname[0] = '\0';
  gpmd->curve_thickness = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  if (gpmd->curve_thickness) {
    BKE_curvemapping_initialize(gpmd->curve_thickness);
  }
}

static void freeData(GpencilModifierData *md)
{
  ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

  if (gpmd->curve_thickness) {
    BKE_curvemapping_free(gpmd->curve_thickness);
  }
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  ThickGpencilModifierData *gmd = (ThickGpencilModifierData *)md;
  ThickGpencilModifierData *tgmd = (ThickGpencilModifierData *)target;

  if (tgmd->curve_thickness != NULL) {
    BKE_curvemapping_free(tgmd->curve_thickness);
    tgmd->curve_thickness = NULL;
  }

  BKE_gpencil_modifier_copyData_generic(md, target);

  tgmd->curve_thickness = BKE_curvemapping_copy(gmd->curve_thickness);
}

/* change stroke thickness */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  ThickGpencilModifierData *mmd = (ThickGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->materialname,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_THICK_INVERT_LAYER,
                                      mmd->flag & GP_THICK_INVERT_PASS,
                                      mmd->flag & GP_THICK_INVERT_LAYERPASS,
                                      mmd->flag & GP_THICK_INVERT_MATERIAL)) {
    return;
  }

  /* Check to see if we normalize the whole stroke or only certain points along it. */
  bool gps_has_affected_points = false;
  bool gps_has_unaffected_points = false;

  if (mmd->flag & GP_THICK_NORMALIZE) {
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
      const float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_THICK_INVERT_VGROUP) != 0, def_nr);
      if (weight < 0.0f) {
        gps_has_unaffected_points = true;
      }
      else {
        gps_has_affected_points = true;
      }

      /* If both checks are true, we have what we need so we can stop looking. */
      if (gps_has_affected_points && gps_has_unaffected_points) {
        break;
      }
    }
  }

  /* If we are normalizing and all points of the stroke are affected, it's safe to reset thickness
   */
  if (mmd->flag & GP_THICK_NORMALIZE && gps_has_affected_points && !gps_has_unaffected_points) {
    gps->thickness = mmd->thickness;
  }
  /* Without this check, modifier alters the thickness of strokes which have no points in scope */

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    float curvef = 1.0f;
    /* Verify point is part of vertex group. */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_THICK_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    if (mmd->flag & GP_THICK_NORMALIZE) {
      if (gps_has_unaffected_points) {
        /* Clamp value for very weird situations when stroke thickness can be zero. */
        CLAMP_MIN(gps->thickness, 1);
        /* Calculate pressure value to match the width of strokes with reset thickness and 1.0
         * pressure. */
        pt->pressure = (float)mmd->thickness / (float)gps->thickness;
      }
      else {
        /* Reset point pressure values so only stroke thickness counts. */
        pt->pressure = 1.0f;
      }
    }
    else {
      if ((mmd->flag & GP_THICK_CUSTOM_CURVE) && (mmd->curve_thickness)) {
        /* Normalize value to evaluate curve. */
        float value = (float)i / (gps->totpoints - 1);
        curvef = BKE_curvemapping_evaluateF(mmd->curve_thickness, 0, value);
      }

      pt->pressure += mmd->thickness * weight * curvef;
      CLAMP_MIN(pt->pressure, 0.1f);
    }
  }
}

static void bakeModifier(struct Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
      for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }
}

GpencilModifierTypeInfo modifierType_Gpencil_Thick = {
    /* name */ "Thickness",
    /* structName */ "ThickGpencilModifierData",
    /* structSize */ sizeof(ThickGpencilModifierData),
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
