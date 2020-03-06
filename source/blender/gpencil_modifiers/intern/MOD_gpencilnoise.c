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

#include "BLI_math_vector.h"
#include "BLI_hash.h"
#include "BLI_rand.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
  NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->flag |= GP_NOISE_MOD_LOCATION;
  gpmd->flag |= GP_NOISE_FULL_STROKE;
  gpmd->flag |= GP_NOISE_USE_RANDOM;
  gpmd->factor = 0.5f;
  gpmd->layername[0] = '\0';
  gpmd->materialname[0] = '\0';
  gpmd->vgname[0] = '\0';
  gpmd->step = 1;
  gpmd->seed = 0;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copyData_generic(md, target);
}

static bool dependsOnTime(GpencilModifierData *md)
{
  NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;
  return (mmd->flag & GP_NOISE_USE_RANDOM) != 0;
}

/* aply noise effect based on stroke direction */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *depsgraph,
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;
  bGPDspoint *pt0, *pt1;
  MDeformVert *dvert = NULL;
  float shift, vran, vdir;
  float normal[3];
  float vec1[3], vec2[3];
  int sc_frame = 0;
  int stroke_seed = 0;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const float unit_v3[3] = {1.0f, 1.0f, 1.0f};

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->materialname,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_NOISE_INVERT_LAYER,
                                      mmd->flag & GP_NOISE_INVERT_PASS,
                                      mmd->flag & GP_NOISE_INVERT_LAYERPASS,
                                      mmd->flag & GP_NOISE_INVERT_MATERIAL)) {
    return;
  }

  sc_frame = (int)DEG_get_ctime(depsgraph);

  zero_v3(vec2);

  /* calculate stroke normal*/
  if (gps->totpoints > 2) {
    BKE_gpencil_stroke_normal(gps, normal);
  }
  else {
    copy_v3_v3(normal, unit_v3);
  }

  /* move points */
  for (int i = 0; i < gps->totpoints; i++) {
    if (((i == 0) || (i == gps->totpoints - 1)) && ((mmd->flag & GP_NOISE_MOVE_EXTREME) == 0)) {
      continue;
    }

    /* first point is special */
    if (i == 0) {
      if (gps->dvert) {
        dvert = &gps->dvert[0];
      }
      pt0 = (gps->totpoints > 1) ? &gps->points[1] : &gps->points[0];
      pt1 = &gps->points[0];
    }
    else {
      int prev_idx = i - 1;
      CLAMP_MIN(prev_idx, 0);
      if (gps->dvert) {
        dvert = &gps->dvert[prev_idx];
      }
      pt0 = &gps->points[prev_idx];
      pt1 = &gps->points[i];
    }

    /* verify vertex group */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_NOISE_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    /* initial vector (p0 -> p1) */
    if (i == 0) {
      sub_v3_v3v3(vec1, &pt0->x, &pt1->x);
    }
    else {
      sub_v3_v3v3(vec1, &pt1->x, &pt0->x);
    }
    vran = len_v3(vec1);
    /* Vector orthogonal to normal. */
    cross_v3_v3v3(vec2, vec1, normal);
    normalize_v3(vec2);
    /* Use random noise */
    if (mmd->flag & GP_NOISE_USE_RANDOM) {
      stroke_seed = BLI_hash_int_2d((sc_frame / mmd->step) + gps->totpoints, mmd->seed + 1);
      vran = BLI_hash_frand(stroke_seed);
      if (mmd->flag & GP_NOISE_FULL_STROKE) {
        vdir = BLI_hash_frand(stroke_seed + 3);
      }
      else {
        int f = (BLI_hash_frand(stroke_seed + 3) * 10.0f) + i;
        vdir = f % 2;
      }
    }
    else {
      vran = 1.0f;
      if (mmd->flag & GP_NOISE_FULL_STROKE) {
        vdir = gps->totpoints % 2;
      }
      else {
        vdir = i % 2;
      }
    }

    /* if vec2 is zero, set to something */
    if (gps->totpoints < 3) {
      if ((vec2[0] == 0.0f) && (vec2[1] == 0.0f) && (vec2[2] == 0.0f)) {
        copy_v3_v3(vec2, unit_v3);
      }
    }

    /* apply randomness to location of the point */
    if (mmd->flag & GP_NOISE_MOD_LOCATION) {
      /* factor is too sensitive, so need divide */
      shift = ((vran * mmd->factor) / 1000.0f) * weight;
      if (vdir > 0.5f) {
        mul_v3_fl(vec2, shift);
      }
      else {
        mul_v3_fl(vec2, shift * -1.0f);
      }
      add_v3_v3(&pt1->x, vec2);
    }

    /* apply randomness to thickness */
    if (mmd->flag & GP_NOISE_MOD_THICKNESS) {
      if (vdir > 0.5f) {
        pt1->pressure -= pt1->pressure * vran * mmd->factor * weight;
      }
      else {
        pt1->pressure += pt1->pressure * vran * mmd->factor * weight;
      }
      CLAMP_MIN(pt1->pressure, GPENCIL_STRENGTH_MIN);
    }

    /* apply randomness to color strength */
    if (mmd->flag & GP_NOISE_MOD_STRENGTH) {
      if (vdir > 0.5f) {
        pt1->strength -= pt1->strength * vran * mmd->factor * weight;
      }
      else {
        pt1->strength += pt1->strength * vran * mmd->factor * weight;
      }
      CLAMP_MIN(pt1->strength, GPENCIL_STRENGTH_MIN);
    }
    /* apply randomness to uv rotation */
    if (mmd->flag & GP_NOISE_MOD_UV) {
      if (vdir > 0.5f) {
        pt1->uv_rot -= pt1->uv_rot * vran * mmd->factor * weight;
      }
      else {
        pt1->uv_rot += pt1->uv_rot * vran * mmd->factor * weight;
      }
      CLAMP(pt1->uv_rot, -M_PI_2, M_PI_2);
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

GpencilModifierTypeInfo modifierType_Gpencil_Noise = {
    /* name */ "Noise",
    /* structName */ "NoiseGpencilModifierData",
    /* structSize */ sizeof(NoiseGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ dependsOnTime,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
};
