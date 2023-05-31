/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_reverse_uv_sampler.hh"

#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

namespace blender::geometry {

static int2 uv_to_cell_key(const float2 &uv, const int resolution)
{
  return int2{uv * resolution};
}

ReverseUVSampler::ReverseUVSampler(const Span<float2> uv_map, const Span<MLoopTri> looptris)
    : uv_map_(uv_map), looptris_(looptris)
{
  resolution_ = std::max<int>(3, std::sqrt(looptris.size()) * 2);

  for (const int looptri_index : looptris.index_range()) {
    const MLoopTri &looptri = looptris[looptri_index];
    const float2 &uv_0 = uv_map_[looptri.tri[0]];
    const float2 &uv_1 = uv_map_[looptri.tri[1]];
    const float2 &uv_2 = uv_map_[looptri.tri[2]];

    const int2 key_0 = uv_to_cell_key(uv_0, resolution_);
    const int2 key_1 = uv_to_cell_key(uv_1, resolution_);
    const int2 key_2 = uv_to_cell_key(uv_2, resolution_);

    const int2 min_key = math::min(math::min(key_0, key_1), key_2);
    const int2 max_key = math::max(math::max(key_0, key_1), key_2);

    for (int key_x = min_key.x; key_x <= max_key.x; key_x++) {
      for (int key_y = min_key.y; key_y <= max_key.y; key_y++) {
        const int2 key{key_x, key_y};
        looptris_by_cell_.add(key, looptri_index);
      }
    }
  }
}

ReverseUVSampler::Result ReverseUVSampler::sample(const float2 &query_uv) const
{
  const int2 cell_key = uv_to_cell_key(query_uv, resolution_);
  const Span<int> looptri_indices = looptris_by_cell_.lookup(cell_key);

  float best_dist = FLT_MAX;
  float3 best_bary_weights;
  int best_looptri;

  /* The distance to an edge that is allowed to be inside or outside the triangle. Without this,
   * the lookup can fail for floating point accuracy reasons when the uv is almost exact on an
   * edge. */
  const float edge_epsilon = 0.00001f;

  for (const int looptri_index : looptri_indices) {
    const MLoopTri &looptri = looptris_[looptri_index];
    const float2 &uv_0 = uv_map_[looptri.tri[0]];
    const float2 &uv_1 = uv_map_[looptri.tri[1]];
    const float2 &uv_2 = uv_map_[looptri.tri[2]];
    float3 bary_weights;
    if (!barycentric_coords_v2(uv_0, uv_1, uv_2, query_uv, bary_weights)) {
      continue;
    }

    /* If #query_uv is in the triangle, the distance is <= 0. Otherwise, the larger the distance,
     * the further away the uv is from the triangle. */
    const float x_dist = std::max(-bary_weights.x, bary_weights.x - 1.0f);
    const float y_dist = std::max(-bary_weights.y, bary_weights.y - 1.0f);
    const float z_dist = std::max(-bary_weights.z, bary_weights.z - 1.0f);
    const float dist = MAX3(x_dist, y_dist, z_dist);

    if (dist <= 0.0f && best_dist <= 0.0f) {
      const float worse_dist = std::max(dist, best_dist);
      /* Allow ignoring multiple triangle intersections if the uv is almost exactly on an edge. */
      if (worse_dist < -edge_epsilon) {
        /* The uv sample is in multiple triangles. */
        return Result{ResultType::Multiple};
      }
    }

    if (dist < best_dist) {
      best_dist = dist;
      best_bary_weights = bary_weights;
      best_looptri = looptri_index;
    }
  }

  /* Allow using the closest (but not intersecting) triangle if the uv is almost exactly on an
   * edge. */
  if (best_dist < edge_epsilon) {
    return Result{ResultType::Ok, best_looptri, math::clamp(best_bary_weights, 0.0f, 1.0f)};
  }

  return Result{};
}

void ReverseUVSampler::sample_many(const Span<float2> query_uvs,
                                   MutableSpan<Result> r_results) const
{
  BLI_assert(query_uvs.size() == r_results.size());
  threading::parallel_for(query_uvs.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      r_results[i] = this->sample(query_uvs[i]);
    }
  });
}

}  // namespace blender::geometry
