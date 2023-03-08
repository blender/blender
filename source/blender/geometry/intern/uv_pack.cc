/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include "GEO_uv_pack.hh"

#include "BLI_boxpack_2d.h"
#include "BLI_convexhull_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

namespace blender::geometry {

static float pack_islands_scale_margin(const Span<PackIsland *> &island_vector,
                                       BoxPack *box_array,
                                       const float scale,
                                       const float margin)
{
  for (const int64_t index : island_vector.index_range()) {
    PackIsland *island = island_vector[index];
    BoxPack *box = &box_array[index];
    box->index = int(index);
    box->w = BLI_rctf_size_x(&island->bounds_rect) * scale + 2 * margin;
    box->h = BLI_rctf_size_y(&island->bounds_rect) * scale + 2 * margin;
  }
  float max_u, max_v;
  BLI_box_pack_2d(box_array, int(island_vector.size()), &max_u, &max_v);
  return max_ff(max_u, max_v);
}

static float pack_islands_margin_fraction(const Span<PackIsland *> &island_vector,
                                          BoxPack *box_array,
                                          const float margin_fraction)
{
  /*
   * Root finding using a combined search / modified-secant method.
   * First, use a robust search procedure to bracket the root within a factor of 10.
   * Then, use a modified-secant method to converge.
   *
   * This is a specialized solver using domain knowledge to accelerate convergence. */

  float scale_low = 0.0f;
  float value_low = 0.0f;
  float scale_high = 0.0f;
  float value_high = 0.0f;
  float scale_last = 0.0f;

  /* Scaling smaller than `min_scale_roundoff` is unlikely to fit and
   * will destroy information in existing UVs. */
  float min_scale_roundoff = 1e-5f;

  /* Certain inputs might have poor convergence properties.
   * Use `max_iteration` to prevent an infinite loop. */
  int max_iteration = 25;
  for (int iteration = 0; iteration < max_iteration; iteration++) {
    float scale = 1.0f;

    if (iteration == 0) {
      BLI_assert(iteration == 0);
      BLI_assert(scale == 1.0f);
      BLI_assert(scale_low == 0.0f);
      BLI_assert(scale_high == 0.0f);
    }
    else if (scale_low == 0.0f) {
      BLI_assert(scale_high > 0.0f);
      /* Search mode, shrink layout until we can find a scale that fits. */
      scale = scale_high * 0.1f;
    }
    else if (scale_high == 0.0f) {
      BLI_assert(scale_low > 0.0f);
      /* Search mode, grow layout until we can find a scale that doesn't fit. */
      scale = scale_low * 10.0f;
    }
    else {
      /* Bracket mode, use modified secant method to find root. */
      BLI_assert(scale_low > 0.0f);
      BLI_assert(scale_high > 0.0f);
      BLI_assert(value_low <= 0.0f);
      BLI_assert(value_high >= 0.0f);
      if (scale_high < scale_low * 1.0001f) {
        /* Convergence. */
        break;
      }

      /* Secant method for area. */
      scale = (sqrtf(scale_low) * value_high - sqrtf(scale_high) * value_low) /
              (value_high - value_low);
      scale = scale * scale;

      if (iteration & 1) {
        /* Modified binary-search to improve robustness. */
        scale = sqrtf(scale * sqrtf(scale_low * scale_high));
      }
    }

    scale = max_ff(scale, min_scale_roundoff);

    /* Evaluate our `f`. */
    scale_last = scale;
    float max_uv = pack_islands_scale_margin(
        island_vector, box_array, scale_last, margin_fraction);
    float value = sqrtf(max_uv) - 1.0f;

    if (value <= 0.0f) {
      scale_low = scale;
      value_low = value;
    }
    else {
      scale_high = scale;
      value_high = value;
      if (scale == min_scale_roundoff) {
        /* Unable to pack without damaging UVs. */
        scale_low = scale;
        break;
      }
    }
  }

  const bool flush = true;
  if (flush) {
    /* Write back best pack as a side-effect. First get best pack. */
    if (scale_last != scale_low) {
      scale_last = scale_low;
      float max_uv = pack_islands_scale_margin(
          island_vector, box_array, scale_last, margin_fraction);
      UNUSED_VARS(max_uv);
      /* TODO (?): `if (max_uv < 1.0f) { scale_last /= max_uv; }` */
    }

    /* Then expand FaceIslands by the correct amount. */
    for (const int64_t index : island_vector.index_range()) {
      BoxPack *box = &box_array[index];
      box->x /= scale_last;
      box->y /= scale_last;
      PackIsland *island = island_vector[index];
      BLI_rctf_pad(
          &island->bounds_rect, margin_fraction / scale_last, margin_fraction / scale_last);
    }
  }
  return scale_last;
}

static float calc_margin_from_aabb_length_sum(const Span<PackIsland *> &island_vector,
                                              const UVPackIsland_Params &params)
{
  /* Logic matches behavior from #geometry::uv_parametrizer_pack.
   * Attempt to give predictable results not dependent on current UV scale by using
   * `aabb_length_sum` (was "`area`") to multiply the margin by the length (was "area"). */
  double aabb_length_sum = 0.0f;
  for (PackIsland *island : island_vector) {
    float w = BLI_rctf_size_x(&island->bounds_rect);
    float h = BLI_rctf_size_y(&island->bounds_rect);
    aabb_length_sum += sqrtf(w * h);
  }
  return params.margin * aabb_length_sum * 0.1f;
}

BoxPack *pack_islands(const Span<PackIsland *> &island_vector,
                      const UVPackIsland_Params &params,
                      float r_scale[2])
{
  BoxPack *box_array = static_cast<BoxPack *>(
      MEM_mallocN(sizeof(*box_array) * island_vector.size(), __func__));

  if (params.margin == 0.0f) {
    /* Special case for zero margin. Margin_method is ignored as all formulas give same result. */
    const float max_uv = pack_islands_scale_margin(island_vector, box_array, 1.0f, 0.0f);
    r_scale[0] = 1.0f / max_uv;
    r_scale[1] = r_scale[0];
    return box_array;
  }

  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION) {
    /* Uses a line search on scale. ~10x slower than other method. */
    const float scale = pack_islands_margin_fraction(island_vector, box_array, params.margin);
    r_scale[0] = scale;
    r_scale[1] = scale;
    /* pack_islands_margin_fraction will pad FaceIslands, return early. */
    return box_array;
  }

  float margin = params.margin;
  switch (params.margin_method) {
    case ED_UVPACK_MARGIN_ADD:    /* Default for Blender 2.8 and earlier. */
      break;                      /* Nothing to do. */
    case ED_UVPACK_MARGIN_SCALED: /* Default for Blender 3.3 and later. */
      margin = calc_margin_from_aabb_length_sum(island_vector, params);
      break;
    case ED_UVPACK_MARGIN_FRACTION: /* Added as an option in Blender 3.4. */
      BLI_assert_unreachable();     /* Handled above. */
      break;
    default:
      BLI_assert_unreachable();
  }

  const float max_uv = pack_islands_scale_margin(island_vector, box_array, 1.0f, margin);
  r_scale[0] = 1.0f / max_uv;
  r_scale[1] = r_scale[0];

  for (int index = 0; index < island_vector.size(); index++) {
    PackIsland *island = island_vector[index];
    BLI_rctf_pad(&island->bounds_rect, margin, margin);
  }
  return box_array;
}

}  // namespace blender::geometry
