/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.hh"
#include "BKE_pointcloud.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

PointCloudComponent::PointCloudComponent() : GeometryComponent(Type::PointCloud) {}

PointCloudComponent::PointCloudComponent(PointCloud *pointcloud, GeometryOwnershipType ownership)
    : GeometryComponent(Type::PointCloud), pointcloud_(pointcloud), ownership_(ownership)
{
}

PointCloudComponent::~PointCloudComponent()
{
  this->clear();
}

GeometryComponentPtr PointCloudComponent::copy() const
{
  PointCloudComponent *new_component = new PointCloudComponent();
  if (pointcloud_ != nullptr) {
    new_component->pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return GeometryComponentPtr(new_component);
}

void PointCloudComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
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

const PointCloud *PointCloudComponent::get() const
{
  return pointcloud_;
}

PointCloud *PointCloudComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_);
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
    if (pointcloud_) {
      pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_);
    }
    ownership_ = GeometryOwnershipType::Owned;
  }
}

void PointCloudComponent::count_memory(MemoryCounter &memory) const
{
  if (pointcloud_) {
    pointcloud_->count_memory(memory);
  }
}

/** \} */

}  // namespace blender::bke

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

std::optional<AttributeAccessor> PointCloudComponent::attributes() const
{
  return AttributeAccessor(pointcloud_, pointcloud_attribute_accessor_functions());
}

std::optional<MutableAttributeAccessor> PointCloudComponent::attributes_for_write()
{
  PointCloud *pointcloud = this->get_for_write();
  return MutableAttributeAccessor(pointcloud, pointcloud_attribute_accessor_functions());
}

/** \} */

}  // namespace blender::bke
