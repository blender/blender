/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_math_vector.hh"
#include "BLI_multi_value_map.hh"
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
  int resolution_;
  MultiValueMap<int2, int> looptris_by_cell_;

 public:
  ReverseUVSampler(const Span<float2> uv_map, const Span<MLoopTri> looptris);

  enum class ResultType {
    None,
    Ok,
    Multiple,
  };

  struct Result {
    ResultType type = ResultType::None;
    int looptri_index = -1;
    float3 bary_weights;
  };

  Result sample(const float2 &query_uv) const;
  void sample_many(Span<float2> query_uvs, MutableSpan<Result> r_results) const;
};

}  // namespace blender::geometry
