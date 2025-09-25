/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ed
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"

#include "BKE_brush.hh"

#include "BLI_math_vector.h"

#include "paint_intern.hh"

namespace blender::ed::sculpt_paint {

constexpr int AntiAliasingSamplesPerTexelAxisMin = 3;
constexpr int AntiAliasingSamplesPerTexelAxisMax = 16;
/**
 * \brief Number of samples to use between 0..1.
 */
constexpr int CurveSamplesBaseLen = 1024;
/**
 * \brief Number of samples to store in the cache.
 *
 * M_SQRT2 is used as brushes are circles and the curve_mask is square.
 * + 1 to fix floating rounding issues.
 */
constexpr int CurveSamplesLen = M_SQRT2 * CurveSamplesBaseLen + 1;

static int aa_samples_per_texel_axis(const Brush *brush, const float radius)
{
  int aa_samples = 1.0f / (radius * 0.20f);
  if (brush->sampling_flag & BRUSH_PAINT_ANTIALIASING) {
    aa_samples = clamp_i(
        aa_samples, AntiAliasingSamplesPerTexelAxisMin, AntiAliasingSamplesPerTexelAxisMax);
  }
  else {
    aa_samples = 1;
  }
  return aa_samples;
}

/* create a mask with the falloff strength */
static void update_curve_mask(CurveMaskCache *curve_mask_cache,
                              const Brush *brush,
                              const int diameter,
                              const float radius,
                              const float cursor_position[2])
{
  BLI_assert(curve_mask_cache->curve_mask != nullptr);
  int offset = int(floorf(diameter / 2.0f));
  float clamped_radius = max_ff(radius, 0.5f);

  ushort *m = curve_mask_cache->curve_mask;

  const int aa_samples = aa_samples_per_texel_axis(brush, radius);
  const float aa_offset = 1.0f / (2.0f * float(aa_samples));
  const float aa_step = 1.0f / float(aa_samples);

  float bpos[2];
  bpos[0] = cursor_position[0] - floorf(cursor_position[0]) + offset;
  bpos[1] = cursor_position[1] - floorf(cursor_position[1]) + offset;

  float weight_factor = 65535.0f / float(aa_samples * aa_samples);

  if (aa_samples == 1) {
    /* When AA is disabled, snap the cursor to either the corners or centers of the pixels,
     * depending on if the diameter is even or odd, respectively.*/

    if (int(clamped_radius * 2) % 2 == 0) {
      bpos[0] = roundf(bpos[0]);
      bpos[1] = roundf(bpos[1]);
    }
    else {
      bpos[0] = floorf(bpos[0]) + 0.5f;
      bpos[1] = floorf(bpos[1]) + 0.5f;
    }
  }

  for (int y = 0; y < diameter; y++) {
    for (int x = 0; x < diameter; x++, m++) {
      float pixel_xy[2];
      pixel_xy[0] = float(x) + aa_offset;
      float total_weight = 0;

      for (int i = 0; i < aa_samples; i++) {
        pixel_xy[1] = float(y) + aa_offset;
        for (int j = 0; j < aa_samples; j++) {
          const float len = len_v2v2(pixel_xy, bpos);
          const int sample_index = min_ii((len / clamped_radius) * CurveSamplesBaseLen,
                                          CurveSamplesLen - 1);
          const float sample_weight = curve_mask_cache->sampled_curve[sample_index];

          total_weight += sample_weight;

          pixel_xy[1] += aa_step;
        }
        pixel_xy[0] += aa_step;
      }
      *m = ushort(total_weight * weight_factor);
    }
  }
}

static bool is_sampled_curve_valid(const CurveMaskCache *curve_mask_cache, const Brush *brush)
{
  if (curve_mask_cache->sampled_curve == nullptr) {
    return false;
  }
  return curve_mask_cache->last_curve_timestamp ==
         brush->curve_distance_falloff->changed_timestamp;
}

static void sampled_curve_free(CurveMaskCache *curve_mask_cache)
{
  MEM_SAFE_FREE(curve_mask_cache->sampled_curve);
  curve_mask_cache->last_curve_timestamp = 0;
}

static void update_sampled_curve(CurveMaskCache *curve_mask_cache, const Brush *brush)
{
  if (curve_mask_cache->sampled_curve == nullptr) {
    curve_mask_cache->sampled_curve = MEM_malloc_arrayN<float>(CurveSamplesLen, __func__);
  }

  for (int i = 0; i < CurveSamplesLen; i++) {
    const float len = i / float(CurveSamplesBaseLen);
    const float sample_weight = BKE_brush_curve_strength_clamped(brush, len, 1.0f);
    curve_mask_cache->sampled_curve[i] = sample_weight;
  }
  curve_mask_cache->last_curve_timestamp = brush->curve_distance_falloff->changed_timestamp;
}

static size_t diameter_to_curve_mask_size(const int diameter)
{
  return diameter * diameter * sizeof(ushort);
}

static bool is_curve_mask_size_valid(const CurveMaskCache *curve_mask_cache, const int diameter)
{
  return curve_mask_cache->curve_mask_size == diameter_to_curve_mask_size(diameter);
}

static void curve_mask_free(CurveMaskCache *curve_mask_cache)
{
  curve_mask_cache->curve_mask_size = 0;
  MEM_SAFE_FREE(curve_mask_cache->curve_mask);
}

static void curve_mask_allocate(CurveMaskCache *curve_mask_cache, const int diameter)
{
  const size_t curve_mask_size = diameter_to_curve_mask_size(diameter);
  curve_mask_cache->curve_mask = static_cast<ushort *>(MEM_mallocN(curve_mask_size, __func__));
  curve_mask_cache->curve_mask_size = curve_mask_size;
}

}  // namespace blender::ed::sculpt_paint

using namespace blender::ed::sculpt_paint;

void paint_curve_mask_cache_free_data(CurveMaskCache *curve_mask_cache)
{
  sampled_curve_free(curve_mask_cache);
  curve_mask_free(curve_mask_cache);
}

void paint_curve_mask_cache_update(CurveMaskCache *curve_mask_cache,
                                   const Brush *brush,
                                   const int diameter,
                                   const float radius,
                                   const float cursor_position[2])
{
  if (!is_sampled_curve_valid(curve_mask_cache, brush)) {
    update_sampled_curve(curve_mask_cache, brush);
  }

  if (!is_curve_mask_size_valid(curve_mask_cache, diameter)) {
    curve_mask_free(curve_mask_cache);
    curve_mask_allocate(curve_mask_cache, diameter);
  }
  update_curve_mask(curve_mask_cache, brush, diameter, radius, cursor_position);
}
