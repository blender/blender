/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_sort.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"

static void initData(GpencilModifierData *md)
{
  BuildGpencilModifierData *gpmd = (BuildGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(BuildGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
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
 * NOTE: This won't be called if all points are present/removed
 */
static void reduce_stroke_points(bGPdata *gpd,
                                 bGPDframe *gpf,
                                 bGPDstroke *gps,
                                 const int points_num,
                                 const eBuildGpencil_Transition transition)
{
  if ((points_num == 0) || (gps->points == NULL)) {
    clear_stroke(gpf, gps);
    return;
  }
  bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * points_num, __func__);
  MDeformVert *new_dvert = NULL;
  if ((gps->dvert != NULL) && (points_num > 0)) {
    new_dvert = MEM_callocN(sizeof(MDeformVert) * points_num, __func__);
  }

  /* Which end should points be removed from. */
  switch (transition) {
    case GP_BUILD_TRANSITION_GROW:   /* Show in forward order =
                                      * Remove ungrown-points from end of stroke. */
    case GP_BUILD_TRANSITION_SHRINK: /* Hide in reverse order =
                                      * Remove dead-points from end of stroke. */
    {
      /* copy over point data */
      memcpy(new_points, gps->points, sizeof(bGPDspoint) * points_num);
      if ((gps->dvert != NULL) && (points_num > 0)) {
        memcpy(new_dvert, gps->dvert, sizeof(MDeformVert) * points_num);

        /* free unused point weights */
        for (int i = points_num; i < gps->totpoints; i++) {
          MDeformVert *dvert = &gps->dvert[i];
          BKE_gpencil_free_point_weights(dvert);
        }
      }
      break;
    }

    /* Hide in forward order = Remove points from start of stroke */
    case GP_BUILD_TRANSITION_VANISH: {
      /* points_num is the number of points left after reducing.
       * We need to know how many to remove
       */
      const int offset = gps->totpoints - points_num;

      /* copy over point data */
      memcpy(new_points, gps->points + offset, sizeof(bGPDspoint) * points_num);
      if ((gps->dvert != NULL) && (points_num > 0)) {
        memcpy(new_dvert, gps->dvert + offset, sizeof(MDeformVert) * points_num);

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
  gps->totpoints = points_num;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void fade_stroke_points(bGPDstroke *gps,
                               const int starting_index,
                               const int ending_index,
                               const float starting_weight,
                               const float ending_weight,
                               const int target_def_nr,
                               const eBuildGpencil_Transition transition,
                               const float thickness_strength,
                               const float opacity_strength)
{
  MDeformVert *dvert;

  int range = ending_index - starting_index;
  if (!range) {
    range = 1;
  }

  /* Which end should points be removed from */
  switch (transition) {
    /* Because starting_weight and ending_weight are set in correct order before calling this
     * function, those three modes can use the same interpolation code. */
    case GP_BUILD_TRANSITION_GROW:
    case GP_BUILD_TRANSITION_SHRINK:
    case GP_BUILD_TRANSITION_VANISH: {
      for (int i = starting_index; i <= ending_index; i++) {
        float weight = interpf(
            ending_weight, starting_weight, (float)(i - starting_index) / range);
        if (target_def_nr >= 0) {
          dvert = &gps->dvert[i];
          MDeformWeight *dw = BKE_defvert_ensure_index(dvert, target_def_nr);
          if (dw) {
            dw->weight = weight;
            CLAMP(dw->weight, 0.0f, 1.0f);
          }
        }
        if (thickness_strength > 1e-5) {
          gps->points[i].pressure *= interpf(weight, 1.0f, thickness_strength);
        }
        if (opacity_strength > 1e-5) {
          gps->points[i].strength *= interpf(weight, 1.0f, opacity_strength);
        }
      }
      break;
    }

    default:
      printf("ERROR: Unknown transition %d in %s()\n", (int)transition, __func__);
      break;
  }
}

/* --------------------------------------------- */

/* Stroke Data Table Entry - This represents one stroke being generated */
typedef struct tStrokeBuildDetails {
  bGPDstroke *gps;

  /* Indices - first/last indices for the stroke's points (overall) */
  size_t start_idx, end_idx;

  /* Number of points - Cache for more convenient access */
  int totpoints;

  /* Distance to control object, used to sort the strokes if set. */
  float distance;
} tStrokeBuildDetails;

static int cmp_stroke_build_details(const void *ps1, const void *ps2)
{
  tStrokeBuildDetails *p1 = (tStrokeBuildDetails *)ps1;
  tStrokeBuildDetails *p2 = (tStrokeBuildDetails *)ps2;
  return p1->distance > p2->distance ? 1 : (p1->distance == p2->distance ? 0 : -1);
}

/* Sequential and additive - Show strokes one after the other. */
static void build_sequential(Object *ob,
                             BuildGpencilModifierData *mmd,
                             bGPdata *gpd,
                             bGPDframe *gpf,
                             const int target_def_nr,
                             float fac,
                             bool additive)
{
  size_t tot_strokes = BLI_listbase_count(&gpf->strokes);
  size_t start_stroke;
  bGPDstroke *gps;
  size_t i;

  /* 1) Determine which strokes to start with & total strokes to build. */

  if (additive) {
    if (gpf->prev) {
      start_stroke = BLI_listbase_count(&gpf->prev->strokes);
    }
    else {
      start_stroke = 0;
    }
    if (start_stroke <= tot_strokes) {
      tot_strokes = tot_strokes - start_stroke;
    }
    else {
      start_stroke = 0;
    }
  }
  else {
    start_stroke = 0;
  }

  /* 2) Compute proportion of time each stroke should occupy */
  /* NOTE: This assumes that the total number of points won't overflow! */
  tStrokeBuildDetails *table = MEM_callocN(sizeof(tStrokeBuildDetails) * tot_strokes, __func__);
  size_t totpoints = 0;

  /* 2.1) First pass - Tally up points */
  for (gps = BLI_findlink(&gpf->strokes, start_stroke), i = 0; gps; gps = gps->next, i++) {
    tStrokeBuildDetails *cell = &table[i];

    cell->gps = gps;
    cell->totpoints = gps->totpoints;

    totpoints += cell->totpoints;

    /* Compute distance to control object if set, and build according to that order. */
    if (mmd->object) {
      float sv1[3], sv2[3];
      mul_v3_m4v3(sv1, ob->obmat, &gps->points[0].x);
      mul_v3_m4v3(sv2, ob->obmat, &gps->points[gps->totpoints - 1].x);
      float dist_l = len_v3v3(sv1, mmd->object->loc);
      float dist_r = len_v3v3(sv2, mmd->object->loc);
      if (dist_r < dist_l) {
        BKE_gpencil_stroke_flip(gps);
        cell->distance = dist_r;
      }
      else {
        cell->distance = dist_l;
      }
    }
  }

  if (mmd->object) {
    qsort(table, tot_strokes, sizeof(tStrokeBuildDetails), cmp_stroke_build_details);
  }

  /* 2.2) Second pass - Compute the overall indices for points */
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

  /* 3) Determine the global indices for points that should be visible */
  size_t first_visible = 0;
  size_t last_visible = 0;
  /* Need signed numbers because the representation of fading offset would exceed the beginning and
   * the end of offsets. */
  int fade_start = 0;
  int fade_end = 0;

  bool fading_enabled = (mmd->flag & GP_BUILD_USE_FADING);

  float set_fade_fac = fading_enabled ? mmd->fade_fac : 0.0f;
  float use_fac = interpf(1 + set_fade_fac, 0, fac);
  float use_fade_fac = use_fac - set_fade_fac;
  CLAMP(use_fade_fac, 0.0f, 1.0f);

  switch (mmd->transition) {
      /* Show in forward order
       *  - As fac increases, the number of visible points increases
       */
    case GP_BUILD_TRANSITION_GROW:
      first_visible = 0; /* always visible */
      last_visible = (size_t)roundf(totpoints * use_fac);
      fade_start = (int)roundf(totpoints * use_fade_fac);
      fade_end = last_visible;
      break;

      /* Hide in reverse order
       *  - As fac increases, the number of points visible at the end decreases
       */
    case GP_BUILD_TRANSITION_SHRINK:
      first_visible = 0; /* always visible (until last point removed) */
      last_visible = (size_t)(totpoints * (1.0f + set_fade_fac - use_fac));
      fade_start = (int)roundf(totpoints * (1.0f - use_fade_fac - set_fade_fac));
      fade_end = last_visible;
      break;

      /* Hide in forward order
       *  - As fac increases, the early points start getting hidden
       */
    case GP_BUILD_TRANSITION_VANISH:
      first_visible = (size_t)(totpoints * use_fade_fac);
      last_visible = totpoints; /* i.e. visible until the end, unless first overlaps this */
      fade_start = first_visible;
      fade_end = (int)roundf(totpoints * use_fac);
      break;
  }

  /* 4) Go through all strokes, deciding which to keep, and/or how much of each to keep */
  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];

    /* Determine what portion of the stroke is visible */
    if ((cell->end_idx < first_visible) || (cell->start_idx > last_visible)) {
      /* Not visible at all - Either ended before */
      clear_stroke(gpf, cell->gps);
    }
    else {
      if (fade_start != fade_end && (int)cell->start_idx < fade_end &&
          (int)cell->end_idx > fade_start) {
        int start_index = fade_start - cell->start_idx;
        int end_index = cell->totpoints + fade_end - cell->end_idx - 1;
        CLAMP(start_index, 0, cell->totpoints - 1);
        CLAMP(end_index, 0, cell->totpoints - 1);
        float start_weight = ratiof(fade_start, fade_end, cell->start_idx + start_index);
        float end_weight = ratiof(fade_start, fade_end, cell->start_idx + end_index);
        if (mmd->transition != GP_BUILD_TRANSITION_VANISH) {
          start_weight = 1.0f - start_weight;
          end_weight = 1.0f - end_weight;
        }
        fade_stroke_points(cell->gps,
                           start_index,
                           end_index,
                           start_weight,
                           end_weight,
                           target_def_nr,
                           mmd->transition,
                           mmd->fade_thickness_strength,
                           mmd->fade_opacity_strength);
        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, cell->gps);
      }
      /* Some proportion of stroke is visible */
      if ((first_visible <= cell->start_idx) && (last_visible >= cell->end_idx)) {
        /* Do nothing - whole stroke is visible */
      }
      else if (first_visible > cell->start_idx) {
        /* Starts partway through this stroke */
        int points_num = cell->end_idx - first_visible;
        reduce_stroke_points(gpd, gpf, cell->gps, points_num, mmd->transition);
      }
      else {
        /* Ends partway through this stroke */
        int points_num = last_visible - cell->start_idx;
        reduce_stroke_points(gpd, gpf, cell->gps, points_num, mmd->transition);
      }
    }
  }

  /* Free table */
  MEM_freeN(table);
}

/* --------------------------------------------- */

/* Concurrent - Show multiple strokes at once */
static void build_concurrent(BuildGpencilModifierData *mmd,
                             bGPdata *gpd,
                             bGPDframe *gpf,
                             const int target_def_nr,
                             float fac)
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

  bool fading_enabled = (mmd->flag & GP_BUILD_USE_FADING);
  float set_fade_fac = fading_enabled ? mmd->fade_fac : 0.0f;
  float use_fac = interpf(1 + set_fade_fac, 0, fac);
  use_fac = reverse ? use_fac - set_fade_fac : use_fac;
  int fade_points = set_fade_fac * max_points;

  /* 2) For each stroke, determine how it should be handled */
  for (gps = gpf->strokes.first; gps; gps = gps_next) {
    gps_next = gps->next;

    /* Relative Length of Stroke - Relative to the longest stroke,
     * what proportion of the available time should this stroke use
     */
    const float relative_len = (float)gps->totpoints / (float)max_points;

    /* Determine how many points should be left in the stroke */
    int points_num = 0;

    switch (mmd->time_alignment) {
      case GP_BUILD_TIMEALIGN_START: /* all start on frame 1 */
      {
        /* Scale fac to fit relative_len */
        const float scaled_fac = use_fac / MAX2(relative_len, PSEUDOINVERSE_EPSILON);

        if (reverse) {
          points_num = (int)roundf((1.0f - scaled_fac) * gps->totpoints);
        }
        else {
          points_num = (int)roundf(scaled_fac * gps->totpoints);
        }

        break;
      }
      case GP_BUILD_TIMEALIGN_END: /* all end on same frame */
      {
        /* Build effect occurs over  1.0 - relative_len, to 1.0  (i.e. over the end of the range)
         */
        const float start_fac = 1.0f - relative_len;

        const float scaled_fac = (use_fac - start_fac) / MAX2(relative_len, PSEUDOINVERSE_EPSILON);

        if (reverse) {
          points_num = (int)roundf((1.0f - scaled_fac) * gps->totpoints);
        }
        else {
          points_num = (int)roundf(scaled_fac * gps->totpoints);
        }

        break;
      }
    }

    /* Modify the stroke geometry */
    if (points_num <= 0) {
      /* Nothing Left - Delete the stroke */
      clear_stroke(gpf, gps);
    }
    else {
      int more_points = points_num - gps->totpoints;
      CLAMP(more_points, 0, fade_points + 1);
      float max_weight = (float)(points_num + more_points) / fade_points;
      CLAMP(max_weight, 0.0f, 1.0f);
      int starting_index = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                               gps->totpoints - points_num - more_points :
                               points_num - 1 - fade_points + more_points;
      int ending_index = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                             gps->totpoints - points_num + fade_points - more_points :
                             points_num - 1 + more_points;
      float starting_weight = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                                  ((float)more_points / fade_points) :
                                  max_weight;
      float ending_weight = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                                max_weight :
                                ((float)more_points / fade_points);
      CLAMP(starting_index, 0, gps->totpoints - 1);
      CLAMP(ending_index, 0, gps->totpoints - 1);
      fade_stroke_points(gps,
                         starting_index,
                         ending_index,
                         starting_weight,
                         ending_weight,
                         target_def_nr,
                         mmd->transition,
                         mmd->fade_thickness_strength,
                         mmd->fade_opacity_strength);
      if (points_num < gps->totpoints) {
        /* Remove some points */
        reduce_stroke_points(gpd, gpf, gps, points_num, mmd->transition);
      }
    }
  }
}

/* --------------------------------------------- */

static void generate_geometry(GpencilModifierData *md,
                              Depsgraph *depsgraph,
                              Object *ob,
                              bGPdata *gpd,
                              bGPDlayer *gpl,
                              bGPDframe *gpf)
{
  BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;
  if (mmd->mode == GP_BUILD_MODE_ADDITIVE) {
    mmd->transition = GP_BUILD_TRANSITION_GROW;
  }
  const bool reverse = (mmd->transition != GP_BUILD_TRANSITION_GROW);
  const bool is_percentage = (mmd->flag & GP_BUILD_PERCENTAGE);

  const float ctime = DEG_get_ctime(depsgraph);

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

  int target_def_nr = -1;
  if (mmd->flag & GP_BUILD_USE_FADING) {
    /* If there are weight output, initialize it with a default weight of 1. */
    target_def_nr = BKE_object_defgroup_name_index(ob, mmd->target_vgname);
    if (target_def_nr >= 0) {
      LISTBASE_FOREACH (bGPDstroke *, fgps, &gpf->strokes) {
        BKE_gpencil_dvert_ensure(fgps);
        /* Assign a initial weight of 1, and only process those who needs additional fading. */
        for (int j = 0; j < fgps->totpoints; j++) {
          MDeformVert *dvert = &fgps->dvert[j];
          MDeformWeight *dw = BKE_defvert_ensure_index(dvert, target_def_nr);
          if (dw) {
            dw->weight = 1.0f;
          }
        }
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
  float start_frame = is_percentage ? gpf->framenum : gpf->framenum + mmd->start_delay;
  /* When use percentage don't need a limit in the upper bound, so use a maximum value for the last
   * frame. */
  float end_frame = is_percentage ? start_frame + 9999 : start_frame + mmd->length;

  if (gpf->next) {
    /* Use the next frame or upper bound as end frame, whichever is lower/closer */
    end_frame = MIN2(end_frame, gpf->next->framenum);
  }

  /* Early exit if current frame is outside start/end bounds */
  /* NOTE: If we're beyond the next/previous frames (if existent),
   * then we wouldn't have this problem anyway... */
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
  if (ctime >= end_frame) {
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
  float fac = is_percentage ? mmd->percentage_fac :
                              (ctime - start_frame) / (end_frame - start_frame);

  /* Time management mode */
  switch (mmd->mode) {
    case GP_BUILD_MODE_SEQUENTIAL:
      build_sequential(ob, mmd, gpd, gpf, target_def_nr, fac, false);
      break;

    case GP_BUILD_MODE_CONCURRENT:
      build_concurrent(mmd, gpd, gpf, target_def_nr, fac);
      break;

    case GP_BUILD_MODE_ADDITIVE:
      build_sequential(ob, mmd, gpd, gpf, target_def_nr, fac, true);
      break;

    default:
      printf("Unsupported build mode (%d) for GP Build Modifier: '%s'\n",
             mmd->mode,
             mmd->modifier.name);
      break;
  }
}

/* Entry-point for Build Modifier */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    generate_geometry(md, depsgraph, ob, gpd, gpl, gpf);
  }
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  int mode = RNA_enum_get(ptr, "mode");
  const bool use_percentage = RNA_boolean_get(ptr, "use_percentage");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
  if (mode == GP_BUILD_MODE_CONCURRENT) {
    uiItemR(layout, ptr, "concurrent_time_alignment", 0, NULL, ICON_NONE);
  }

  uiItemS(layout);

  if (ELEM(mode, GP_BUILD_MODE_SEQUENTIAL, GP_BUILD_MODE_CONCURRENT)) {
    uiItemR(layout, ptr, "transition", 0, NULL, ICON_NONE);
  }
  row = uiLayoutRow(layout, true);
  uiLayoutSetActive(row, !use_percentage);
  uiItemR(row, ptr, "start_delay", 0, NULL, ICON_NONE);
  row = uiLayoutRow(layout, true);
  uiLayoutSetActive(row, !use_percentage);
  uiItemR(row, ptr, "length", 0, IFACE_("Frames"), ICON_NONE);

  uiItemS(layout);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Factor"));
  uiLayoutSetPropDecorate(row, false);
  uiItemR(row, ptr, "use_percentage", 0, "", ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, use_percentage);
  uiItemR(sub, ptr, "percentage_factor", 0, "", ICON_NONE);
  uiItemDecoratorR(row, ptr, "percentage_factor", 0);

  uiItemS(layout);

  if (ELEM(mode, GP_BUILD_MODE_SEQUENTIAL, GP_BUILD_MODE_ADDITIVE)) {
    uiItemR(layout, ptr, "object", 0, NULL, ICON_NONE);
  }

  /* Check for incompatible time modifier. */
  Object *ob = ob_ptr.data;
  GpencilModifierData *md = ptr->data;
  if (BKE_gpencil_modifiers_findby_type(ob, eGpencilModifierType_Time) != NULL) {
    BKE_gpencil_modifier_set_error(md, "Build and Time Offset modifiers are incompatible");
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void frame_range_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_restrict_frame_range", 0, IFACE_("Custom Range"), ICON_NONE);
}

static void frame_range_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "frame_start", 0, IFACE_("Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", 0, IFACE_("End"), ICON_NONE);
}

static void fading_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_fading", 0, IFACE_("Fade"), ICON_NONE);
}

static void fading_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "fade_factor", 0, IFACE_("Factor"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "fade_thickness_strength", 0, IFACE_("Thickness"), ICON_NONE);
  uiItemR(col, ptr, "fade_opacity_strength", 0, IFACE_("Opacity"), ICON_NONE);

  uiItemPointerR(layout,
                 ptr,
                 "target_vertex_group",
                 &ob_ptr,
                 "vertex_groups",
                 IFACE_("Weight Output"),
                 ICON_NONE);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, false, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Build, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "frame_range", "", frame_range_header_draw, frame_range_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "fading", "", fading_header_draw, fading_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "_mask", "Influence", NULL, mask_panel_draw, panel_type);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int UNUSED(mode))
{
  BuildGpencilModifierData *lmd = (BuildGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Build Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
}

/* ******************************************** */

GpencilModifierTypeInfo modifierType_Gpencil_Build = {
    /* name */ N_("Build"),
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
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
