/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_hash.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_mpq_types.hh"
#include "BLI_task.hh"

#include "PRF_profile.hh"

namespace blender::math {

template<>
isect_result<float2> isect_seg_seg(const float2 &v1,
                                   const float2 &v2,
                                   const float2 &v3,
                                   const float2 &v4)
{
  isect_result<float2> ans;
  float div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0f) {
    ans.lambda = 0.0f;
    ans.kind = isect_result<float2>::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    float mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;
    if (ans.lambda >= 0.0f && ans.lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
      if (ans.lambda == 0.0f || ans.lambda == 1.0f || mu == 0.0f || mu == 1.0f) {
        ans.kind = isect_result<float2>::LINE_LINE_EXACT;
      }
      else {
        ans.kind = isect_result<float2>::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = isect_result<float2>::LINE_LINE_NONE;
    }
  }
  return ans;
}

template<>
isect_result<double2> isect_seg_seg(const double2 &v1,
                                    const double2 &v2,
                                    const double2 &v3,
                                    const double2 &v4)
{
  isect_result<double2> ans;
  double div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0) {
    ans.lambda = 0.0;
    ans.kind = isect_result<double2>::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    double mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;
    if (ans.lambda >= 0.0 && ans.lambda <= 1.0 && mu >= 0.0 && mu <= 1.0) {
      if (ans.lambda == 0.0 || ans.lambda == 1.0 || mu == 0.0 || mu == 1.0) {
        ans.kind = isect_result<double2>::LINE_LINE_EXACT;
      }
      else {
        ans.kind = isect_result<double2>::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = isect_result<double2>::LINE_LINE_NONE;
    }
  }
  return ans;
}

#ifdef WITH_GMP
template<>
isect_result<mpq2> isect_seg_seg(const mpq2 &v1, const mpq2 &v2, const mpq2 &v3, const mpq2 &v4)
{
  isect_result<mpq2> ans;
  mpq_class div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (div == 0.0) {
    ans.lambda = 0.0;
    ans.kind = isect_result<mpq2>::LINE_LINE_COLINEAR;
  }
  else {
    ans.lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;
    /* Avoid dividing mu by div: it is expensive in multi-precision. */
    mpq_class mudiv = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1]));
    if (ans.lambda >= 0 && ans.lambda <= 1 &&
        ((div > 0 && mudiv >= 0 && mudiv <= div) || (div < 0 && mudiv <= 0 && mudiv >= div)))
    {
      if (ans.lambda == 0 || ans.lambda == 1 || mudiv == 0 || mudiv == div) {
        ans.kind = isect_result<mpq2>::LINE_LINE_EXACT;
      }
      else {
        ans.kind = isect_result<mpq2>::LINE_LINE_CROSS;
      }
    }
    else {
      ans.kind = isect_result<mpq2>::LINE_LINE_NONE;
    }
  }
  return ans;
}
#endif

#ifdef WITH_GMP

uint64_t hash_mpq_class(const mpq_class &value)
{
  /* TODO: better/faster implementation of this. */
  return get_default_hash(float(value.get_d()));
}

#endif

static void translate_points_range(float3 *data,
                                   const int64_t start,
                                   const int64_t size,
                                   const float3 translation)
{
  /* This is written such that auto-vectorization works. */
  const float tx = translation.x;
  const float ty = translation.y;
  const float tz = translation.z;
  float *f = reinterpret_cast<float *>(data + start);
  const int64_t n = size;
  for (const int64_t i : IndexRange(n)) {
    f[i * 3 + 0] += tx;
    f[i * 3 + 1] += ty;
    f[i * 3 + 2] += tz;
  }
}

void translate_points(MutableSpan<float3> points,
                      const float3 &translation,
                      const bool use_threading)
{
  if (math::is_zero(translation)) {
    return;
  }

  float3 *data = points.data();
  const float3 t = translation;
  const auto translate_range = [&](const IndexRange range) {
    PRF_scope(ProfileCategory::Default);
    translate_points_range(data, range.start(), range.size(), t);
  };

  if (use_threading) {
    threading::parallel_for(points.index_range(), 2048, translate_range);
  }
  else {
    translate_range(points.index_range());
  }
}

}  // namespace blender::math
