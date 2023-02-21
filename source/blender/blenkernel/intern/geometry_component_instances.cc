/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "attribute_access_intern.hh"

#include "BLI_cpp_type_make.hh"

using blender::float4x4;
using blender::GSpan;
using blender::IndexMask;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::VectorSet;
using blender::bke::InstanceReference;
using blender::bke::Instances;

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

InstancesComponent::InstancesComponent() : GeometryComponent(GEO_COMPONENT_TYPE_INSTANCES)
{
}

InstancesComponent::~InstancesComponent()
{
  this->clear();
}

GeometryComponent *InstancesComponent::copy() const
{
  InstancesComponent *new_component = new InstancesComponent();
  if (instances_ != nullptr) {
    new_component->instances_ = new Instances(*instances_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void InstancesComponent::clear()
{
  BLI_assert(this->is_mutable());
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

const blender::bke::Instances *InstancesComponent::get_for_read() const
{
  return instances_;
}

blender::bke::Instances *InstancesComponent::get_for_write()
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

namespace blender::bke {

static float3 get_transform_position(const float4x4 &transform)
{
  return transform.location();
}

static void set_transform_position(float4x4 &transform, const float3 position)
{
  transform.location() = position;
}

class InstancePositionAttributeProvider final : public BuiltinAttributeProvider {
 public:
  InstancePositionAttributeProvider()
      : BuiltinAttributeProvider(
            "position", ATTR_DOMAIN_INSTANCE, CD_PROP_FLOAT3, NonCreatable, NonDeletable)
  {
  }

  GVArray try_get_for_read(const void *owner) const final
  {
    const Instances *instances = static_cast<const Instances *>(owner);
    if (instances == nullptr) {
      return {};
    }
    Span<float4x4> transforms = instances->transforms();
    return VArray<float3>::ForDerivedSpan<float4x4, get_transform_position>(transforms);
  }

  GAttributeWriter try_get_for_write(void *owner) const final
  {
    Instances *instances = static_cast<Instances *>(owner);
    if (instances == nullptr) {
      return {};
    }
    MutableSpan<float4x4> transforms = instances->transforms();
    return {VMutableArray<float3>::ForDerivedSpan<float4x4,
                                                  get_transform_position,
                                                  set_transform_position>(transforms),
            domain_};
  }

  bool try_delete(void * /*owner*/) const final
  {
    return false;
  }

  bool try_create(void * /*owner*/, const AttributeInit & /*initializer*/) const final
  {
    return false;
  }

  bool exists(const void * /*owner*/) const final
  {
    return true;
  }
};

static ComponentAttributeProviders create_attribute_providers_for_instances()
{
  static InstancePositionAttributeProvider position;
  static CustomDataAccessInfo instance_custom_data_access = {
      [](void *owner) -> CustomData * {
        Instances *instances = static_cast<Instances *>(owner);
        return &instances->custom_data_attributes().data;
      },
      [](const void *owner) -> const CustomData * {
        const Instances *instances = static_cast<const Instances *>(owner);
        return &instances->custom_data_attributes().data;
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
                                           ATTR_DOMAIN_INSTANCE,
                                           CD_PROP_INT32,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Creatable,
                                           BuiltinAttributeProvider::Deletable,
                                           instance_custom_data_access,
                                           make_array_read_attribute<int>,
                                           make_array_write_attribute<int>,
                                           nullptr);

  static CustomDataAttributeProvider instance_custom_data(ATTR_DOMAIN_INSTANCE,
                                                          instance_custom_data_access);

  return ComponentAttributeProviders({&position, &id}, {&instance_custom_data});
}

static AttributeAccessorFunctions get_instances_accessor_functions()
{
  static const ComponentAttributeProviders providers = create_attribute_providers_for_instances();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const eAttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const Instances *instances = static_cast<const Instances *>(owner);
    switch (domain) {
      case ATTR_DOMAIN_INSTANCE:
        return instances->instances_num();
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const eAttrDomain domain) {
    return domain == ATTR_DOMAIN_INSTANCE;
  };
  fn.adapt_domain = [](const void * /*owner*/,
                       const blender::GVArray &varray,
                       const eAttrDomain from_domain,
                       const eAttrDomain to_domain) {
    if (from_domain == to_domain && from_domain == ATTR_DOMAIN_INSTANCE) {
      return varray;
    }
    return blender::GVArray{};
  };
  return fn;
}

static const AttributeAccessorFunctions &get_instances_accessor_functions_ref()
{
  static const AttributeAccessorFunctions fn = get_instances_accessor_functions();
  return fn;
}

blender::bke::AttributeAccessor Instances::attributes() const
{
  return blender::bke::AttributeAccessor(this,
                                         blender::bke::get_instances_accessor_functions_ref());
}

blender::bke::MutableAttributeAccessor Instances::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(
      this, blender::bke::get_instances_accessor_functions_ref());
}

}  // namespace blender::bke

std::optional<blender::bke::AttributeAccessor> InstancesComponent::attributes() const
{
  return blender::bke::AttributeAccessor(instances_,
                                         blender::bke::get_instances_accessor_functions_ref());
}

std::optional<blender::bke::MutableAttributeAccessor> InstancesComponent::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(
      instances_, blender::bke::get_instances_accessor_functions_ref());
}

/** \} */
