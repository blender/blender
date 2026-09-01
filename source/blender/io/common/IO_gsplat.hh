/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.hh"
#include "BLI_math_base.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"

namespace blender {

struct PointCloud;

namespace io::gsplat {

class GsplatMutableAttributeAccessor {
  PointCloud &point_cloud_;
  bke::MutableAttributeAccessor attributes_;

  bke::SpanAttributeWriter<float3> scales_attr_;
  bke::SpanAttributeWriter<float4> radiance_base_attr_;
  bke::SpanAttributeWriter<math::Quaternion> rotations_attr_;
  Vector<bke::SpanAttributeWriter<float3>> sh_attrs_;
  Vector<MutableSpan<float3>> sh_spans_for_write_;

 public:
  GsplatMutableAttributeAccessor(PointCloud &point_cloud, int sh_degrees);

  MutableSpan<float3> positions_for_write();
  MutableSpan<float4> radiance_base_for_write();
  MutableSpan<float3> scales_for_write();
  MutableSpan<math::Quaternion> rotations_for_write();
  Span<MutableSpan<float3>> sh_for_write();

  void finish();
};

inline int degree_for_dimension(const int dimension)
{
  if (dimension < 3) {
    return 0;
  }
  if (dimension < 8) {
    return 1;
  }
  if (dimension < 15) {
    return 2;
  }
  if (dimension < 24) {
    return 3;
  }
  return 4;
}

inline int dimension_for_degree(const int degree)
{
  switch (degree) {
    case 0:
      return 0;
    case 1:
      return 3;
    case 2:
      return 8;
    case 3:
      return 15;
    case 4:
      return 24;
    default:
      return 0;
  }
}

/* Get spherical harmonics for given point.
 * Essentially a slize of all elements of the sh_attrs at the given index.
 *
 * The sh_attrs are used read-only. The reason it is a MutableSpan is for the ease of integration
 * with the IO code that acquires attributes for write. */
inline void get_spherical_harmonics(const Span<MutableSpan<float3>> sh_attrs,
                                    const int point_index,
                                    const MutableSpan<float3> spherical_harmonics)
{
  BLI_assert(spherical_harmonics.size() == sh_attrs.size());
  for (const int64_t i : sh_attrs.index_range()) {
    spherical_harmonics[i] = sh_attrs[i][point_index];
  }
}

/* Set spherical harmonics attributes for the given point. */
inline void set_spherical_harmonics(const Span<MutableSpan<float3>> sh_attrs,
                                    const int point_index,
                                    const Span<float3> spherical_harmonics)
{
  BLI_assert(spherical_harmonics.size() == sh_attrs.size());
  for (const int k : sh_attrs.index_range()) {
    sh_attrs[k][point_index] = spherical_harmonics[k];
  }
}

/* Files that are saved using the original Gaussian Splatting paper are encoding opacity and scale
 * in a special way: they save values before an activation functions are applied for fitting. These
 * activation function needs to be applied in order to get an actually usable scale of the gaussian
 * and its opacity.
 *
 * This class encapsulates implementation of these functions. Formulas are from supplemental code
 * of the paper "Gaussian Point Splatting" (2026) by Joris Rijsdijk et. al.
 * See gaussian_loader.cu, recordToCPUGaussian(). */
struct OriginalActivationFunctions {
  static float decode_opacity(const float opacity)
  {
    return math::clamp(1.0f / (1.0f + math::exp(-opacity)), 0.0f, 1.0f);
  }
  static float3 decode_scale(const float3 scale)
  {
    return math::exp(scale);
  }
};

}  // namespace io::gsplat
}  // namespace blender
