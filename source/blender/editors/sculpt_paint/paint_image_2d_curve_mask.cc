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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup ed
 */

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"

#include "BKE_brush.h"

#include "paint_intern.h"

namespace blender::ed::sculpt_paint {

static int aa_samples_per_texel_axis(const Brush *brush, const float radius)
{
  int aa_samples = 1.0f / (radius * 0.20f);
  if (brush->sampling_flag & BRUSH_PAINT_ANTIALIASING) {
    aa_samples = clamp_i(aa_samples, 3, 16);
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
  int offset = (int)floorf(diameter / 2.0f);

  unsigned short *m = curve_mask_cache->curve_mask;

  const int aa_samples = aa_samples_per_texel_axis(brush, radius);
  const float aa_offset = 1.0f / (2.0f * (float)aa_samples);
  const float aa_step = 1.0f / (float)aa_samples;

  float bpos[2];
  bpos[0] = cursor_position[0] - floorf(cursor_position[0]) + offset - aa_offset;
  bpos[1] = cursor_position[1] - floorf(cursor_position[1]) + offset - aa_offset;

  float weight_factor = 65535.0f / (float)(aa_samples * aa_samples);

  for (int y = 0; y < diameter; y++) {
    for (int x = 0; x < diameter; x++, m++) {
      float total_weight = 0;
      for (int i = 0; i < aa_samples; i++) {
        for (int j = 0; j < aa_samples; j++) {
          float pixel_xy[2] = {x + (aa_step * i), y + (aa_step * j)};
          sub_v2_v2(pixel_xy, bpos);

          const float len = len_v2(pixel_xy);
          const float sample_weight = BKE_brush_curve_strength_clamped(brush, len, radius);
          total_weight += sample_weight;
        }
      }
      *m = (unsigned short)(total_weight * weight_factor);
    }
  }
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
  curve_mask_cache->curve_mask = static_cast<unsigned short *>(
      MEM_mallocN(curve_mask_size, __func__));
  curve_mask_cache->curve_mask_size = curve_mask_size;
}

}  // namespace blender::ed::sculpt_paint

using namespace blender::ed::sculpt_paint;

void paint_curve_mask_cache_free_data(CurveMaskCache *curve_mask_cache)
{
  curve_mask_free(curve_mask_cache);
}

void paint_curve_mask_cache_update(CurveMaskCache *curve_mask_cache,
                                   const Brush *brush,
                                   const int diameter,
                                   const float radius,
                                   const float cursor_position[2])
{
  if (!is_curve_mask_size_valid(curve_mask_cache, diameter)) {
    curve_mask_free(curve_mask_cache);
    curve_mask_allocate(curve_mask_cache, diameter);
  }
  update_curve_mask(curve_mask_cache, brush, diameter, radius, cursor_position);
}

