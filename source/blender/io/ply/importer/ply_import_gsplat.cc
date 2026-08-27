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

static Span<float> get_custom_attribute(const PlyData &data, const StringRefNull name)
{
  std::optional<Span<float>> attr = find_custom_attribute(data, name);
  BLI_assert(attr.has_value());
  return *attr;
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

static int degree_for_dim(const int dimension)
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

constexpr int dim_for_degree(const int degree)
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

PointCloud *convert_gsplat_ply_to_point_cloud(const PlyData &data,
                                              const PLYImportParams & /*params*/)
{
  if (!validate::size_fits_in_int(data.vertices.size())) {
    return BKE_pointcloud_new_nomain(0);
  }

  PointCloud *point_cloud = BKE_pointcloud_new_nomain(data.vertices.size());

  point_cloud->positions_for_write().copy_from(data.vertices);

  /* Radiance base attributes in the PLY (r, g, b stored as a DC component of SH), and opacity.
   * Despite the name it seems to be alpha (at least according to the conversion in SPZ. */
  const Span<float> ply_f_dc_0_attr = get_custom_attribute(data, "f_dc_0");
  const Span<float> ply_f_dc_1_attr = get_custom_attribute(data, "f_dc_1");
  const Span<float> ply_f_dc_2_attr = get_custom_attribute(data, "f_dc_2");
  const Span<float> ply_opacity_attr = get_custom_attribute(data, "opacity");

  /* Scale. */
  const Span<float> ply_scale_0_attr = get_custom_attribute(data, "scale_0");
  const Span<float> ply_scale_1_attr = get_custom_attribute(data, "scale_1");
  const Span<float> ply_scale_2_attr = get_custom_attribute(data, "scale_2");

  /* Rotation (w, x, y, z). */
  const Span<float> ply_rot_0_attr = get_custom_attribute(data, "rot_0");
  const Span<float> ply_rot_1_attr = get_custom_attribute(data, "rot_1");
  const Span<float> ply_rot_2_attr = get_custom_attribute(data, "rot_2");
  const Span<float> ply_rot_3_attr = get_custom_attribute(data, "rot_3");

  /* f_rest_<i> */
  const Vector<Span<float>> f_rest = get_rest_custom_attributes(data);
  const int sh_degree = (f_rest.size() % 3 == 0) ? degree_for_dim(f_rest.size() / 3) : 0;
  const int num_sh_dimensions = dim_for_degree(sh_degree);

  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, sh_degree);
  MutableSpan<float4> radiance_base = accessor.radiance_base_for_write();
  MutableSpan<float3> scale = accessor.scales_for_write();
  MutableSpan<math::Quaternion> rotation = accessor.rotations_for_write();
  Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  for (int i = 0; i < data.vertices.size(); i++) {
    radiance_base[i] = float4(
        ply_f_dc_0_attr[i], ply_f_dc_1_attr[i], ply_f_dc_2_attr[i], ply_opacity_attr[i]);
    scale[i] = float3(ply_scale_0_attr[i], ply_scale_1_attr[i], ply_scale_2_attr[i]);
    rotation[i] = math::Quaternion(
        ply_rot_0_attr[i], ply_rot_1_attr[i], ply_rot_2_attr[i], ply_rot_3_attr[i]);

    radiance_base[i].w = gsplat::OriginalActivationFunctions::decode_opacity(radiance_base[i].w);
    scale[i] = gsplat::OriginalActivationFunctions::decode_scale(scale[i]);

    for (int dimension = 0; dimension < num_sh_dimensions; dimension++) {
      sh_attrs[dimension][i] = float3(f_rest[dimension][i],
                                      f_rest[dimension + num_sh_dimensions][i],
                                      f_rest[dimension + 2 * num_sh_dimensions][i]);
    }
  }

  // TODO(sergey): Handle conversion denoted in the params.

  accessor.finish();

  point_cloud->render_as = PT_RENDER_AS_SPLATS;

  // TODO(sergey): Handle params.import_attributes.

  return point_cloud;
}

}  // namespace blender::io::ply
