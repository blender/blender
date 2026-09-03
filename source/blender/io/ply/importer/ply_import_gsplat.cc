/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_import_gsplat.hh"

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_pointcloud.hh"

#include "IO_gsplat.hh"
#include "IO_validate.hh"

namespace blender::io::ply {

static std::optional<Span<float>> find_custom_attribute(const PlyData &data,
                                                        const StringRefNull name)
{
  for (const PlyCustomAttribute &attr : data.vertex_custom_attr) {
    if (attr.name == name) {
      return attr.data;
    }
  }
  return std::nullopt;
}

/* Get f_rest_<i> attributes from the PLY data.
 * The result is indexed by <i>. */
static Vector<Span<float>> get_rest_custom_attributes(const PlyData &data)
{
  /* Matches SPZ 3.0.0 which defines SH_MAX_COEFFS as 24 (corresponding to the maximum SH degrees
   * of 4). */
  constexpr int MAX_REST_ATTRIBUTES = 72;

  Vector<Span<float>> result;
  for (int i = 0; i < MAX_REST_ATTRIBUTES; i++) {
    std::optional<Span<float>> attr = find_custom_attribute(data, "f_rest_" + std::to_string(i));
    if (!attr.has_value()) {
      break;
    }
    result.append(*attr);
  }
  return result;
}

static bool is_builtin_gsplat_attribute(StringRefNull name)
{
  if (name == "f_dc_0" || name == "f_dc_1" || name == "f_dc_2") {
    return true;
  }

  if (name == "opacity") {
    return true;
  }

  if (name == "scale_0" || name == "scale_1" || name == "scale_2") {
    return true;
  }

  if (name == "rot_0" || name == "rot_1" || name == "rot_2" || name == "rot_3") {
    return true;
  }

  if (name.startswith("f_rest_")) {
    return true;
  }

  return false;
}

PointCloud *convert_gsplat_ply_to_point_cloud(const PlyData &data, const PLYImportParams &params)
{
  if (!validate::size_fits_in_int(data.vertices.size())) {
    return BKE_pointcloud_new_nomain(PointCloudType::GSplat, 0);
  }

  /* Radiance base attributes in the PLY (r, g, b stored as a DC component of SH), and opacity.
   * Despite the name it seems to be alpha (at least according to the conversion in SPZ. */
  const std::optional<Span<float>> ply_f_dc_0_attr = find_custom_attribute(data, "f_dc_0");
  const std::optional<Span<float>> ply_f_dc_1_attr = find_custom_attribute(data, "f_dc_1");
  const std::optional<Span<float>> ply_f_dc_2_attr = find_custom_attribute(data, "f_dc_2");
  if (!ply_f_dc_0_attr || !ply_f_dc_1_attr || !ply_f_dc_2_attr) {
    return nullptr;
  }
  Span<float> ply_f_dc[3] = {*ply_f_dc_0_attr, *ply_f_dc_1_attr, *ply_f_dc_2_attr};

  const std::optional<Span<float>> ply_opacity_attr = find_custom_attribute(data, "opacity");
  if (!ply_opacity_attr) {
    return nullptr;
  }
  Span<float> ply_opacity = *ply_opacity_attr;

  /* Scale. */
  const std::optional<Span<float>> ply_scale_0_attr = find_custom_attribute(data, "scale_0");
  const std::optional<Span<float>> ply_scale_1_attr = find_custom_attribute(data, "scale_1");
  const std::optional<Span<float>> ply_scale_2_attr = find_custom_attribute(data, "scale_2");
  if (!ply_scale_0_attr || !ply_scale_1_attr || !ply_scale_2_attr) {
    return nullptr;
  }
  const Span<float> ply_scale[3] = {*ply_scale_0_attr, *ply_scale_1_attr, *ply_scale_2_attr};

  /* Rotation (w, x, y, z). */
  const std::optional<Span<float>> ply_rot_0_attr = find_custom_attribute(data, "rot_0");
  const std::optional<Span<float>> ply_rot_1_attr = find_custom_attribute(data, "rot_1");
  const std::optional<Span<float>> ply_rot_2_attr = find_custom_attribute(data, "rot_2");
  const std::optional<Span<float>> ply_rot_3_attr = find_custom_attribute(data, "rot_3");
  if (!ply_rot_0_attr || !ply_rot_1_attr || !ply_rot_2_attr || !ply_rot_3_attr) {
    return nullptr;
  }
  Span<float> ply_rot[4] = {*ply_rot_0_attr, *ply_rot_1_attr, *ply_rot_2_attr, *ply_rot_3_attr};

  PointCloud *point_cloud = BKE_pointcloud_new_nomain(PointCloudType::GSplat,
                                                      data.vertices.size());

  point_cloud->positions_for_write().copy_from(data.vertices);

  /* f_rest_<i> */
  const Vector<Span<float>> f_rest = get_rest_custom_attributes(data);
  const int sh_degree = (f_rest.size() % 3 == 0) ?
                            gsplat::degree_for_dimension(f_rest.size() / 3) :
                            0;
  const int num_sh_dimensions = gsplat::dimension_for_degree(sh_degree);

  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, sh_degree);
  MutableSpan<float4> radiance_base = accessor.radiance_base_for_write();
  MutableSpan<float3> scale = accessor.scales_for_write();
  MutableSpan<math::Quaternion> rotation = accessor.rotations_for_write();
  Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  for (int i = 0; i < data.vertices.size(); i++) {
    radiance_base[i] = float4(ply_f_dc[0][i], ply_f_dc[1][i], ply_f_dc[2][i], ply_opacity[i]);
    scale[i] = float3(ply_scale[0][i], ply_scale[1][i], ply_scale[2][i]);
    rotation[i] = math::Quaternion(ply_rot[0][i], ply_rot[1][i], ply_rot[2][i], ply_rot[3][i]);

    radiance_base[i].w = gsplat::OriginalActivationFunctions::decode_opacity(radiance_base[i].w);
    scale[i] = gsplat::OriginalActivationFunctions::decode_scale(scale[i]);

    for (int dimension = 0; dimension < num_sh_dimensions; dimension++) {
      sh_attrs[dimension][i] = float3(f_rest[dimension][i],
                                      f_rest[dimension + num_sh_dimensions][i],
                                      f_rest[dimension + 2 * num_sh_dimensions][i]);
    }
  }

  /* TODO(sergey): Handle coordinate system conversion denoted in the params. */
  /* For "regular" import (via UI, using File -> Import or by dropping PLY file onto the viewport)
   * this is handled via object matrix in `importer_main()`. However, some code paths might use
   * this code directly: for example, geometry nodes. Currently geometry nodes do not provide axis
   * control for import nodes, so handling coordinate system conversion here is not required. */

  accessor.finish();

  if (params.import_attributes) {
    bke::MutableAttributeAccessor attributes = point_cloud->attributes_for_write();
    for (const PlyCustomAttribute &attr : data.vertex_custom_attr) {
      if (is_builtin_gsplat_attribute(attr.name)) {
        continue;
      }
      attributes.add<float>(attr.name,
                            bke::AttrDomain::Point,
                            bke::AttributeInitVArray(VArray<float>::from_span(attr.data)));
    }
  }

  return point_cloud;
}

}  // namespace blender::io::ply
