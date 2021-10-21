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

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

using fn::GVArrayPtr;
using fn::GVMutableArrayPtr;

/**
 * Utility to group together multiple functions that are used to access custom data on geometry
 * components in a generic way.
 */
struct CustomDataAccessInfo {
  using CustomDataGetter = CustomData *(*)(GeometryComponent &component);
  using ConstCustomDataGetter = const CustomData *(*)(const GeometryComponent &component);
  using UpdateCustomDataPointers = void (*)(GeometryComponent &component);

  CustomDataGetter get_custom_data;
  ConstCustomDataGetter get_const_custom_data;
  UpdateCustomDataPointers update_custom_data_pointers;
};

/**
 * A #BuiltinAttributeProvider is responsible for exactly one attribute on a geometry component.
 * The attribute is identified by its name and has a fixed domain and type. Builtin attributes do
 * not follow the same loose rules as other attributes, because they are mapped to internal
 * "legacy" data structures. For example, some builtin attributes cannot be deleted. */
class BuiltinAttributeProvider {
 public:
  /* Some utility enums to avoid hard to read booleans in function calls. */
  enum CreatableEnum {
    Creatable,
    NonCreatable,
  };
  enum WritableEnum {
    Writable,
    Readonly,
  };
  enum DeletableEnum {
    Deletable,
    NonDeletable,
  };

 protected:
  const std::string name_;
  const AttributeDomain domain_;
  const CustomDataType data_type_;
  const CreatableEnum createable_;
  const WritableEnum writable_;
  const DeletableEnum deletable_;

 public:
  BuiltinAttributeProvider(std::string name,
                           const AttributeDomain domain,
                           const CustomDataType data_type,
                           const CreatableEnum createable,
                           const WritableEnum writable,
                           const DeletableEnum deletable)
      : name_(std::move(name)),
        domain_(domain),
        data_type_(data_type),
        createable_(createable),
        writable_(writable),
        deletable_(deletable)
  {
  }

  virtual GVArrayPtr try_get_for_read(const GeometryComponent &component) const = 0;
  virtual GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const = 0;
  virtual bool try_delete(GeometryComponent &component) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component),
                          const AttributeInit &UNUSED(initializer)) const = 0;
  virtual bool exists(const GeometryComponent &component) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  AttributeDomain domain() const
  {
    return domain_;
  }

  CustomDataType data_type() const
  {
    return data_type_;
  }
};

/**
 * A #DynamicAttributesProvider manages a set of named attributes on a geometry component. Each
 * attribute has a name, domain and type.
 */
class DynamicAttributesProvider {
 public:
  virtual ReadAttributeLookup try_get_for_read(const GeometryComponent &component,
                                               const AttributeIDRef &attribute_id) const = 0;
  virtual WriteAttributeLookup try_get_for_write(GeometryComponent &component,
                                                 const AttributeIDRef &attribute_id) const = 0;
  virtual bool try_delete(GeometryComponent &component,
                          const AttributeIDRef &attribute_id) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component),
                          const AttributeIDRef &UNUSED(attribute_id),
                          const AttributeDomain UNUSED(domain),
                          const CustomDataType UNUSED(data_type),
                          const AttributeInit &UNUSED(initializer)) const
  {
    /* Some providers should not create new attributes. */
    return false;
  };

  virtual bool foreach_attribute(const GeometryComponent &component,
                                 const AttributeForeachCallback callback) const = 0;
  virtual void foreach_domain(const FunctionRef<void(AttributeDomain)> callback) const = 0;
};

/**
 * This is the attribute provider for most user generated attributes.
 */
class CustomDataAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 |
                                                   CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_INT32 |
                                                   CD_MASK_PROP_COLOR | CD_MASK_PROP_BOOL;
  const AttributeDomain domain_;
  const CustomDataAccessInfo custom_data_access_;

 public:
  CustomDataAttributeProvider(const AttributeDomain domain,
                              const CustomDataAccessInfo custom_data_access)
      : domain_(domain), custom_data_access_(custom_data_access)
  {
  }

  ReadAttributeLookup try_get_for_read(const GeometryComponent &component,
                                       const AttributeIDRef &attribute_id) const final;

  WriteAttributeLookup try_get_for_write(GeometryComponent &component,
                                         const AttributeIDRef &attribute_id) const final;

  bool try_delete(GeometryComponent &component, const AttributeIDRef &attribute_id) const final;

  bool try_create(GeometryComponent &component,
                  const AttributeIDRef &attribute_id,
                  const AttributeDomain domain,
                  const CustomDataType data_type,
                  const AttributeInit &initializer) const final;

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final;

  void foreach_domain(const FunctionRef<void(AttributeDomain)> callback) const final
  {
    callback(domain_);
  }

 private:
  bool type_is_supported(CustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }
};

/**
 * This attribute provider is used for uv maps and vertex colors.
 */
class NamedLegacyCustomDataProvider final : public DynamicAttributesProvider {
 private:
  using AsReadAttribute = GVArrayPtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = GVMutableArrayPtr (*)(void *data, const int domain_size);
  const AttributeDomain domain_;
  const CustomDataType attribute_type_;
  const CustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;

 public:
  NamedLegacyCustomDataProvider(const AttributeDomain domain,
                                const CustomDataType attribute_type,
                                const CustomDataType stored_type,
                                const CustomDataAccessInfo custom_data_access,
                                const AsReadAttribute as_read_attribute,
                                const AsWriteAttribute as_write_attribute)
      : domain_(domain),
        attribute_type_(attribute_type),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute)
  {
  }

  ReadAttributeLookup try_get_for_read(const GeometryComponent &component,
                                       const AttributeIDRef &attribute_id) const final;
  WriteAttributeLookup try_get_for_write(GeometryComponent &component,
                                         const AttributeIDRef &attribute_id) const final;
  bool try_delete(GeometryComponent &component, const AttributeIDRef &attribute_id) const final;
  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final;
  void foreach_domain(const FunctionRef<void(AttributeDomain)> callback) const final;
};

/**
 * This provider is used to provide access to builtin attributes. It supports making internal types
 * available as different types. For example, the vertex position attribute is stored as part of
 * the #MVert struct, but is exposed as float3 attribute.
 *
 * It also supports named builtin attributes, and will look up attributes in #CustomData by name
 * if the stored type is the same as the attribute type.
 */
class BuiltinCustomDataLayerProvider final : public BuiltinAttributeProvider {
  using AsReadAttribute = GVArrayPtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = GVMutableArrayPtr (*)(void *data, const int domain_size);
  using UpdateOnRead = void (*)(const GeometryComponent &component);
  using UpdateOnWrite = void (*)(GeometryComponent &component);
  const CustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;
  const UpdateOnWrite update_on_write_;
  bool stored_as_named_attribute_;

 public:
  BuiltinCustomDataLayerProvider(std::string attribute_name,
                                 const AttributeDomain domain,
                                 const CustomDataType attribute_type,
                                 const CustomDataType stored_type,
                                 const CreatableEnum creatable,
                                 const WritableEnum writable,
                                 const DeletableEnum deletable,
                                 const CustomDataAccessInfo custom_data_access,
                                 const AsReadAttribute as_read_attribute,
                                 const AsWriteAttribute as_write_attribute,
                                 const UpdateOnWrite update_on_write)
      : BuiltinAttributeProvider(
            std::move(attribute_name), domain, attribute_type, creatable, writable, deletable),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute),
        update_on_write_(update_on_write),
        stored_as_named_attribute_(data_type_ == stored_type_)
  {
  }

  GVArrayPtr try_get_for_read(const GeometryComponent &component) const final;
  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const final;
  bool try_delete(GeometryComponent &component) const final;
  bool try_create(GeometryComponent &component, const AttributeInit &initializer) const final;
  bool exists(const GeometryComponent &component) const final;
};

/**
 * This is a container for multiple attribute providers that are used by one geometry component
 * type (e.g. there is a set of attribute providers for mesh components).
 */
class ComponentAttributeProviders {
 private:
  /**
   * Builtin attribute providers are identified by their name. Attribute names that are in this
   * map will only be accessed using builtin attribute providers. Therefore, these providers have
   * higher priority when an attribute name is looked up. Usually, that means that builtin
   * providers are checked before dynamic ones.
   */
  Map<std::string, const BuiltinAttributeProvider *> builtin_attribute_providers_;
  /**
   * An ordered list of dynamic attribute providers. The order is important because that is order
   * in which they are checked when an attribute is looked up.
   */
  Vector<const DynamicAttributesProvider *> dynamic_attribute_providers_;
  /**
   * All the domains that are supported by at least one of the providers above.
   */
  VectorSet<AttributeDomain> supported_domains_;

 public:
  ComponentAttributeProviders(Span<const BuiltinAttributeProvider *> builtin_attribute_providers,
                              Span<const DynamicAttributesProvider *> dynamic_attribute_providers)
      : dynamic_attribute_providers_(dynamic_attribute_providers)
  {
    for (const BuiltinAttributeProvider *provider : builtin_attribute_providers) {
      /* Use #add_new to make sure that no two builtin attributes have the same name. */
      builtin_attribute_providers_.add_new(provider->name(), provider);
      supported_domains_.add(provider->domain());
    }
    for (const DynamicAttributesProvider *provider : dynamic_attribute_providers) {
      provider->foreach_domain([&](AttributeDomain domain) { supported_domains_.add(domain); });
    }
  }

  const Map<std::string, const BuiltinAttributeProvider *> &builtin_attribute_providers() const
  {
    return builtin_attribute_providers_;
  }

  Span<const DynamicAttributesProvider *> dynamic_attribute_providers() const
  {
    return dynamic_attribute_providers_;
  }

  Span<AttributeDomain> supported_domains() const
  {
    return supported_domains_;
  }
};

}  // namespace blender::bke
