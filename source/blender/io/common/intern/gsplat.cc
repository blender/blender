/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "IO_gsplat.hh"

#include <string>

#include "DNA_pointcloud_types.h"

namespace blender::io::gsplat {

GsplatMutableAttributeAccessor::GsplatMutableAttributeAccessor(PointCloud &point_cloud,
                                                               const int sh_degrees)
    : point_cloud_(point_cloud), attributes_(point_cloud.attributes_for_write())
{
  radiance_base_attr_ = attributes_.lookup_or_add_for_write_span<float4>("radiance:base",
                                                                         bke::AttrDomain::Point);
  scales_attr_ = attributes_.lookup_or_add_for_write_span<float3>("scale", bke::AttrDomain::Point);
  rotations_attr_ = attributes_.lookup_or_add_for_write_span<math::Quaternion>(
      "rotation", bke::AttrDomain::Point);

  for (int degree = 1; degree <= sh_degrees; ++degree) {
    const int num_harmonics = (2 * degree + 1);
    for (int harmonic = 0; harmonic < num_harmonics; ++harmonic) {
      sh_attrs_.append_as(attributes_.lookup_or_add_for_write_span<float3>(
          "radiance:sh_" + std::to_string(sh_attrs_.size()), bke::AttrDomain::Point));
      sh_spans_for_write_.append(sh_attrs_[sh_attrs_.size() - 1].span);
    }
  }
}

MutableSpan<float3> GsplatMutableAttributeAccessor::positions_for_write()
{
  return point_cloud_.positions_for_write();
}

MutableSpan<float4> GsplatMutableAttributeAccessor::radiance_base_for_write()
{
  return radiance_base_attr_.span;
}

MutableSpan<float3> GsplatMutableAttributeAccessor::scales_for_write()
{
  return scales_attr_.span;
}

MutableSpan<math::Quaternion> GsplatMutableAttributeAccessor::rotations_for_write()
{
  return rotations_attr_.span;
}

Span<MutableSpan<float3>> GsplatMutableAttributeAccessor::sh_for_write()
{
  return sh_spans_for_write_;
}

void GsplatMutableAttributeAccessor::finish()
{
  radiance_base_attr_.finish();
  scales_attr_.finish();
  rotations_attr_.finish();
  for (bke::SpanAttributeWriter<float3> &sh_attr : sh_attrs_) {
    sh_attr.finish();
  }
}

}  // namespace blender::io::gsplat
