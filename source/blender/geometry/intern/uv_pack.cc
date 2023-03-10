/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include "GEO_uv_pack.hh"

#include "BLI_array.hh"
#include "BLI_boxpack_2d.h"
#include "BLI_convexhull_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

namespace blender::geometry {

/* Compact representation for AABB packers. */
class UVAABBIsland {
 public:
  float2 uv_diagonal;
  float2 uv_placement;
  int64_t index;
};

/**
 * Pack AABB islands using the "Alpaca" strategy, with no rotation.
 *
 * Each box is packed into an "L" shaped region, gradually filling up space.
 * "Alpaca" is a pun, as it's pronounced the same as "L-Packer" in English.
 *
 * In theory, alpaca_turbo should be the fastest non-trivial packer, hence the "turbo" suffix.
 *
 * Technically, the algorithm here is only `O(n)`, In practice, to get reasonable results,
 * the input must be pre-sorted, which costs an additional `O(nlogn)` time complexity.
 */
static void pack_islands_alpaca_turbo(const Span<UVAABBIsland *> islands,
                                      float *r_max_u,
                                      float *r_max_v)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = *r_max_u;
  float next_v1 = *r_max_v;
  bool zigzag = next_u1 < next_v1; /* Horizontal or Vertical strip? */

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order. */
  for (UVAABBIsland *island : islands) {
    float dsm_u = island->uv_diagonal.x;
    float dsm_v = island->uv_diagonal.y;

    bool restart = false;
    if (zigzag) {
      restart = (next_v1 < v0 + dsm_v);
    }
    else {
      restart = (next_u1 < u0 + dsm_u);
    }
    if (restart) {
      /* We're at the end of a strip. Restart from U axis or V axis. */
      zigzag = next_u1 < next_v1;
      u0 = zigzag ? next_u1 : 0.0f;
      v0 = zigzag ? 0.0f : next_v1;
    }

    /* Place the island. */
    island->uv_placement.x = u0;
    island->uv_placement.y = v0;
    if (zigzag) {
      /* Move upwards. */
      v0 += dsm_v;
      next_u1 = max_ff(next_u1, u0 + dsm_u);
      next_v1 = max_ff(next_v1, v0);
    }
    else {
      /* Move sideways. */
      u0 += dsm_u;
      next_v1 = max_ff(next_v1, v0 + dsm_v);
      next_u1 = max_ff(next_u1, u0);
    }
  }

  /* Write back total pack AABB. */
  *r_max_u = next_u1;
  *r_max_v = next_v1;
}

static float pack_islands_scale_margin(const Span<PackIsland *> islands,
                                       BoxPack *box_array,
                                       const float scale,
                                       const float margin)
{
  /* #BLI_box_pack_2d produces layouts with high packing efficiency, but has `O(n^3)`
   * time complexity, causing poor performance if there are lots of islands. See: #102843.
   * #pack_islands_alpaca_turbo is designed to be the fastest packing method, `O(nlogn)`,
   * but has poor packing efficiency if the AABBs have a spread of sizes and aspect ratios.
   * Here, we merge the best properties of both packers into one combined packer.
   *
   * The free tuning parameter, `alpaca_cutoff` will determine how many islands are packed
   * using each method.
   *
   * The current strategy is:
   * - Sort islands in size order.
   * - Call #BLI_box_pack_2d on the first `alpaca_cutoff` islands.
   * - Call #pack_islands_alpaca_turbo on the remaining islands.
   * - Combine results.
   */

  /* First, copy information from our input into the AABB structure. */
  Array<UVAABBIsland *> aabbs(islands.size());
  for (const int64_t i : islands.index_range()) {
    PackIsland *pack_island = islands[i];
    UVAABBIsland *aabb = new UVAABBIsland();
    aabb->index = i;
    aabb->uv_diagonal.x = BLI_rctf_size_x(&pack_island->bounds_rect) * scale + 2 * margin;
    aabb->uv_diagonal.y = BLI_rctf_size_y(&pack_island->bounds_rect) * scale + 2 * margin;
    aabbs[i] = aabb;
  }

  /* Sort from "biggest" to "smallest". */
  std::stable_sort(aabbs.begin(), aabbs.end(), [](const UVAABBIsland *a, const UVAABBIsland *b) {
    /* Just choose the AABB with larger rectangular area. */
    return b->uv_diagonal.x * b->uv_diagonal.y < a->uv_diagonal.x * a->uv_diagonal.y;
  });

  /* Partition island_vector, largest will go to box_pack, the rest alpaca_turbo.
   * See discussion above for details. */
  const int64_t alpaca_cutoff = int64_t(1024); /* TODO: Tune constant. */
  int64_t max_box_pack = std::min(alpaca_cutoff, islands.size());

  /* Prepare for box_pack_2d. */
  for (const int64_t i : islands.index_range()) {
    UVAABBIsland *aabb = aabbs[i];
    BoxPack *box = &box_array[i];
    box->index = int(aabb->index);
    box->w = aabb->uv_diagonal.x;
    box->h = aabb->uv_diagonal.y;
  }

  /* Call box_pack_2d (slow for large N.) */
  float max_u = 0.0f;
  float max_v = 0.0f;
  BLI_box_pack_2d(box_array, int(max_box_pack), &max_u, &max_v);

  /* At this stage, `max_u` and `max_v` contain the box_pack UVs. */

  /* Call Alpaca. */
  pack_islands_alpaca_turbo(aabbs.as_span().drop_front(max_box_pack), &max_u, &max_v);

  /* Write back Alpaca UVs. */
  for (int64_t index = max_box_pack; index < aabbs.size(); index++) {
    UVAABBIsland *aabb = aabbs[index];
    BoxPack *box = &box_array[index];
    box->x = aabb->uv_placement.x;
    box->y = aabb->uv_placement.y;
  }

  /* Memory management. */
  for (int64_t i : aabbs.index_range()) {
    UVAABBIsland *aabb = aabbs[i];
    aabbs[i] = nullptr;
    delete aabb;
  }

  return std::max(max_u, max_v);
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

    scale = std::max(scale, min_scale_roundoff);

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

static BoxPack *pack_islands_box_array(const Span<PackIsland *> &island_vector,
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

void pack_islands(const Span<PackIsland *> &islands,
                  const UVPackIsland_Params &params,
                  float r_scale[2])
{
  BoxPack *box_array = pack_islands_box_array(islands, params, r_scale);

  for (int64_t i : islands.index_range()) {
    BoxPack *box = box_array + i;
    PackIsland *island = islands[box->index];
    island->pre_translate.x = box->x - island->bounds_rect.xmin;
    island->pre_translate.y = box->y - island->bounds_rect.ymin;
  }

  MEM_freeN(box_array);
}

}  // namespace blender::geometry
