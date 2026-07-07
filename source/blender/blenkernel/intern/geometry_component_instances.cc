/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

InstancesComponent::InstancesComponent() : GeometryComponent(Type::Instance) {}

InstancesComponent::InstancesComponent(Instances *instances, GeometryOwnershipType ownership)
    : GeometryComponent(Type::Instance), instances_(instances), ownership_(ownership)
{
}

InstancesComponent::~InstancesComponent()
{
  this->clear();
}

GeometryComponentPtr InstancesComponent::copy() const
{
  InstancesComponent *new_component = new InstancesComponent();
  if (instances_ != nullptr) {
    new_component->instances_ = new Instances(*instances_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return GeometryComponentPtr(new_component);
}

void InstancesComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  if (ownership_ == GeometryOwnershipType::Owned) {
    delete instances_;
  }
  instances_ = nullptr;
}

bool InstancesComponent::is_empty() const
{
  if (instances_ != nullptr) {
    if (instances_->instances_num() > 0) {
      return false;
    }
  }
  return true;
}

bool InstancesComponent::owns_direct_data() const
{
  if (instances_ != nullptr) {
    return instances_->owns_direct_data();
  }
  return true;
}

void InstancesComponent::ensure_owns_direct_data()
{
  if (instances_ != nullptr) {
    instances_->ensure_owns_direct_data();
  }
}

const Instances *InstancesComponent::get() const
{
  return instances_;
}

Instances *InstancesComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    instances_ = new Instances(*instances_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return instances_;
}

void InstancesComponent::replace(Instances *instances, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  instances_ = instances;
  ownership_ = ownership;
}

Instances *InstancesComponent::release()
{
  BLI_assert(this->is_mutable());
  Instances *instance = instances_;
  instances_ = nullptr;
  return instance;
}

void InstancesComponent::count_memory(MemoryCounter &memory) const
{
  if (instances_) {
    instances_->count_memory(memory);
  }
}

std::optional<AttributeAccessor> InstancesComponent::attributes() const
{
  return AttributeAccessor(instances_, instance_attribute_accessor_functions());
}

std::optional<MutableAttributeAccessor> InstancesComponent::attributes_for_write()
{
  return MutableAttributeAccessor(instances_, instance_attribute_accessor_functions());
}

/** \} */

}  // namespace blender::bke
