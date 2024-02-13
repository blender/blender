/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_gpencil_legacy_ui_common.h"

/* Two hard-coded values for GP_BUILD_MODE_ADDITIVE with GP_BUILD_TIMEMODE_DRAWSPEED. */

/* The minimum time gap we should worry about points with no time. */
#define GP_BUILD_CORRECTGAP 0.001
/* The time for geometric strokes */
#define GP_BUILD_TIME_GEOSTROKES 1.0

static void init_data(GpencilModifierData *md)
{
  BuildGpencilModifierData *gpmd = (BuildGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(BuildGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static bool depends_on_time(GpencilModifierData * /*md*/)
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
  for (gps = static_cast<bGPDstroke *>(gpf->strokes.first); gps; gps = gps_next) {
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
  if ((points_num == 0) || (gps->points == nullptr)) {
    clear_stroke(gpf, gps);
    return;
  }
  bGPDspoint *new_points = static_cast<bGPDspoint *>(
      MEM_callocN(sizeof(bGPDspoint) * points_num, __func__));
  MDeformVert *new_dvert = nullptr;
  if ((gps->dvert != nullptr) && (points_num > 0)) {
    new_dvert = static_cast<MDeformVert *>(
        MEM_callocN(sizeof(MDeformVert) * points_num, __func__));
  }

  /* Which end should points be removed from. */
  switch (transition) {
    case GP_BUILD_TRANSITION_GROW:   /* Show in forward order =
                                      * Remove ungrown-points from end of stroke. */
    case GP_BUILD_TRANSITION_SHRINK: /* Hide in reverse order =
                                      * Remove dead-points from end of stroke. */
    {
      /* copy over point data */
      blender::dna::shallow_copy_array(new_points, gps->points, points_num);
      if ((gps->dvert != nullptr) && (points_num > 0)) {
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
      blender::dna::shallow_copy_array(new_points, gps->points + offset, points_num);
      if ((gps->dvert != nullptr) && (points_num > 0)) {
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
      printf("ERROR: Unknown transition %d in %s()\n", int(transition), __func__);
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
        float weight = interpf(ending_weight, starting_weight, float(i - starting_index) / range);
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
      printf("ERROR: Unknown transition %d in %s()\n", int(transition), __func__);
      break;
  }
}

/* --------------------------------------------- */

/* Stroke Data Table Entry - This represents one stroke being generated */
struct tStrokeBuildDetails {
  bGPDstroke *gps;

  /* Indices - first/last indices for the stroke's points (overall) */
  size_t start_idx, end_idx;

  /* Number of points - Cache for more convenient access */
  int totpoints;

  /* Distance to control object, used to sort the strokes if set. */
  float distance;
};

static int cmp_stroke_build_details(const void *ps1, const void *ps2)
{
  tStrokeBuildDetails *p1 = (tStrokeBuildDetails *)ps1;
  tStrokeBuildDetails *p2 = (tStrokeBuildDetails *)ps2;
  return p1->distance > p2->distance ? 1 : (p1->distance == p2->distance ? 0 : -1);
}

/* Sequential - Show strokes one after the other (includes additive mode). */
static void build_sequential(Object *ob,
                             BuildGpencilModifierData *mmd,
                             Depsgraph *depsgraph,
                             bGPdata *gpd,
                             bGPDframe *gpf,
                             int target_def_nr,
                             float fac,
                             const float *ctime)
{
  /* Total number of strokes in this run. */
  size_t tot_strokes = BLI_listbase_count(&gpf->strokes);
  /* First stroke to build. */
  size_t start_stroke = 0;
  /* Pointer to current stroke. */
  bGPDstroke *gps;
  /* Recycled counter. */
  size_t i;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  /* Frame-rate of scene. */
  const float fps = (float(scene->r.frs_sec) / scene->r.frs_sec_base);

  /* 1) Determine which strokes to start with (& adapt total number of strokes to build). */
  if (mmd->mode == GP_BUILD_MODE_ADDITIVE) {
    if (gpf->prev) {
      start_stroke = BLI_listbase_count(&gpf->runtime.gpf_orig->prev->strokes);
    }
    if (start_stroke <= tot_strokes) {
      tot_strokes = tot_strokes - start_stroke;
    }
    else {
      start_stroke = 0;
    }
  }

  /* 2) Compute proportion of time each stroke should occupy. */
  /* NOTE: This assumes that the total number of points won't overflow! */
  tStrokeBuildDetails *table = static_cast<tStrokeBuildDetails *>(
      MEM_callocN(sizeof(tStrokeBuildDetails) * tot_strokes, __func__));
  /* Pointer to cache table of times for each point. */
  float *idx_times;
  /* Running overall time sum incrementing per point. */
  float sumtime = 0;
  /* Running overall point sum. */
  size_t sumpoints = 0;

  /* 2.1) Pass to initially tally up points. */
  for (gps = static_cast<bGPDstroke *>(BLI_findlink(&gpf->strokes, start_stroke)), i = 0; gps;
       gps = gps->next, i++)
  {
    tStrokeBuildDetails *cell = &table[i];

    cell->gps = gps;
    cell->totpoints = gps->totpoints;
    sumpoints += cell->totpoints;

    /* Compute distance to control object if set, and build according to that order. */
    if (mmd->object) {
      float sv1[3], sv2[3];
      mul_v3_m4v3(sv1, ob->object_to_world, &gps->points[0].x);
      mul_v3_m4v3(sv2, ob->object_to_world, &gps->points[gps->totpoints - 1].x);
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

  /* 2.2) If GP_BUILD_TIMEMODE_DRAWSPEED: Tally up point timestamps & delays to idx_times. */
  if (mmd->time_mode == GP_BUILD_TIMEMODE_DRAWSPEED) {
    idx_times = static_cast<float *>(MEM_callocN(sizeof(float) * sumpoints, __func__));
    /* Maximum time gap between strokes in seconds. */
    const float GP_BUILD_MAXGAP = mmd->speed_maxgap;
    /* Running reference to overall current point. */
    size_t curpoint = 0;
    /* Running timestamp of last point that had data. */
    float last_pointtime = 0;

    for (i = 0; i < tot_strokes; i++) {
      tStrokeBuildDetails *cell = &table[i];
      /* Adding delay between strokes to sumtime. */
      if (mmd->object == nullptr) {
        /* Normal case: Delay to last stroke. */
        if (i != 0 && 0 < cell->gps->inittime && 0 < (cell - 1)->gps->inittime) {
          float curgps_delay = fabs(cell->gps->inittime - (cell - 1)->gps->inittime) -
                               last_pointtime;
          if (0 < curgps_delay) {
            sumtime += std::min(curgps_delay, GP_BUILD_MAXGAP);
          }
        }
      }

      /* Going through the points of the current stroke
       * and filling in "zeropoints" where "time" = 0. */

      /* Count of consecutive points where "time" is 0. */
      int zeropoints = 0;
      for (int j = 0; j < cell->totpoints; j++) {
        /* Defining time for first point in stroke. */
        if (j == 0) {
          idx_times[curpoint] = sumtime;
          last_pointtime = cell->gps->points[0].time;
        }
        /* Entering subsequent points */
        else {
          if (cell->gps->points[j].time <= 0) {
            idx_times[curpoint] = sumtime;
            zeropoints++;
          }
          /* From here current point has time data */
          else {
            float deltatime = fabs(cell->gps->points[j].time - last_pointtime);
            /* Do we need to sanitize previous points? */
            if (0 < zeropoints) {
              /* Only correct if time-gap bigger than #GP_BUILD_CORRECTGAP. */
              if (GP_BUILD_CORRECTGAP < deltatime) {
                /* Cycling backwards through zero-points to fix them. */
                for (int k = 0; k < zeropoints; k++) {
                  float linear_fill = interpf(
                      0, deltatime, (float(k) + 1) / (zeropoints + 1)); /* Factor = Proportion. */
                  idx_times[curpoint - k - 1] = sumtime + linear_fill;
                }
              }
              else {
                zeropoints = 0;
              }
            }

            /* Normal behavior with time data. */
            idx_times[curpoint] = sumtime + deltatime;
            sumtime = idx_times[curpoint];
            last_pointtime = cell->gps->points[j].time;
            zeropoints = 0;
          }
        }
        curpoint += 1;
      }

      /* If stroke had no time data at all, use mmd->time_geostrokes. */
      if (zeropoints + 1 == cell->totpoints) {
        for (int j = 0; j < cell->totpoints; j++) {
          idx_times[int(curpoint) - j - 1] = float(cell->totpoints - j) *
                                                 GP_BUILD_TIME_GEOSTROKES /
                                                 float(cell->totpoints) +
                                             sumtime;
        }
        last_pointtime = GP_BUILD_TIME_GEOSTROKES;
        sumtime += GP_BUILD_TIME_GEOSTROKES;
      }
    }

    float gp_build_speedfactor = mmd->speed_fac;
    /* If current frame can't be built before next frame, adjust gp_build_speedfactor. */
    if (gpf->next && (gpf->framenum + sumtime * fps / gp_build_speedfactor) > gpf->next->framenum)
    {
      gp_build_speedfactor = sumtime * fps / (gpf->next->framenum - gpf->framenum);
    }
    /* Apply gp_build_speedfactor to all points & to sumtime. */
    for (i = 0; i < sumpoints; i++) {
      float *idx_time = &idx_times[i];
      *idx_time /= gp_build_speedfactor;
    }
    sumtime /= gp_build_speedfactor;
  }

  /* 2.3) Pass to compute overall indices for points (per stroke). */
  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];

    if (i == 0) {
      cell->start_idx = 0;
    }
    else {
      cell->start_idx = (cell - 1)->end_idx + 1;
    }
    cell->end_idx = cell->start_idx + cell->totpoints - 1;
  }

  /* 3) Determine the global indices for points that should be visible. */
  size_t first_visible = 0;
  size_t last_visible = 0;
  /* Need signed numbers because the representation of fading offset would exceed the beginning and
   * the end of offsets. */
  int fade_start = 0;
  int fade_end = 0;

  bool fading_enabled = (mmd->flag & GP_BUILD_USE_FADING);
  float set_fade_fac = fading_enabled ? mmd->fade_fac : 0.0f;
  float use_fac;

  if (mmd->time_mode == GP_BUILD_TIMEMODE_DRAWSPEED) {
    /* Recalculate equivalent of "fac" using timestamps. */
    float targettime = (*ctime - float(gpf->framenum)) / fps;
    fac = 0;
    /* If ctime is in current frame, find last point. */
    if (0 < targettime && targettime < sumtime) {
      /* All except GP_BUILD_TRANSITION_SHRINK count forwards. */
      if (mmd->transition != GP_BUILD_TRANSITION_SHRINK) {
        for (i = 0; i < sumpoints; i++) {
          if (targettime < idx_times[i]) {
            fac = float(i) / sumpoints;
            break;
          }
        }
      }
      else {
        for (i = 0; i < sumpoints; i++) {
          if (targettime < sumtime - idx_times[sumpoints - i - 1]) {
            fac = float(i) / sumpoints;
            break;
          }
        }
      }
    }
    /* Don't check if ctime is beyond time of current frame. */
    else if (targettime >= sumtime) {
      fac = 1;
    }
  }
  use_fac = interpf(1 + set_fade_fac, 0, fac);
  float use_fade_fac = use_fac - set_fade_fac;
  CLAMP(use_fade_fac, 0.0f, 1.0f);

  switch (mmd->transition) {
    /* Show in forward order
     *  - As fac increases, the number of visible points increases
     */
    case GP_BUILD_TRANSITION_GROW:
      first_visible = 0; /* always visible */
      last_visible = size_t(roundf(sumpoints * use_fac));
      fade_start = int(roundf(sumpoints * use_fade_fac));
      fade_end = last_visible;
      break;

    /* Hide in reverse order
     *  - As fac increases, the number of points visible at the end decreases
     */
    case GP_BUILD_TRANSITION_SHRINK:
      first_visible = 0; /* always visible (until last point removed) */
      last_visible = size_t(sumpoints * (1.0f + set_fade_fac - use_fac));
      fade_start = int(roundf(sumpoints * (1.0f - use_fade_fac - set_fade_fac)));
      fade_end = last_visible;
      break;

    /* Hide in forward order
     *  - As fac increases, the early points start getting hidden
     */
    case GP_BUILD_TRANSITION_VANISH:
      first_visible = size_t(sumpoints * use_fade_fac);
      last_visible = sumpoints; /* i.e. visible until the end, unless first overlaps this */
      fade_start = first_visible;
      fade_end = int(roundf(sumpoints * use_fac));
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
      if (fade_start != fade_end && int(cell->start_idx) < fade_end &&
          int(cell->end_idx) > fade_start)
      {
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
                           eBuildGpencil_Transition(mmd->transition),
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
        reduce_stroke_points(
            gpd, gpf, cell->gps, points_num, eBuildGpencil_Transition(mmd->transition));
      }
      else {
        /* Ends partway through this stroke */
        int points_num = last_visible - cell->start_idx;
        reduce_stroke_points(
            gpd, gpf, cell->gps, points_num, eBuildGpencil_Transition(mmd->transition));
      }
    }
  }

  /* Free table */
  MEM_freeN(table);
  if (mmd->time_mode == GP_BUILD_TIMEMODE_DRAWSPEED) {
    MEM_freeN(idx_times);
  }
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
  /* Todo: A *really* long stroke here could dwarf everything else, causing bad timings */
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
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
  for (gps = static_cast<bGPDstroke *>(gpf->strokes.first); gps; gps = gps_next) {
    gps_next = gps->next;

    /* Relative Length of Stroke - Relative to the longest stroke,
     * what proportion of the available time should this stroke use
     */
    const float relative_len = float(gps->totpoints) / float(max_points);

    /* Determine how many points should be left in the stroke */
    int points_num = 0;

    switch (mmd->time_alignment) {
      case GP_BUILD_TIMEALIGN_START: /* all start on frame 1 */
      {
        /* Scale fac to fit relative_len */
        const float scaled_fac = use_fac / std::max(relative_len, PSEUDOINVERSE_EPSILON);

        if (reverse) {
          points_num = int(roundf((1.0f - scaled_fac) * gps->totpoints));
        }
        else {
          points_num = int(roundf(scaled_fac * gps->totpoints));
        }

        break;
      }
      case GP_BUILD_TIMEALIGN_END: /* all end on same frame */
      {
        /* Build effect occurs over  1.0 - relative_len, to 1.0  (i.e. over the end of the range)
         */
        const float start_fac = 1.0f - relative_len;

        const float scaled_fac = (use_fac - start_fac) /
                                 std::max(relative_len, PSEUDOINVERSE_EPSILON);

        if (reverse) {
          points_num = int(roundf((1.0f - scaled_fac) * gps->totpoints));
        }
        else {
          points_num = int(roundf(scaled_fac * gps->totpoints));
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
      float max_weight = float(points_num + more_points) / fade_points;
      CLAMP(max_weight, 0.0f, 1.0f);
      int starting_index = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                               gps->totpoints - points_num - more_points :
                               points_num - 1 - fade_points + more_points;
      int ending_index = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                             gps->totpoints - points_num + fade_points - more_points :
                             points_num - 1 + more_points;
      float starting_weight = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                                  (float(more_points) / fade_points) :
                                  max_weight;
      float ending_weight = mmd->transition == GP_BUILD_TRANSITION_VANISH ?
                                max_weight :
                                (float(more_points) / fade_points);
      CLAMP(starting_index, 0, gps->totpoints - 1);
      CLAMP(ending_index, 0, gps->totpoints - 1);
      fade_stroke_points(gps,
                         starting_index,
                         ending_index,
                         starting_weight,
                         ending_weight,
                         target_def_nr,
                         eBuildGpencil_Transition(mmd->transition),
                         mmd->fade_thickness_strength,
                         mmd->fade_opacity_strength);
      if (points_num < gps->totpoints) {
        /* Remove some points */
        reduce_stroke_points(gpd, gpf, gps, points_num, eBuildGpencil_Transition(mmd->transition));
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
  /* Prevent incompatible options at runtime. */
  if (mmd->mode == GP_BUILD_MODE_ADDITIVE) {
    mmd->transition = GP_BUILD_TRANSITION_GROW;
    mmd->start_delay = 0;
  }
  if (mmd->mode == GP_BUILD_MODE_CONCURRENT && mmd->time_mode == GP_BUILD_TIMEMODE_DRAWSPEED) {
    mmd->time_mode = GP_BUILD_TIMEMODE_FRAMES;
  }

  const bool reverse = (mmd->transition != GP_BUILD_TRANSITION_GROW);
  const bool is_percentage = (mmd->time_mode == GP_BUILD_TIMEMODE_PERCENTAGE);

  const float ctime = DEG_get_ctime(depsgraph);

  /* Early exit if it's an empty frame */
  if (gpf->strokes.first == nullptr) {
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

  /* Default "fac" value to call build_sequential even with
   * GP_BUILD_TIMEMODE_DRAWSPEED, which uses separate logic
   * in function build_sequential()
   */
  float fac = 1;

  if (mmd->time_mode != GP_BUILD_TIMEMODE_DRAWSPEED) {
    /* Compute start and end frames for the animation effect
     * By default, the upper bound is given by the "length" setting.
     */
    float start_frame = is_percentage ? gpf->framenum : gpf->framenum + mmd->start_delay;
    /* When use percentage don't need a limit in the upper bound, so use a maximum value for the
     * last frame. */
    float end_frame = is_percentage ? start_frame + 9999 : start_frame + mmd->length;

    if (gpf->next) {
      /* Use the next frame or upper bound as end frame, whichever is lower/closer */
      end_frame = std::min(end_frame, float(gpf->next->framenum));
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
    /* Determine how far along we are given current time, start_frame and end_frame */
    fac = is_percentage ? mmd->percentage_fac : (ctime - start_frame) / (end_frame - start_frame);
  }

  /* Calling the correct build mode */
  switch (mmd->mode) {
    case GP_BUILD_MODE_SEQUENTIAL:
    case GP_BUILD_MODE_ADDITIVE:
      build_sequential(ob, mmd, depsgraph, gpd, gpf, target_def_nr, fac, &ctime);
      break;

    case GP_BUILD_MODE_CONCURRENT:
      build_concurrent(mmd, gpd, gpf, target_def_nr, fac);
      break;

    default:
      printf("Unsupported build mode (%d) for GP Build Modifier: '%s'\n",
             mmd->mode,
             mmd->modifier.name);
      break;
  }
}

/* Entry-point for Build Modifier */
static void generate_strokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == nullptr) {
      continue;
    }
    generate_geometry(md, depsgraph, ob, gpd, gpl, gpf);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  const int mode = RNA_enum_get(ptr, "mode");
  int time_mode = RNA_enum_get(ptr, "time_mode");

  uiLayoutSetPropSep(layout, true);

  /* First: Build mode and build settings. */
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (mode == GP_BUILD_MODE_SEQUENTIAL) {
    uiItemR(layout, ptr, "transition", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  if (mode == GP_BUILD_MODE_CONCURRENT) {
    /* Concurrent mode doesn't support GP_BUILD_TIMEMODE_DRAWSPEED, so unset it. */
    if (time_mode == GP_BUILD_TIMEMODE_DRAWSPEED) {
      RNA_enum_set(ptr, "time_mode", GP_BUILD_TIMEMODE_FRAMES);
      time_mode = GP_BUILD_TIMEMODE_FRAMES;
    }
    uiItemR(layout, ptr, "transition", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemS(layout);

  /* Second: Time mode and time settings. */

  uiItemR(layout, ptr, "time_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (mode == GP_BUILD_MODE_CONCURRENT) {
    uiItemR(layout, ptr, "concurrent_time_alignment", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  switch (time_mode) {
    case GP_BUILD_TIMEMODE_DRAWSPEED:
      uiItemR(layout, ptr, "speed_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "speed_maxgap", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    case GP_BUILD_TIMEMODE_FRAMES:
      uiItemR(layout, ptr, "length", UI_ITEM_NONE, IFACE_("Frames"), ICON_NONE);
      if (mode != GP_BUILD_MODE_ADDITIVE) {
        uiItemR(layout, ptr, "start_delay", UI_ITEM_NONE, nullptr, ICON_NONE);
      }
      break;
    case GP_BUILD_TIMEMODE_PERCENTAGE:
      uiItemR(layout, ptr, "percentage_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    default:
      break;
  }
  uiItemS(layout);
  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  /* Some housekeeping to prevent clashes between incompatible
   * options */

  /* Check for incompatible time modifier. */
  Object *ob = static_cast<Object *>(ob_ptr.data);
  GpencilModifierData *md = static_cast<GpencilModifierData *>(ptr->data);
  if (BKE_gpencil_modifiers_findby_type(ob, eGpencilModifierType_Time) != nullptr) {
    BKE_gpencil_modifier_set_error(md, "Build and Time Offset modifiers are incompatible");
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void frame_range_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(
      layout, ptr, "use_restrict_frame_range", UI_ITEM_NONE, IFACE_("Custom Range"), ICON_NONE);
}

static void frame_range_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "frame_start", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
}

static void fading_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_fading", UI_ITEM_NONE, IFACE_("Fade"), ICON_NONE);
}

static void fading_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "fade_factor", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "fade_thickness_strength", UI_ITEM_NONE, IFACE_("Thickness"), ICON_NONE);
  uiItemR(col, ptr, "fade_opacity_strength", UI_ITEM_NONE, IFACE_("Opacity"), ICON_NONE);

  uiItemPointerR(layout,
                 ptr,
                 "target_vertex_group",
                 &ob_ptr,
                 "vertex_groups",
                 IFACE_("Weight Output"),
                 ICON_NONE);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, false, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Build, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "frame_range", "", frame_range_header_draw, frame_range_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "fading", "", fading_header_draw, fading_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "_mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  BuildGpencilModifierData *lmd = (BuildGpencilModifierData *)md;
  if (lmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Build Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
}

/* ******************************************** */

GpencilModifierTypeInfo modifierType_Gpencil_Build = {
    /*name*/ N_("Build"),
    /*struct_name*/ "BuildGpencilModifierData",
    /*struct_size*/ sizeof(BuildGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_NoApply,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ nullptr,
    /*generate_strokes*/ generate_strokes,
    /*bake_modifier*/ nullptr,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
