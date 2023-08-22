/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

/**
 * Utility to group together multiple functions that are used to access custom data on geometry
 * components in a generic way.
 */
struct CustomDataAccessInfo {
  using CustomDataGetter = CustomData *(*)(void *owner);
  using ConstCustomDataGetter = const CustomData *(*)(const void *owner);
  using GetElementNum = int (*)(const void *owner);
  using UpdateCustomDataPointers = void (*)(void *owner);

  CustomDataGetter get_custom_data;
  ConstCustomDataGetter get_const_custom_data;
  GetElementNum get_element_num;
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
  enum DeletableEnum {
    Deletable,
    NonDeletable,
  };

 protected:
  const std::string name_;
  const eAttrDomain domain_;
  const eCustomDataType data_type_;
  const CreatableEnum createable_;
  const DeletableEnum deletable_;
  const AttributeValidator validator_;

 public:
  BuiltinAttributeProvider(std::string name,
                           const eAttrDomain domain,
                           const eCustomDataType data_type,
                           const CreatableEnum createable,
                           const DeletableEnum deletable,
                           AttributeValidator validator = {})
      : name_(std::move(name)),
        domain_(domain),
        data_type_(data_type),
        createable_(createable),
        deletable_(deletable),
        validator_(validator)
  {
  }

  virtual GAttributeReader try_get_for_read(const void *owner) const = 0;
  virtual GAttributeWriter try_get_for_write(void *owner) const = 0;
  virtual bool try_delete(void *owner) const = 0;
  virtual bool try_create(void *onwer, const AttributeInit &initializer) const = 0;
  virtual bool exists(const void *owner) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  eAttrDomain domain() const
  {
    return domain_;
  }

  eCustomDataType data_type() const
  {
    return data_type_;
  }

  AttributeValidator validator() const
  {
    return validator_;
  }
};

/**
 * A #DynamicAttributesProvider manages a set of named attributes on a geometry component. Each
 * attribute has a name, domain and type.
 */
class DynamicAttributesProvider {
 public:
  virtual GAttributeReader try_get_for_read(const void *owner,
                                            const AttributeIDRef &attribute_id) const = 0;
  virtual GAttributeWriter try_get_for_write(void *owner,
                                             const AttributeIDRef &attribute_id) const = 0;
  virtual bool try_delete(void *owner, const AttributeIDRef &attribute_id) const = 0;
  virtual bool try_create(void *owner,
                          const AttributeIDRef &attribute_id,
                          const eAttrDomain domain,
                          const eCustomDataType data_type,
                          const AttributeInit &initializer) const
  {
    UNUSED_VARS(owner, attribute_id, domain, data_type, initializer);
    /* Some providers should not create new attributes. */
    return false;
  };

  virtual bool foreach_attribute(const void *owner,
                                 const AttributeForeachCallback callback) const = 0;
  virtual void foreach_domain(const FunctionRef<void(eAttrDomain)> callback) const = 0;
};

/**
 * This is the attribute provider for most user generated attributes.
 */
class CustomDataAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_ALL;
  const eAttrDomain domain_;
  const CustomDataAccessInfo custom_data_access_;

 public:
  CustomDataAttributeProvider(const eAttrDomain domain,
                              const CustomDataAccessInfo custom_data_access)
      : domain_(domain), custom_data_access_(custom_data_access)
  {
  }

  GAttributeReader try_get_for_read(const void *owner,
                                    const AttributeIDRef &attribute_id) const final;

  GAttributeWriter try_get_for_write(void *owner, const AttributeIDRef &attribute_id) const final;

  bool try_delete(void *owner, const AttributeIDRef &attribute_id) const final;

  bool try_create(void *owner,
                  const AttributeIDRef &attribute_id,
                  eAttrDomain domain,
                  const eCustomDataType data_type,
                  const AttributeInit &initializer) const final;

  bool foreach_attribute(const void *owner, const AttributeForeachCallback callback) const final;

  void foreach_domain(const FunctionRef<void(eAttrDomain)> callback) const final
  {
    callback(domain_);
  }

 private:
  bool type_is_supported(eCustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }
};

/**
 * This provider is used to provide access to builtin attributes. It supports making internal types
 * available as different types.
 *
 * It also supports named builtin attributes, and will look up attributes in #CustomData by name
 * if the stored type is the same as the attribute type.
 */
class BuiltinCustomDataLayerProvider final : public BuiltinAttributeProvider {
  using UpdateOnChange = void (*)(void *owner);
  const eCustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const UpdateOnChange update_on_change_;
  bool stored_as_named_attribute_;

 public:
  BuiltinCustomDataLayerProvider(std::string attribute_name,
                                 const eAttrDomain domain,
                                 const eCustomDataType attribute_type,
                                 const eCustomDataType stored_type,
                                 const CreatableEnum creatable,
                                 const DeletableEnum deletable,
                                 const CustomDataAccessInfo custom_data_access,
                                 const UpdateOnChange update_on_write,
                                 const AttributeValidator validator = {})
      : BuiltinAttributeProvider(
            std::move(attribute_name), domain, attribute_type, creatable, deletable, validator),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        update_on_change_(update_on_write),
        stored_as_named_attribute_(data_type_ == stored_type_)
  {
  }

  GAttributeReader try_get_for_read(const void *owner) const final;
  GAttributeWriter try_get_for_write(void *owner) const final;
  bool try_delete(void *owner) const final;
  bool try_create(void *owner, const AttributeInit &initializer) const final;
  bool exists(const void *owner) const final;

 private:
  bool layer_exists(const CustomData &custom_data) const;
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
  VectorSet<eAttrDomain> supported_domains_;

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
      provider->foreach_domain([&](eAttrDomain domain) { supported_domains_.add(domain); });
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

  Span<eAttrDomain> supported_domains() const
  {
    return supported_domains_;
  }
};

namespace attribute_accessor_functions {

template<const ComponentAttributeProviders &providers>
inline bool is_builtin(const void * /*owner*/, const AttributeIDRef &attribute_id)
{
  if (attribute_id.is_anonymous()) {
    return false;
  }
  const StringRef name = attribute_id.name();
  return providers.builtin_attribute_providers().contains_as(name);
}

template<const ComponentAttributeProviders &providers>
inline GAttributeReader lookup(const void *owner, const AttributeIDRef &attribute_id)
{
  if (!attribute_id.is_anonymous()) {
    const StringRef name = attribute_id.name();
    if (const BuiltinAttributeProvider *provider =
            providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
    {
      return provider->try_get_for_read(owner);
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    GAttributeReader attribute = provider->try_get_for_read(owner, attribute_id);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

template<const ComponentAttributeProviders &providers>
inline bool for_all(const void *owner,
                    FunctionRef<bool(const AttributeIDRef &, const AttributeMetaData &)> fn)
{
  Set<AttributeIDRef> handled_attribute_ids;
  for (const BuiltinAttributeProvider *provider : providers.builtin_attribute_providers().values())
  {
    if (provider->exists(owner)) {
      AttributeMetaData meta_data{provider->domain(), provider->data_type()};
      if (!fn(provider->name(), meta_data)) {
        return false;
      }
      handled_attribute_ids.add_new(provider->name());
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    const bool continue_loop = provider->foreach_attribute(
        owner, [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          if (handled_attribute_ids.add(attribute_id)) {
            return fn(attribute_id, meta_data);
          }
          return true;
        });
    if (!continue_loop) {
      return false;
    }
  }
  return true;
}

template<const ComponentAttributeProviders &providers>
inline AttributeValidator lookup_validator(const void * /*owner*/,
                                           const blender::bke::AttributeIDRef &attribute_id)
{
  if (attribute_id.is_anonymous()) {
    return {};
  }
  const BuiltinAttributeProvider *provider =
      providers.builtin_attribute_providers().lookup_default_as(attribute_id.name(), nullptr);
  if (!provider) {
    return {};
  }
  return provider->validator();
}

template<const ComponentAttributeProviders &providers>
inline bool contains(const void *owner, const blender::bke::AttributeIDRef &attribute_id)
{
  bool found = false;
  for_all<providers>(
      owner,
      [&](const AttributeIDRef &other_attribute_id, const AttributeMetaData & /* meta_data */) {
        if (attribute_id == other_attribute_id) {
          found = true;
          return false;
        }
        return true;
      });
  return found;
}

template<const ComponentAttributeProviders &providers>
inline std::optional<AttributeMetaData> lookup_meta_data(const void *owner,
                                                         const AttributeIDRef &attribute_id)
{
  std::optional<AttributeMetaData> meta_data;
  for_all<providers>(
      owner,
      [&](const AttributeIDRef &other_attribute_id, const AttributeMetaData &other_meta_data) {
        if (attribute_id == other_attribute_id) {
          meta_data = other_meta_data;
          return false;
        }
        return true;
      });
  return meta_data;
}

template<const ComponentAttributeProviders &providers>
inline GAttributeWriter lookup_for_write(void *owner, const AttributeIDRef &attribute_id)
{
  if (!attribute_id.is_anonymous()) {
    const StringRef name = attribute_id.name();
    if (const BuiltinAttributeProvider *provider =
            providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
    {
      return provider->try_get_for_write(owner);
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    GAttributeWriter attribute = provider->try_get_for_write(owner, attribute_id);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

template<const ComponentAttributeProviders &providers>
inline bool remove(void *owner, const AttributeIDRef &attribute_id)
{
  if (!attribute_id.is_anonymous()) {
    const StringRef name = attribute_id.name();
    if (const BuiltinAttributeProvider *provider =
            providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
    {
      return provider->try_delete(owner);
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    if (provider->try_delete(owner, attribute_id)) {
      return true;
    }
  }
  return false;
}

template<const ComponentAttributeProviders &providers>
inline bool add(void *owner,
                const AttributeIDRef &attribute_id,
                eAttrDomain domain,
                eCustomDataType data_type,
                const AttributeInit &initializer)
{
  if (contains<providers>(owner, attribute_id)) {
    return false;
  }
  if (!attribute_id.is_anonymous()) {
    const StringRef name = attribute_id.name();
    if (const BuiltinAttributeProvider *provider =
            providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
    {
      if (provider->domain() != domain) {
        return false;
      }
      if (provider->data_type() != data_type) {
        return false;
      }
      return provider->try_create(owner, initializer);
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    if (provider->try_create(owner, attribute_id, domain, data_type, initializer)) {
      return true;
    }
  }
  return false;
}

template<const ComponentAttributeProviders &providers>
inline AttributeAccessorFunctions accessor_functions_for_providers()
{
  return AttributeAccessorFunctions{contains<providers>,
                                    lookup_meta_data<providers>,
                                    nullptr,
                                    nullptr,
                                    is_builtin<providers>,
                                    lookup<providers>,
                                    nullptr,
                                    for_all<providers>,
                                    lookup_validator<providers>,
                                    lookup_for_write<providers>,
                                    remove<providers>,
                                    add<providers>};
}

}  // namespace attribute_accessor_functions

}  // namespace blender::bke
