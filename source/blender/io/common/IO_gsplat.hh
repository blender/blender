/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_color_types.hh"
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
  bke::SpanAttributeWriter<ColorGeometry4f> colors_attr_;
  bke::SpanAttributeWriter<math::Quaternion> rotations_attr_;
  Vector<bke::SpanAttributeWriter<float3>> sh_attrs_;
  Vector<MutableSpan<float3>> sh_spans_for_write_;

 public:
  GsplatMutableAttributeAccessor(PointCloud &point_cloud, int sh_degrees);

  MutableSpan<float3> positions_for_write();
  MutableSpan<ColorGeometry4f> colors_for_write();
  MutableSpan<float3> scales_for_write();
  MutableSpan<math::Quaternion> rotations_for_write();
  Span<MutableSpan<float3>> sh_for_write();

  void finish();
};

}  // namespace io::gsplat
}  // namespace blender
