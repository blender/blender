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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
  BuildGpencilModifierData *gpmd = (BuildGpencilModifierData *)md;

  /* We deliberately set this range to the half the default
   * frame-range to have an immediate effect to suggest use-cases
   */
  gpmd->start_frame = 1;
  gpmd->end_frame = 125;

  /* Init default length of each build effect - Nothing special */
  gpmd->start_delay = 0.0f;
  gpmd->length = 100.0f;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copyData_generic(md, target);
}

static bool dependsOnTime(GpencilModifierData *UNUSED(md))
{
  return true;
}

/* ******************************************** */
/* Build Modifier - Stroke generation logic
 *
 * There are two modes for how the strokes are sequenced (at a macro-level):
 * - Sequential Mode - Strokes appear/disappear one after the other. Only a single one changes at a
 * time.
 * - Concurrent Mode - Multiple strokes appear/disappear at once.
 *
 * Assumptions:
 * - Stroke points are generally equally spaced. This implies that we can just add/remove points,
 *   without worrying about distances between them / adding extra interpolated points between
 *   an visible point and one about to be added/removed (or any similar tapering effects).
 *
 * - All strokes present are fully visible (i.e. we don't have to ignore any)
 */

/* Remove a particular stroke */
static void clear_stroke(bGPDframe *gpf, bGPDstroke *gps)
{
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);
}

/* Clear all strokes in frame */
static void gpf_clear_all_strokes(bGPDframe *gpf)
{
  bGPDstroke *gps, *gps_next;
  for (gps = gpf->strokes.first; gps; gps = gps_next) {
    gps_next = gps->next;
    clear_stroke(gpf, gps);
  }
  BLI_listbase_clear(&gpf->strokes);
}

/* Reduce the number of points in the stroke
 *
 * Note: This won't be called if all points are present/removed
 * TODO: Allow blending of growing/shrinking tip (e.g. for more gradual transitions)
 */
static void reduce_stroke_points(bGPDstroke *gps,
                                 const int num_points,
                                 const eBuildGpencil_Transition transition)
{
  bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * num_points, __func__);
  MDeformVert *new_dvert = NULL;
  if (gps->dvert != NULL) {
    new_dvert = MEM_callocN(sizeof(MDeformVert) * num_points, __func__);
  }

  /* Which end should points be removed from */
  // TODO: free stroke weights
  switch (transition) {
    case GP_BUILD_TRANSITION_GROW:   /* Show in forward order =
                                      * Remove ungrown-points from end of stroke. */
    case GP_BUILD_TRANSITION_SHRINK: /* Hide in reverse order =
                                      * Remove dead-points from end of stroke. */
    {
      /* copy over point data */
      memcpy(new_points, gps->points, sizeof(bGPDspoint) * num_points);
      if (gps->dvert != NULL) {
        memcpy(new_dvert, gps->dvert, sizeof(MDeformVert) * num_points);

        /* free unused point weights */
        for (int i = num_points; i < gps->totpoints; i++) {
          MDeformVert *dvert = &gps->dvert[i];
          BKE_gpencil_free_point_weights(dvert);
        }
      }
      break;
    }

    /* Hide in forward order = Remove points from start of stroke */
    case GP_BUILD_TRANSITION_FADE: {
      /* num_points is the number of points left after reducing.
       * We need to know how many to remove
       */
      const int offset = gps->totpoints - num_points;

      /* copy over point data */
      memcpy(new_points, gps->points + offset, sizeof(bGPDspoint) * num_points);
      if (gps->dvert != NULL) {
        memcpy(new_dvert, gps->dvert + offset, sizeof(MDeformVert) * num_points);

        /* free unused weights */
        for (int i = 0; i < offset; i++) {
          MDeformVert *dvert = &gps->dvert[i];
          BKE_gpencil_free_point_weights(dvert);
        }
      }
      break;
    }

    default:
      printf("ERROR: Unknown transition %d in %s()\n", (int)transition, __func__);
      break;
  }

  /* replace stroke geometry */
  MEM_SAFE_FREE(gps->points);
  MEM_SAFE_FREE(gps->dvert);
  gps->points = new_points;
  gps->dvert = new_dvert;
  gps->totpoints = num_points;

  /* mark stroke as needing to have its geometry caches rebuilt */
  gps->flag |= GP_STROKE_RECALC_GEOMETRY;
  gps->tot_triangles = 0;
  MEM_SAFE_FREE(gps->triangles);
}

/* --------------------------------------------- */

/* Stroke Data Table Entry - This represents one stroke being generated */
typedef struct tStrokeBuildDetails {
  bGPDstroke *gps;

  /* Indices - first/last indices for the stroke's points (overall) */
  size_t start_idx, end_idx;

  /* Number of points - Cache for more convenient access */
  int totpoints;
} tStrokeBuildDetails;

/* Sequential - Show strokes one after the other */
static void build_sequential(BuildGpencilModifierData *mmd, bGPDframe *gpf, float fac)
{
  const size_t tot_strokes = BLI_listbase_count(&gpf->strokes);
  bGPDstroke *gps;
  size_t i;

  /* 1) Compute proportion of time each stroke should occupy */
  /* NOTE: This assumes that the total number of points won't overflow! */
  tStrokeBuildDetails *table = MEM_callocN(sizeof(tStrokeBuildDetails) * tot_strokes, __func__);
  size_t totpoints = 0;

  /* 1.1) First pass - Tally up points */
  for (gps = gpf->strokes.first, i = 0; gps; gps = gps->next, i++) {
    tStrokeBuildDetails *cell = &table[i];

    cell->gps = gps;
    cell->totpoints = gps->totpoints;

    totpoints += cell->totpoints;
  }

  /* 1.2) Second pass - Compute the overall indices for points */
  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];

    if (i == 0) {
      cell->start_idx = 0;
    }
    else {
      cell->start_idx = (cell - 1)->end_idx;
    }
    cell->end_idx = cell->start_idx + cell->totpoints - 1;
  }

  /* 2) Determine the global indices for points that should be visible */
  size_t first_visible = 0;
  size_t last_visible = 0;

  switch (mmd->transition) {
    /* Show in forward order
     *  - As fac increases, the number of visible points increases
     */
    case GP_BUILD_TRANSITION_GROW:
      first_visible = 0; /* always visible */
      last_visible = (size_t)roundf(totpoints * fac);
      break;

    /* Hide in reverse order
     *  - As fac increases, the number of points visible at the end decreases
     */
    case GP_BUILD_TRANSITION_SHRINK:
      first_visible = 0; /* always visible (until last point removed) */
      last_visible = (size_t)(totpoints * (1.0f - fac));
      break;

    /* Hide in forward order
     *  - As fac increases, the early points start getting hidden
     */
    case GP_BUILD_TRANSITION_FADE:
      first_visible = (size_t)(totpoints * fac);
      last_visible = totpoints; /* i.e. visible until the end, unless first overlaps this */
      break;
  }

  /* 3) Go through all strokes, deciding which to keep, and/or how much of each to keep */
  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];

    /* Determine what portion of the stroke is visible */
    if ((cell->end_idx < first_visible) || (cell->start_idx > last_visible)) {
      /* Not visible at all - Either ended before */
      clear_stroke(gpf, cell->gps);
    }
    else {
      /* Some proportion of stroke is visible */
      /* XXX: Will the transition settings still be valid now? */
      if ((first_visible <= cell->start_idx) && (last_visible >= cell->end_idx)) {
        /* Do nothing - whole stroke is visible */
      }
      else if (first_visible > cell->start_idx) {
        /* Starts partway through this stroke */
        int num_points = cell->end_idx - first_visible;
        reduce_stroke_points(cell->gps, num_points, mmd->transition);
      }
      else {
        /* Ends partway through this stroke */
        int num_points = last_visible - cell->start_idx;
        reduce_stroke_points(cell->gps, num_points, mmd->transition);
      }
    }
  }

  /* Free table */
  MEM_freeN(table);
}

/* --------------------------------------------- */

/* Concurrent - Show multiple strokes at once */
// TODO: Allow random offsets to start times
// TODO: Allow varying speeds? Scaling of progress?
static void build_concurrent(BuildGpencilModifierData *mmd, bGPDframe *gpf, float fac)
{
  bGPDstroke *gps, *gps_next;
  int max_points = 0;

  const bool reverse = (mmd->transition != GP_BUILD_TRANSITION_GROW);

  /* 1) Determine the longest stroke, to figure out when short strokes should start */
  /* FIXME: A *really* long stroke here could dwarf everything else, causing bad timings */
  for (gps = gpf->strokes.first; gps; gps = gps->next) {
    if (gps->totpoints > max_points) {
      max_points = gps->totpoints;
    }
  }
  if (max_points == 0) {
    printf("ERROR: Strokes are all empty (GP Build Modifier: %s)\n", __func__);
    return;
  }

  /* 2) For each stroke, determine how it should be handled */
  for (gps = gpf->strokes.first; gps; gps = gps_next) {
    gps_next = gps->next;

    /* Relative Length of Stroke - Relative to the longest stroke,
     * what proportion of the available time should this stroke use
     */
    const float relative_len = (float)gps->totpoints / (float)max_points;

    /* Determine how many points should be left in the stroke */
    int num_points = 0;

    switch (mmd->time_alignment) {
      case GP_BUILD_TIMEALIGN_START: /* all start on frame 1 */
      {
        /* Build effect occurs over when fac = 0, to fac = relative_len */
        if (fac <= relative_len) {
          /* Scale fac to fit relative_len */
          /* FIXME: prevent potential div by zero (e.g. very short stroke vs one very long one) */
          const float scaled_fac = fac / relative_len;

          if (reverse) {
            num_points = (int)roundf((1.0f - scaled_fac) * gps->totpoints);
          }
          else {
            num_points = (int)roundf(scaled_fac * gps->totpoints);
          }
        }
        else {
          /* Build effect has ended */
          if (reverse) {
            num_points = 0;
          }
          else {
            num_points = gps->totpoints;
          }
        }

        break;
      }
      case GP_BUILD_TIMEALIGN_END: /* all end on same frame */
      {
        /* Build effect occurs over  1.0 - relative_len, to 1.0  (i.e. over the end of the range)
         */
        const float start_fac = 1.0f - relative_len;

        if (fac >= start_fac) {
          /* FIXME: prevent potential div by zero (e.g. very short stroke vs one very long one) */
          const float scaled_fac = (fac - start_fac) / relative_len;

          if (reverse) {
            num_points = (int)roundf((1.0f - scaled_fac) * gps->totpoints);
          }
          else {
            num_points = (int)roundf(scaled_fac * gps->totpoints);
          }
        }
        else {
          /* Build effect hasn't started */
          if (reverse) {
            num_points = gps->totpoints;
          }
          else {
            num_points = 0;
          }
        }

        break;
      }

        /* TODO... */
    }

    /* Modify the stroke geometry */
    if (num_points <= 0) {
      /* Nothing Left - Delete the stroke */
      clear_stroke(gpf, gps);
    }
    else if (num_points < gps->totpoints) {
      /* Remove some points */
      reduce_stroke_points(gps, num_points, mmd->transition);
    }
  }
}

/* --------------------------------------------- */

/* Entry-point for Build Modifier */
static void generateStrokes(GpencilModifierData *md,
                            Depsgraph *depsgraph,
                            Object *UNUSED(ob),
                            bGPDlayer *gpl,
                            bGPDframe *gpf)
{
  BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;
  const bool reverse = (mmd->transition != GP_BUILD_TRANSITION_GROW);

  const float ctime = DEG_get_ctime(depsgraph);
  // printf("GP Build Modifier - %f\n", ctime);

  /* Early exit if it's an empty frame */
  if (gpf->strokes.first == NULL) {
    return;
  }

  /* Omit layer if filter by layer */
  if (mmd->layername[0] != '\0') {
    if ((mmd->flag & GP_BUILD_INVERT_LAYER) == 0) {
      if (!STREQ(mmd->layername, gpl->info)) {
        return;
      }
    }
    else {
      if (STREQ(mmd->layername, gpl->info)) {
        return;
      }
    }
  }
  /* verify layer pass */
  if (mmd->layer_pass > 0) {
    if ((mmd->flag & GP_BUILD_INVERT_LAYERPASS) == 0) {
      if (gpl->pass_index != mmd->layer_pass) {
        return;
      }
    }
    else {
      if (gpl->pass_index == mmd->layer_pass) {
        return;
      }
    }
  }

  /* Early exit if outside of the frame range for this modifier
   * (e.g. to have one forward, and one backwards modifier)
   */
  if (mmd->flag & GP_BUILD_RESTRICT_TIME) {
    if ((ctime < mmd->start_frame) || (ctime > mmd->end_frame)) {
      return;
    }
  }

  /* Compute start and end frames for the animation effect
   * By default, the upper bound is given by the "maximum length" setting
   */
  float start_frame = gpf->framenum + mmd->start_delay;
  float end_frame = gpf->framenum + mmd->length;

  if (gpf->next) {
    /* Use the next frame or upper bound as end frame, whichever is lower/closer */
    end_frame = MIN2(end_frame, gpf->next->framenum);
  }

  /* Early exit if current frame is outside start/end bounds */
  /* NOTE: If we're beyond the next/prev frames (if existent), then we wouldn't have this problem
   * anyway... */
  if (ctime < start_frame) {
    /* Before Start - Animation hasn't started. Display initial state. */
    if (reverse) {
      /* 1) Reverse = Start with all, end with nothing.
       *    ==> Do nothing (everything already present)
       */
    }
    else {
      /* 2) Forward Order = Start with nothing, end with the full frame.
       *    ==> Free all strokes, and return an empty frame
       */
      gpf_clear_all_strokes(gpf);
    }

    /* Early exit */
    return;
  }
  else if (ctime >= end_frame) {
    /* Past End - Animation finished. Display final result. */
    if (reverse) {
      /* 1) Reverse = Start with all, end with nothing.
       *    ==> Free all strokes, and return an empty frame
       */
      gpf_clear_all_strokes(gpf);
    }
    else {
      /* 2) Forward Order = Start with nothing, end with the full frame.
       *    ==> Do Nothing (everything already present)
       */
    }

    /* Early exit */
    return;
  }

  /* Determine how far along we are between the keyframes */
  float fac = (ctime - start_frame) / (end_frame - start_frame);
  // printf("  Progress on %d = %f (%f - %f)\n", gpf->framenum, fac, start_frame, end_frame);

  /* Time management mode */
  switch (mmd->mode) {
    case GP_BUILD_MODE_SEQUENTIAL:
      build_sequential(mmd, gpf, fac);
      break;

    case GP_BUILD_MODE_CONCURRENT:
      build_concurrent(mmd, gpf, fac);
      break;

    default:
      printf("Unsupported build mode (%d) for GP Build Modifier: '%s'\n",
             mmd->mode,
             mmd->modifier.name);
      break;
  }
}

/* ******************************************** */

GpencilModifierTypeInfo modifierType_Gpencil_Build = {
    /* name */ "Build",
    /* structName */ "BuildGpencilModifierData",
    /* structSize */ sizeof(BuildGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_NoApply,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ generateStrokes,
    /* bakeModifier */ NULL,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ dependsOnTime,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* getDuplicationFactor */ NULL,
};
