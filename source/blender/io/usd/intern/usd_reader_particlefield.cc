/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_particlefield.hh"

#include "IO_gsplat.hh"
#include "IO_validate.hh"

#include "BLI_span.hh"
#include "BLI_task.hh"

#include "BKE_geometry_set.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "CLG_log.h"

namespace blender::io::usd {

static CLG_LogRef LOG = {"io.usd"};

/*
 * The UsdVolParticleField3DGaussianSplat schema allows for attributes to be stored as either float
 * or half on a per-attribute basis. The following wrapper classes allow callers to read the data
 * without needing to know the underlying type being used.
 */

template<typename UsdFloatT, typename UsdHalfT> struct UsdWrappedData {
  UsdFloatT usd_float;
  UsdHalfT usd_half;
  bool uses_float = false;

  template<typename Fn> UsdWrappedData(Fn uses_float_fn, pxr::UsdTimeCode time)
  {
    pxr::UsdAttribute usd_attr;
    if (uses_float_fn(&usd_attr)) {
      usd_attr.Get(&usd_float, time);
      uses_float = true;
    }
    else {
      usd_attr.Get(&usd_half, time);
      uses_float = false;
    }
  }

  size_t size()
  {
    return uses_float ? usd_float.size() : usd_half.size();
  }
};

struct UsdWrappedFloat : UsdWrappedData<pxr::VtFloatArray, pxr::VtHalfArray> {
  template<typename Fn>
  UsdWrappedFloat(Fn uses_float_fn, pxr::UsdTimeCode time) : UsdWrappedData(uses_float_fn, time)
  {
  }

  float operator[](size_t i) const
  {
    if (uses_float) {
      return usd_float.AsConst()[i];
    }

    return usd_half.AsConst()[i];
  }
};

struct UsdWrappedVec3 : UsdWrappedData<pxr::VtVec3fArray, pxr::VtVec3hArray> {
  template<typename Fn>
  UsdWrappedVec3(Fn uses_float_fn, pxr::UsdTimeCode time) : UsdWrappedData(uses_float_fn, time)
  {
  }

  float3 operator[](size_t i) const
  {
    if (uses_float) {
      const pxr::GfVec3f vec = usd_float.AsConst()[i];
      return float3(vec[0], vec[1], vec[2]);
    }

    const pxr::GfVec3h vec = usd_half.AsConst()[i];
    return float3(vec[0], vec[1], vec[2]);
  }
};

struct UsdWrappedQuat : UsdWrappedData<pxr::VtQuatfArray, pxr::VtQuathArray> {
  template<typename Fn>
  UsdWrappedQuat(Fn uses_float_fn, pxr::UsdTimeCode time) : UsdWrappedData(uses_float_fn, time)
  {
  }

  math::Quaternion operator[](size_t i) const
  {
    if (uses_float) {
      const pxr::GfQuatf quat = usd_float.AsConst()[i];
      return math::Quaternion(
          quat.GetReal(), quat.GetImaginary()[0], quat.GetImaginary()[1], quat.GetImaginary()[2]);
    }

    const pxr::GfQuath quat = usd_half.AsConst()[i];
    return math::Quaternion(
        quat.GetReal(), quat.GetImaginary()[0], quat.GetImaginary()[1], quat.GetImaginary()[2]);
  }
};

void USDParticleFieldReader::create_object(Main *bmain)
{
  PointCloud *point_cloud = BKE_pointcloud_add(bmain, name_.c_str());
  object_ = BKE_object_add_only_object(bmain, OB_POINTCLOUD, name_.c_str());
  object_->data = id_cast<ID *>(point_cloud);
}

void USDParticleFieldReader::read_object_data(Main *bmain, pxr::UsdTimeCode time)
{
  const USDMeshReadParams params = create_mesh_read_params(time.GetValue(),
                                                           import_params_.mesh_read_flag);

  PointCloud *point_cloud = id_cast<PointCloud *>(object_->data);

  bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
      point_cloud, bke::GeometryOwnershipType::Editable);

  read_geometry(geometry_set, params, nullptr);

  PointCloud *read_point_cloud =
      geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

  if (read_point_cloud != point_cloud) {
    BKE_pointcloud_nomain_to_pointcloud(read_point_cloud, point_cloud);
  }

  if (is_animated()) {
    add_cache_modifier();
  }

  USDXformReader::read_object_data(bmain, time);
}

void USDParticleFieldReader::read_geometry(bke::GeometrySet &geometry_set,
                                           USDMeshReadParams params,
                                           const char ** /*r_err_str*/)
{
  UsdWrappedVec3 usd_positions(
      [&](pxr::UsdAttribute *attr) { return gsplat_prim_.UsesFloatPositions(attr); },
      params.motion_sample_time);

  UsdWrappedVec3 usd_scales(
      [&](pxr::UsdAttribute *attr) { return gsplat_prim_.UsesFloatScales(attr); },
      params.motion_sample_time);

  UsdWrappedFloat usd_opacities(
      [&](pxr::UsdAttribute *attr) { return gsplat_prim_.UsesFloatOpacities(attr); },
      params.motion_sample_time);

  UsdWrappedQuat usd_orientations(
      [&](pxr::UsdAttribute *attr) { return gsplat_prim_.UsesFloatOrientations(attr); },
      params.motion_sample_time);

  UsdWrappedVec3 usd_coeffs(
      [&](pxr::UsdAttribute *attr) { return gsplat_prim_.UsesFloatRadianceCoefficients(attr); },
      params.motion_sample_time);

  int usd_degree = 0;
  gsplat_prim_.GetRadianceSphericalHarmonicsDegreeAttr().Get(&usd_degree,
                                                             params.motion_sample_time);

  if (!validate::size_fits_in_int(usd_positions.size())) {
    CLOG_WARN(&LOG,
              "ParticleField '%s' has an unsupported number of points.",
              this->prim_path().GetAsString().c_str());
    return;
  }

  if (usd_degree < 0 || usd_degree > 4) {
    CLOG_WARN(&LOG,
              "ParticleField '%s' has an unsupported spherical harmonics degree.",
              this->prim_path().GetAsString().c_str());
    return;
  }

  if (usd_scales.size() != usd_positions.size() || usd_opacities.size() != usd_positions.size() ||
      usd_orientations.size() != usd_positions.size())
  {
    CLOG_WARN(&LOG,
              "ParticleField '%s' has incorrect data sizes.",
              this->prim_path().GetAsString().c_str());
    return;
  }

  const int usd_coeff_element_size = (usd_degree + 1) * (usd_degree + 1);
  if (usd_coeffs.size() != usd_positions.size() * usd_coeff_element_size) {
    CLOG_WARN(&LOG,
              "ParticleField '%s' has an incorrect amount of spherical harmonics data.",
              this->prim_path().GetAsString().c_str());
    return;
  }

  PointCloud *point_cloud = geometry_set.get_pointcloud_for_write();
  if (point_cloud->totpoint != usd_positions.size()) {
    point_cloud = BKE_pointcloud_new_nomain(PointCloudType::GSplat, usd_positions.size());
  }

  MutableSpan<float3> positions = point_cloud->positions_for_write();
  for (int64_t i = 0; i < positions.size(); i++) {
    positions[i] = usd_positions[i];
  }

  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, usd_degree);
  MutableSpan<float4> radiance = accessor.radiance_base_for_write();
  MutableSpan<float3> scale = accessor.scales_for_write();
  MutableSpan<math::Quaternion> rotation = accessor.rotations_for_write();
  Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  const int num_sh_dimensions = usd_coeff_element_size - 1;
  threading::parallel_for(positions.index_range(), 16384, [&](IndexRange range) {
    for (const int64_t i : range) {
      const int64_t sh_offset = (i * usd_coeff_element_size);

      radiance[i] = float4(usd_coeffs[sh_offset], usd_opacities[i]);
      scale[i] = usd_scales[i];
      rotation[i] = math::normalize(usd_orientations[i]);

      for (int dimension = 0; dimension < num_sh_dimensions; dimension++) {
        sh_attrs[dimension][i] = usd_coeffs[sh_offset + dimension + 1];
      }
    }
  });

  accessor.finish();

  geometry_set.replace_pointcloud(point_cloud);
}

bool USDParticleFieldReader::is_animated() const
{
  bool is_animated = gsplat_prim_.GetPositionsAttr().ValueMightBeTimeVarying();
  is_animated |= gsplat_prim_.GetScalesAttr().ValueMightBeTimeVarying();
  is_animated |= gsplat_prim_.GetOpacitiesAttr().ValueMightBeTimeVarying();
  is_animated |= gsplat_prim_.GetOrientationsAttr().ValueMightBeTimeVarying();
  is_animated |=
      gsplat_prim_.GetRadianceSphericalHarmonicsCoefficientsAttr().ValueMightBeTimeVarying();

  return is_animated;
}

}  // namespace blender::io::usd
