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

/**
 * For playback frame-rate info stored during runtime as `scene->fps_info`.
 */
struct ScreenFrameRateInfo {
  double time_curr;
  double time_prev;

  /** The target FPS, use to reset on change. */
  float fps_target;
  /** Final result, ignore when -1.0. */
  float fps_average;

  int times_fps_index;
  int times_fps_num;
  int times_fps_num_set;

  /** Over allocate. */
  float times_fps[0];
};

void ED_scene_fps_average_clear(Scene *scene)
{
  MEM_SAFE_FREE(scene->fps_info);
}

void ED_scene_fps_average_accumulate(Scene *scene, const uchar fps_samples, const double ltime)
{
  const float fps_target = float(FPS);
  const int times_fps_num = fps_samples ? fps_samples : max_ii(1, int(ceilf(fps_target)));

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
    scene->fps_info = MEM_callocN(sizeof(ScreenFrameRateInfo) + (sizeof(float) * times_fps_num),
                                  __func__);
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
    fpsi->times_fps[fpsi->times_fps_index] = float(1.0 / (fpsi->time_prev - fpsi->time_curr));
    fpsi->times_fps_index++;
    if (fpsi->times_fps_index > fpsi->times_fps_num_set) {
      fpsi->times_fps_num_set = fpsi->times_fps_index;
    }
    BLI_assert(fpsi->times_fps_num_set > 0);
    float fps = 0.0f;
    for (int i = 0; i < fpsi->times_fps_num_set; i++) {
      fps += fpsi->times_fps[i];
    }
    fpsi->fps_average = fps / float(fpsi->times_fps_num_set);
  }

  *r_fps_target = fpsi->fps_target;
  return fpsi->fps_average;
}
