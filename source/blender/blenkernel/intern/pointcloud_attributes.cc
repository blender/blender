/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.hh"

#include "attribute_access_intern.hh"

namespace blender::bke {

static void tag_component_positions_changed(void *owner)
{
  PointCloud &points = *static_cast<PointCloud *>(owner);
  points.tag_positions_changed();
}

static void tag_component_radius_changed(void *owner)
{
  PointCloud &points = *static_cast<PointCloud *>(owner);
  points.tag_radii_changed();
}

/**
 * In this function all the attribute providers for a point cloud component are created. Most data
 * in this function is statically allocated, because it does not change over time.
 */
static GeometryAttributeProviders create_attribute_providers_for_point_cloud()
{
  static CustomDataAccessInfo point_access = {
      [](void *owner) -> CustomData * {
        PointCloud *pointcloud = static_cast<PointCloud *>(owner);
        return &pointcloud->pdata;
      },
      [](const void *owner) -> const CustomData * {
        const PointCloud *pointcloud = static_cast<const PointCloud *>(owner);
        return &pointcloud->pdata;
      },
      [](const void *owner) -> int {
        const PointCloud *pointcloud = static_cast<const PointCloud *>(owner);
        return pointcloud->totpoint;
      }};

  static BuiltinCustomDataLayerProvider position("position",
                                                 AttrDomain::Point,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 tag_component_positions_changed);
  static BuiltinCustomDataLayerProvider radius("radius",
                                               AttrDomain::Point,
                                               CD_PROP_FLOAT,
                                               BuiltinAttributeProvider::Deletable,
                                               point_access,
                                               tag_component_radius_changed);
  static BuiltinCustomDataLayerProvider id("id",
                                           AttrDomain::Point,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Deletable,
                                           point_access,
                                           nullptr);
  static CustomDataAttributeProvider point_custom_data(AttrDomain::Point, point_access);
  return GeometryAttributeProviders({&position, &radius, &id}, {&point_custom_data});
}

static AttributeAccessorFunctions get_pointcloud_accessor_functions()
{
  static const GeometryAttributeProviders providers = create_attribute_providers_for_point_cloud();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const PointCloud &pointcloud = *static_cast<const PointCloud *>(owner);
    switch (domain) {
      case AttrDomain::Point:
        return pointcloud.totpoint;
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return domain == AttrDomain::Point;
  };
  fn.adapt_domain = [](const void * /*owner*/,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) {
    if (from_domain == to_domain && from_domain == AttrDomain::Point) {
      return varray;
    }
    return GVArray{};
  };
  return fn;
}

const AttributeAccessorFunctions &pointcloud_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_pointcloud_accessor_functions();
  return fn;
}

}  // namespace blender::bke
