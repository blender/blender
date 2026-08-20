/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
