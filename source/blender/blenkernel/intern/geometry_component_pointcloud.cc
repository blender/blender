/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

/* Clear the component and replace it with the new point cloud. */
void PointCloudComponent::replace(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  pointcloud_ = pointcloud;
  ownership_ = ownership;
}

/* Return the point cloud and clear the component. The caller takes over responsibility for freeing
 * the point cloud (if the component was responsible before). */
PointCloud *PointCloudComponent::release()
{
  BLI_assert(this->is_mutable());
  PointCloud *pointcloud = pointcloud_;
  pointcloud_ = nullptr;
  return pointcloud;
}

/* Get the point cloud from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned point cloud should not be modified. No ownership is transferred.
 */
const PointCloud *PointCloudComponent::get_for_read() const
{
  return pointcloud_;
}

/* Get the point cloud from this component. This method can only be used when the component is
 * mutable, i.e. it is not shared. The returned point cloud can be modified. No ownership is
 * transferred. */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

int PointCloudComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(domain == ATTR_DOMAIN_POINT);
  UNUSED_VARS_NDEBUG(domain);
  if (pointcloud_ == nullptr) {
    return 0;
  }
  return pointcloud_->totpoint;
}

namespace blender::bke {

template<typename T, AttributeDomain Domain>
static ReadAttributePtr make_array_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<ArrayReadAttribute<T>>(Domain, Span<T>((const T *)data, domain_size));
}

template<typename T, AttributeDomain Domain>
static WriteAttributePtr make_array_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<ArrayWriteAttribute<T>>(Domain, MutableSpan<T>((T *)data, domain_size));
}

/**
 * In this function all the attribute providers for a point cloud component are created. Most data
 * in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_point_cloud()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
    PointCloud *pointcloud = pointcloud_component.get_for_write();
    if (pointcloud != nullptr) {
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

  static BuiltinCustomDataLayerProvider position(
      "position",
      ATTR_DOMAIN_POINT,
      CD_PROP_FLOAT3,
      CD_PROP_FLOAT3,
      BuiltinAttributeProvider::NonCreatable,
      BuiltinAttributeProvider::Writable,
      BuiltinAttributeProvider::NonDeletable,
      point_access,
      make_array_read_attribute<float3, ATTR_DOMAIN_POINT>,
      make_array_write_attribute<float3, ATTR_DOMAIN_POINT>,
      nullptr,
      nullptr);
  static BuiltinCustomDataLayerProvider radius(
      "radius",
      ATTR_DOMAIN_POINT,
      CD_PROP_FLOAT,
      CD_PROP_FLOAT,
      BuiltinAttributeProvider::Creatable,
      BuiltinAttributeProvider::Writable,
      BuiltinAttributeProvider::Deletable,
      point_access,
      make_array_read_attribute<float, ATTR_DOMAIN_POINT>,
      make_array_write_attribute<float, ATTR_DOMAIN_POINT>,
      nullptr,
      nullptr);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);
  return ComponentAttributeProviders({&position, &radius}, {&point_custom_data});
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
