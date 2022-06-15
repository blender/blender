/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_reverse_uv_sampler.hh"

#include "BLI_math_geom.h"

namespace blender::geometry {

ReverseUVSampler::ReverseUVSampler(const Span<float2> uv_map, const Span<MLoopTri> looptris)
    : uv_map_(uv_map), looptris_(looptris)
{
}

ReverseUVSampler::Result ReverseUVSampler::sample(const float2 &query_uv) const
{
  for (const MLoopTri &looptri : looptris_) {
    const float2 &uv0 = uv_map_[looptri.tri[0]];
    const float2 &uv1 = uv_map_[looptri.tri[1]];
    const float2 &uv2 = uv_map_[looptri.tri[2]];
    float3 bary_weights;
    if (!barycentric_coords_v2(uv0, uv1, uv2, query_uv, bary_weights)) {
      continue;
    }
    if (IN_RANGE_INCL(bary_weights.x, 0.0f, 1.0f) && IN_RANGE_INCL(bary_weights.y, 0.0f, 1.0f) &&
        IN_RANGE_INCL(bary_weights.z, 0.0f, 1.0f)) {
      return Result{ResultType::Ok, &looptri, bary_weights};
    }
  }
  return Result{};
}

}  // namespace blender::geometry
