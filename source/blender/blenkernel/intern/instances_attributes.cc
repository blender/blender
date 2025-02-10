/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_instances.hh"

#include "attribute_access_intern.hh"

namespace blender::bke {

static void tag_component_reference_index_changed(void *owner)
{
  Instances &instances = *static_cast<Instances *>(owner);
  instances.tag_reference_handles_changed();
}

static GeometryAttributeProviders create_attribute_providers_for_instances()
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
                                           BuiltinAttributeProvider::Deletable,
                                           instance_custom_data_access,
                                           nullptr);

  static BuiltinCustomDataLayerProvider instance_transform("instance_transform",
                                                           AttrDomain::Instance,
                                                           CD_PROP_FLOAT4X4,
                                                           BuiltinAttributeProvider::NonDeletable,
                                                           instance_custom_data_access,
                                                           nullptr);

  /** Indices into `Instances::references_`. Determines what data is instanced. */
  static BuiltinCustomDataLayerProvider reference_index(".reference_index",
                                                        AttrDomain::Instance,
                                                        CD_PROP_INT32,
                                                        BuiltinAttributeProvider::NonDeletable,
                                                        instance_custom_data_access,
                                                        tag_component_reference_index_changed);

  static CustomDataAttributeProvider instance_custom_data(AttrDomain::Instance,
                                                          instance_custom_data_access);

  return GeometryAttributeProviders({&instance_transform, &id, &reference_index},
                                    {&instance_custom_data});
}

static AttributeAccessorFunctions get_instances_accessor_functions()
{
  static const GeometryAttributeProviders providers = create_attribute_providers_for_instances();
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

const AttributeAccessorFunctions &instance_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_instances_accessor_functions();
  return fn;
}

}  // namespace blender::bke
