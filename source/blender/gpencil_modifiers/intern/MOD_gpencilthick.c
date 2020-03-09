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

#include "BLI_listbase.h"
#include "BLI_math.h"
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
  gpmd->thickness_fac = 1.0f;
  gpmd->thickness = 30;
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

  float stroke_thickness_inv = 1.0f / max_ii(gps->thickness, 1);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    /* Verify point is part of vertex group. */
    float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_THICK_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    float curvef = 1.0f;
    if ((mmd->flag & GP_THICK_CUSTOM_CURVE) && (mmd->curve_thickness)) {
      /* Normalize value to evaluate curve. */
      float value = (float)i / (gps->totpoints - 1);
      curvef = BKE_curvemapping_evaluateF(mmd->curve_thickness, 0, value);
    }

    float target;
    if (mmd->flag & GP_THICK_NORMALIZE) {
      target = mmd->thickness * stroke_thickness_inv;
      target *= curvef;
    }
    else {
      target = pt->pressure * mmd->thickness_fac;
      weight *= curvef;
    }

    pt->pressure = interpf(target, pt->pressure, weight);

    CLAMP_MIN(pt->pressure, 0.1f);
  }
}

static void bakeModifier(struct Main *UNUSED(bmain),
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
