/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscene
 *
 * Define `ED_scene_api_*` functions.
 */

#include "BLI_math_base.h"

#include "DNA_scene_types.h"

#include "ED_scene.hh"

#include "MEM_guardedalloc.h"

using FrameSampleT = uint32_t;
using FrameSumT = uint64_t;

/**
 * This values gives enough precision while not overflowing a 64-bit integer when accumulating.
 * Compared to the same calculation with `double` the error rate is less than 0.5 microsecond,
 * more than enough precision for FPS display.
 */
#define FIXED_UNIT FrameSampleT(65535)

/** Use report the difference (error) between fixed-point arithmetic and double-precision. */
// #define USE_DEBUG_REPORT_ERROR_MARGIN

struct FrameSample {
  FrameSampleT value;
#ifdef USE_DEBUG_REPORT_ERROR_MARGIN
  double value_db;
#endif
};

/**
 * For playback frame-rate info stored during runtime as `scene->fps_info`.
 */
struct ScreenFrameRateInfo {
  double time_curr;
  double time_prev;

#ifdef USE_DEBUG_REPORT_ERROR_MARGIN
  double error_sum;
  int error_samples;
#endif

  /** The target FPS, use to reset on change. */
  float fps_target;
  /** Final result, ignore when -1.0. */
  float fps_average;

  int times_fps_index;
  int times_fps_num;
  int times_fps_num_set;

  FrameSumT times_fps_sum;

  /** Over allocate (containing `times_fps_num` elements). */
  FrameSample times_fps[0];
};

void ED_scene_fps_average_clear(Scene *scene)
{
  if (scene->fps_info == nullptr) {
    return;
  }

#ifndef NDEBUG
  /* Assert this value has not somehow become out of sync as this would mean
   * the reported frame-rate would be wrong. */
  ScreenFrameRateInfo *fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
  FrameSumT times_fps_sum_cmp = 0;
  for (int i = 0; i < fpsi->times_fps_num_set; i++) {
    times_fps_sum_cmp += fpsi->times_fps[i].value;
  }
  BLI_assert(fpsi->times_fps_sum == times_fps_sum_cmp);
#endif

  MEM_freeN(scene->fps_info);
  scene->fps_info = nullptr;
}

void ED_scene_fps_average_accumulate(Scene *scene, const short fps_samples, const double ltime)
{
  const float fps_target = float(FPS);
  const int times_fps_num = (fps_samples > 0) ? fps_samples : max_ii(1, int(ceilf(fps_target)));

  ScreenFrameRateInfo *fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
  if (fpsi) {
    /* Reset when the target FPS changes.
     * Needed redraw times from when a different FPS was set do not contribute
     * to an average that is over/under the new target. */
    if ((fpsi->fps_target != fps_target) || (fpsi->times_fps_num != times_fps_num)) {
      MEM_freeN(fpsi);
      fpsi = nullptr;
      scene->fps_info = nullptr;
    }
  }

  /* If there isn't any info, initialize it first. */
  if (fpsi == nullptr) {
    scene->fps_info = MEM_callocN(
        sizeof(ScreenFrameRateInfo) + (sizeof(FrameSample) * times_fps_num), __func__);
    fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
    fpsi->fps_target = fps_target;
    fpsi->times_fps_num = times_fps_num;
    fpsi->times_fps_num_set = 0;
  }

  /* Update the values. */
  fpsi->time_curr = fpsi->time_prev;
  fpsi->time_prev = ltime;

  /* Mark as outdated. */
  fpsi->fps_average = -1.0f;
}

float ED_scene_fps_average_calc(const Scene *scene, float *r_fps_target)
{
  ScreenFrameRateInfo *fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
  if (fpsi == nullptr) {
    return -1.0f;
  }

  /* Doing an average for a more robust calculation. */
  if (fpsi->time_prev == 0.0 || fpsi->time_curr == 0.0) {
    /* The user should never see this. */
    fpsi->fps_average = -1.0f;
  }
  else if (fpsi->fps_average == -1.0) {
    if (fpsi->times_fps_index >= fpsi->times_fps_num) {
      fpsi->times_fps_index = 0;
    }

    /* Doing an average for a more robust calculation. */
    const double fps_sample = 1.0 / (fpsi->time_prev - fpsi->time_curr);

    {
      const FrameSampleT fps_prev = (fpsi->times_fps_num_set == fpsi->times_fps_num) ?
                                        fpsi->times_fps[fpsi->times_fps_index].value :
                                        FrameSampleT(0);
      const double fps_curr_db = std::round(fps_sample * double(FIXED_UNIT));
      BLI_assert(fps_curr_db >= 0.0f);
      const FrameSampleT fps_curr = FrameSampleT(fps_curr_db);
      fpsi->times_fps[fpsi->times_fps_index].value = fps_curr;
      fpsi->times_fps_sum -= fps_prev;
      fpsi->times_fps_sum += fps_curr;
    }
#ifdef USE_DEBUG_REPORT_ERROR_MARGIN
    fpsi->times_fps[fpsi->times_fps_index].value_db = fps_sample;
#endif

    fpsi->times_fps_index++;
    if (fpsi->times_fps_index > fpsi->times_fps_num_set) {
      fpsi->times_fps_num_set = fpsi->times_fps_index;
    }
    BLI_assert(fpsi->times_fps_num_set > 0);

    fpsi->fps_average = float((double(fpsi->times_fps_sum) / double(fpsi->times_fps_num_set)) /
                              FIXED_UNIT);

#ifdef USE_DEBUG_REPORT_ERROR_MARGIN
    {
      double fps_average_ref = 0.0f;
      for (int i = 0; i < fpsi->times_fps_num_set; i++) {
        fps_average_ref += fpsi->times_fps[i].value_db;
      }
      fps_average_ref = float(fps_average_ref / double(fpsi->times_fps_num_set));

      const float error = float(fps_average_ref) - fpsi->fps_average;
      fpsi->error_sum += error;
      fpsi->error_samples += 1;
      if ((fpsi->error_samples % 100) == 0) {
        printf("%s error: %.16f over %d samples (average %.16f)\n",
               __func__,
               fpsi->error_sum,
               fpsi->error_samples,
               fpsi->error_sum / double(fpsi->error_samples));
      }
    }
#endif /* USE_DEBUG_REPORT_ERROR_MARGIN */
  }

  *r_fps_target = fpsi->fps_target;
  return fpsi->fps_average;
}
