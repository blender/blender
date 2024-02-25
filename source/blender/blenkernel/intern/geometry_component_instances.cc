/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

#include "attribute_access_intern.hh"

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

static void tag_component_reference_index_changed(void *owner)
{
  Instances &instances = *static_cast<Instances *>(owner);
  instances.tag_reference_handles_changed();
}

static ComponentAttributeProviders create_attribute_providers_for_instances()
{
  static CustomDataAccessInfo instance_custom_data_access = {
      [](void *owner) -> CustomData * {
        Instances *instances = static_cast<Instances *>(owner);
        return &instances->custom_data_attributes();
      },
      [](const void *owner) -> const CustomData * {
        const Instances *instances = static_cast<const Instances *>(owner);
        return &instances->custom_data_attributes();
      },
      [](const void *owner) -> int {
        const Instances *instances = static_cast<const Instances *>(owner);
        return instances->instances_num();
      }};

  /**
   * IDs of the instances. They are used for consistency over multiple frames for things like
   * motion blur. Proper stable ID data that actually helps when rendering can only be generated
   * in some situations, so this vector is allowed to be empty, in which case the index of each
   * instance will be used for the final ID.
   */
  static BuiltinCustomDataLayerProvider id("id",
                                           AttrDomain::Instance,
                                           CD_PROP_INT32,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Creatable,
                                           BuiltinAttributeProvider::Deletable,
                                           instance_custom_data_access,
                                           nullptr);

  static BuiltinCustomDataLayerProvider instance_transform("instance_transform",
                                                           AttrDomain::Instance,
                                                           CD_PROP_FLOAT4X4,
                                                           CD_PROP_FLOAT4X4,
                                                           BuiltinAttributeProvider::Creatable,
                                                           BuiltinAttributeProvider::NonDeletable,
                                                           instance_custom_data_access,
                                                           nullptr);

  /** Indices into `Instances::references_`. Determines what data is instanced. */
  static BuiltinCustomDataLayerProvider reference_index(".reference_index",
                                                        AttrDomain::Instance,
                                                        CD_PROP_INT32,
                                                        CD_PROP_INT32,
                                                        BuiltinAttributeProvider::Creatable,
                                                        BuiltinAttributeProvider::NonDeletable,
                                                        instance_custom_data_access,
                                                        tag_component_reference_index_changed);

  static CustomDataAttributeProvider instance_custom_data(AttrDomain::Instance,
                                                          instance_custom_data_access);

  return ComponentAttributeProviders({&instance_transform, &id, &reference_index},
                                     {&instance_custom_data});
}

static AttributeAccessorFunctions get_instances_accessor_functions()
{
  static const ComponentAttributeProviders providers = create_attribute_providers_for_instances();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const Instances *instances = static_cast<const Instances *>(owner);
    switch (domain) {
      case AttrDomain::Instance:
        return instances->instances_num();
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return domain == AttrDomain::Instance;
  };
  fn.adapt_domain = [](const void * /*owner*/,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) {
    if (from_domain == to_domain && from_domain == AttrDomain::Instance) {
      return varray;
    }
    return GVArray{};
  };
  return fn;
}

static const AttributeAccessorFunctions &get_instances_accessor_functions_ref()
{
  static const AttributeAccessorFunctions fn = get_instances_accessor_functions();
  return fn;
}

AttributeAccessor Instances::attributes() const
{
  return AttributeAccessor(this, get_instances_accessor_functions_ref());
}

MutableAttributeAccessor Instances::attributes_for_write()
{
  return MutableAttributeAccessor(this, get_instances_accessor_functions_ref());
}

std::optional<AttributeAccessor> InstancesComponent::attributes() const
{
  return AttributeAccessor(instances_, get_instances_accessor_functions_ref());
}

std::optional<MutableAttributeAccessor> InstancesComponent::attributes_for_write()
{
  return MutableAttributeAccessor(instances_, get_instances_accessor_functions_ref());
}

/** \} */

}  // namespace blender::bke
