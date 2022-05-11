/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_pointcloud.h"

#include "attribute_access_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

PointCloudComponent::PointCloudComponent() : GeometryComponent(GEO_COMPONENT_TYPE_POINT_CLOUD)
{
}

PointCloudComponent::~PointCloudComponent()
{
  this->clear();
}

GeometryComponent *PointCloudComponent::copy() const
{
  PointCloudComponent *new_component = new PointCloudComponent();
  if (pointcloud_ != nullptr) {
    new_component->pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void PointCloudComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (pointcloud_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, pointcloud_);
    }
    pointcloud_ = nullptr;
  }
}

bool PointCloudComponent::has_pointcloud() const
{
  return pointcloud_ != nullptr;
}

void PointCloudComponent::replace(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  pointcloud_ = pointcloud;
  ownership_ = ownership;
}

PointCloud *PointCloudComponent::release()
{
  BLI_assert(this->is_mutable());
  PointCloud *pointcloud = pointcloud_;
  pointcloud_ = nullptr;
  return pointcloud;
}

const PointCloud *PointCloudComponent::get_for_read() const
{
  return pointcloud_;
}

PointCloud *PointCloudComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return pointcloud_;
}

bool PointCloudComponent::is_empty() const
{
  return pointcloud_ == nullptr;
}

bool PointCloudComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void PointCloudComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

int PointCloudComponent::attribute_domain_num(const AttributeDomain domain) const
{
  if (pointcloud_ == nullptr) {
    return 0;
  }
  if (domain != ATTR_DOMAIN_POINT) {
    return 0;
  }
  return pointcloud_->totpoint;
}

namespace blender::bke {

/**
 * In this function all the attribute providers for a point cloud component are created. Most data
 * in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_point_cloud()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
    if (PointCloud *pointcloud = pointcloud_component.get_for_write()) {
      BKE_pointcloud_update_customdata_pointers(pointcloud);
    }
  };
  static CustomDataAccessInfo point_access = {
      [](GeometryComponent &component) -> CustomData * {
        PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
        PointCloud *pointcloud = pointcloud_component.get_for_write();
        return pointcloud ? &pointcloud->pdata : nullptr;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const PointCloudComponent &pointcloud_component = static_cast<const PointCloudComponent &>(
            component);
        const PointCloud *pointcloud = pointcloud_component.get_for_read();
        return pointcloud ? &pointcloud->pdata : nullptr;
      },
      update_custom_data_pointers};

  static BuiltinCustomDataLayerProvider position("position",
                                                 ATTR_DOMAIN_POINT,
                                                 CD_PROP_FLOAT3,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonCreatable,
                                                 BuiltinAttributeProvider::Writable,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 make_array_read_attribute<float3>,
                                                 make_array_write_attribute<float3>,
                                                 nullptr);
  static BuiltinCustomDataLayerProvider radius("radius",
                                               ATTR_DOMAIN_POINT,
                                               CD_PROP_FLOAT,
                                               CD_PROP_FLOAT,
                                               BuiltinAttributeProvider::Creatable,
                                               BuiltinAttributeProvider::Writable,
                                               BuiltinAttributeProvider::Deletable,
                                               point_access,
                                               make_array_read_attribute<float>,
                                               make_array_write_attribute<float>,
                                               nullptr);
  static BuiltinCustomDataLayerProvider id("id",
                                           ATTR_DOMAIN_POINT,
                                           CD_PROP_INT32,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Creatable,
                                           BuiltinAttributeProvider::Writable,
                                           BuiltinAttributeProvider::Deletable,
                                           point_access,
                                           make_array_read_attribute<int>,
                                           make_array_write_attribute<int>,
                                           nullptr);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);
  return ComponentAttributeProviders({&position, &radius, &id}, {&point_custom_data});
}

}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *PointCloudComponent::get_attribute_providers()
    const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_point_cloud();
  return &providers;
}

/** \} */
