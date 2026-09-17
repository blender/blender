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
#include "BKE_report.hh"

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

/* Spherical harmonics up to degree 4 are supported, matching SH_MAX_DEGREE of the SPZ library.
 * This corresponds to 24 coefficients per channel, or 72 f_rest_<i> properties. */
constexpr int MAX_SH_DEGREE = 4;
constexpr int MAX_REST_ATTRIBUTES = 24 * 3;

/* Get f_rest_<i> attributes from the PLY data.
 * The result is indexed by <i>. */
static Vector<Span<float>> get_rest_custom_attributes(const PlyData &data)
{
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

  /* f_rest_<i>
   *
   * The f_rest layout is channel-major: all per-channel coefficients of R first, then all of G,
   * then all of B. The offset between channels is therefore the file's own per-channel count,
   * independent of how many of those coefficients end up imported. */
  const Vector<Span<float>> f_rest = get_rest_custom_attributes(data);
  int num_file_coefficients = int(f_rest.size() / 3);
  int sh_degree = 0;

  if (f_rest.size() == MAX_REST_ATTRIBUTES &&
      find_custom_attribute(data, "f_rest_" + std::to_string(MAX_REST_ATTRIBUTES)))
  {
    /* More coefficients than the maximum supported degree: the channel offsets of such a file do
     * not line up with any supported layout, so no coefficients can be imported. */
    BKE_reportf(params.reports,
                RPT_WARNING,
                "PLY Importer: Spherical harmonics degree above the supported %d, spherical "
                "harmonics are not imported",
                MAX_SH_DEGREE);
    num_file_coefficients = 0;
  }
  else if (f_rest.size() % 3 != 0) {
    BKE_reportf(params.reports,
                RPT_WARNING,
                "PLY Importer: Unexpected number of f_rest properties %d, spherical harmonics "
                "are not imported",
                int(f_rest.size()));
    num_file_coefficients = 0;
  }
  else {
    sh_degree = gsplat::degree_for_dimension(num_file_coefficients);
    if (gsplat::dimension_for_degree(sh_degree) != num_file_coefficients) {
      BKE_reportf(params.reports,
                  RPT_WARNING,
                  "PLY Importer: Incomplete spherical harmonics band, importing %d of %d "
                  "coefficients",
                  gsplat::dimension_for_degree(sh_degree),
                  num_file_coefficients);
    }
  }

  const int num_sh_dimensions = gsplat::dimension_for_degree(sh_degree);

  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, sh_degree);
  MutableSpan<float4> radiance_base = accessor.radiance_base_for_write();
  MutableSpan<float3> scale = accessor.scales_for_write();
  MutableSpan<math::Quaternion> rotation = accessor.rotations_for_write();
  Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  for (int i = 0; i < data.vertices.size(); i++) {
    radiance_base[i] = float4(ply_f_dc[0][i], ply_f_dc[1][i], ply_f_dc[2][i], ply_opacity[i]);
    scale[i] = float3(ply_scale[0][i], ply_scale[1][i], ply_scale[2][i]);
    rotation[i] = math::normalize(
        math::Quaternion(ply_rot[0][i], ply_rot[1][i], ply_rot[2][i], ply_rot[3][i]));

    radiance_base[i].w = gsplat::OriginalActivationFunctions::decode_opacity(radiance_base[i].w);
    scale[i] = gsplat::OriginalActivationFunctions::decode_scale(scale[i]);

    for (int dimension = 0; dimension < num_sh_dimensions; dimension++) {
      sh_attrs[dimension][i] = float3(f_rest[dimension][i],
                                      f_rest[dimension + num_file_coefficients][i],
                                      f_rest[dimension + 2 * num_file_coefficients][i]);
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
