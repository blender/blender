/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_math_vector.hh"
#include "BLI_span.hh"

#include "DNA_meshdata_types.h"

namespace blender::geometry {

/**
 * Can find the polygon/triangle that maps to a specific uv coordinate.
 *
 * \note this uses a trivial implementation currently that has to be replaced.
 */
class ReverseUVSampler {
 private:
  const Span<float2> uv_map_;
  const Span<MLoopTri> looptris_;

 public:
  ReverseUVSampler(const Span<float2> uv_map, const Span<MLoopTri> looptris);

  enum class ResultType {
    None,
    Ok,
    Multiple,
  };

  struct Result {
    ResultType type = ResultType::None;
    const MLoopTri *looptri = nullptr;
    float3 bary_weights;
  };

  Result sample(const float2 &query_uv) const;
};

}  // namespace blender::geometry
