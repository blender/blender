/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscene
 *
 * Define `ED_scene_fps_*` functions.
 */

#include <algorithm>
#include <cmath>

#include "BLI_math_base.h"

#include "DNA_scene_types.h"

#include "ED_scene.hh"

#include "MEM_guardedalloc.h"

namespace blender {

/**
 * Ring buffer of recent frame timestamps; FPS is `(samples - 1) / (newest - oldest)`.
 *
 * \note Stores timestamps rather than per-sample rates on purpose: averaging `1/interval`
 * is biased by short intervals and spikes during uneven timer cadence.
 */
struct ScreenFrameRateInfo {
  /** When the target FPS is not a whole number. */
  bool fps_target_is_fractional;

  /** The target FPS, use to reset on change. */
  float fps_target;
  /** Final result, ignore when -1.0. */
  float fps_average;

  int times_index;
  int times_num;
  /** Number of `times` set, never exceeds `times_num`. */
  int times_num_set;

  /** Over-allocated, containing `times_num` elements. */
  double times[0];
};

void ED_scene_fps_average_clear(Scene *scene)
{
  if (scene->fps_info == nullptr) {
    return;
  }
  MEM_delete(static_cast<ScreenFrameRateInfo *>(scene->fps_info));
  scene->fps_info = nullptr;
}

void ED_scene_fps_average_accumulate(Scene *scene, const short fps_samples, const double ltime)
{
  const float fps_target = float(scene->frames_per_second());
  /* Averaging intervals needs +1 timestamps.
   * Needed because the intervals are computed *between* the time-steps. */
  const int times_num = ((fps_samples > 0) ? fps_samples : std::max(1, int(ceilf(fps_target)))) +
                        1;

  ScreenFrameRateInfo *fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
  if (fpsi) {
    /* Reset when the target FPS changes.
     * Needed redraw times from when a different FPS was set do not contribute
     * to an average that is over/under the new target. */
    if ((fpsi->fps_target != fps_target) || (fpsi->times_num != times_num)) {
      MEM_delete(fpsi);
      fpsi = nullptr;
      scene->fps_info = nullptr;
    }
  }

  /* If there isn't any info, initialize it first. */
  if (fpsi == nullptr) {
    scene->fps_info = MEM_new_zeroed(sizeof(ScreenFrameRateInfo) +
                                         (sizeof(ScreenFrameRateInfo::times[0]) * times_num),
                                     __func__);
    fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
    fpsi->fps_target = fps_target;
    fpsi->times_num = times_num;

    /* Use 100 for 2 decimal places (currently used for FPS display), could be configurable. */
    const double decimal_places = 100.0;
    fpsi->fps_target_is_fractional = std::round(fps_target) !=
                                     (std::round(fps_target * decimal_places) / decimal_places);
  }

  fpsi->times[fpsi->times_index] = ltime;
  fpsi->times_index = (fpsi->times_index + 1) % fpsi->times_num;
  if (fpsi->times_num_set < fpsi->times_num) {
    fpsi->times_num_set++;
  }

  /* Mark as outdated. */
  fpsi->fps_average = -1.0f;
}

bool ED_scene_fps_average_calc(const Scene *scene, SceneFPS_State *r_state)
{
  ScreenFrameRateInfo *fpsi = static_cast<ScreenFrameRateInfo *>(scene->fps_info);
  if (fpsi == nullptr) {
    return false;
  }

  /* Need at least two timestamps to define an interval. */
  if (fpsi->times_num_set < 2) [[unlikely]] {
    return false;
  }

  if (fpsi->fps_average == -1.0f) {
    const int newest_index = ((fpsi->times_index - 1) + fpsi->times_num) % fpsi->times_num;
    const int oldest_index = (fpsi->times_num_set < fpsi->times_num) ? 0 : fpsi->times_index;
    const double delta = fpsi->times[newest_index] - fpsi->times[oldest_index];
    BLI_assert(delta >= 0.0);
    fpsi->fps_average = (delta > 0.0) ? float(double(fpsi->times_num_set - 1) / delta) : 0.0f;
  }

  r_state->fps_average = fpsi->fps_average;
  r_state->fps_target = fpsi->fps_target;
  r_state->fps_target_is_fractional = fpsi->fps_target_is_fractional;
  return true;
}

}  // namespace blender
