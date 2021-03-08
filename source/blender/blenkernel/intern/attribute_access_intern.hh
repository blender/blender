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

namespace blender::bke {

class ConstantReadAttribute final : public ReadAttribute {
 private:
  void *value_;

 public:
  ConstantReadAttribute(AttributeDomain domain,
                        const int64_t size,
                        const CPPType &type,
                        const void *value)
      : ReadAttribute(domain, type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_to_uninitialized(value, value_);
  }

  ~ConstantReadAttribute() override
  {
    this->cpp_type_.destruct(value_);
    MEM_freeN(value_);
  }

  void get_internal(const int64_t UNUSED(index), void *r_value) const override
  {
    this->cpp_type_.copy_to_uninitialized(value_, r_value);
  }

  void initialize_span() const override
  {
    const int element_size = cpp_type_.size();
    array_buffer_ = MEM_mallocN_aligned(size_ * element_size, cpp_type_.alignment(), __func__);
    array_is_temporary_ = true;
    cpp_type_.fill_uninitialized(value_, array_buffer_, size_);
  }
};

template<typename T> class ArrayReadAttribute final : public ReadAttribute {
 private:
  Span<T> data_;

 public:
  ArrayReadAttribute(AttributeDomain domain, Span<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void initialize_span() const override
  {
    /* The data will not be modified, so this const_cast is fine. */
    array_buffer_ = const_cast<T *>(data_.data());
    array_is_temporary_ = false;
  }
};

template<typename T> class OwnedArrayReadAttribute final : public ReadAttribute {
 private:
  Array<T> data_;

 public:
  OwnedArrayReadAttribute(AttributeDomain domain, Array<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(std::move(data))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void initialize_span() const override
  {
    /* The data will not be modified, so this const_cast is fine. */
    array_buffer_ = const_cast<T *>(data_.data());
    array_is_temporary_ = false;
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  blender::Span<StructT> data_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }
};

template<typename T> class ArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<T> data_;

 public:
  ArrayWriteAttribute(AttributeDomain domain, MutableSpan<T> data)
      : WriteAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data_[index] = *reinterpret_cast<const T *>(value);
  }

  void initialize_span(const bool UNUSED(write_only)) override
  {
    array_buffer_ = data_.data();
    array_is_temporary_ = false;
  }

  void apply_span_if_necessary() override
  {
    /* Do nothing, because the span contains the attribute itself already. */
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, const ElemT &)>
class DerivedArrayWriteAttribute final : public WriteAttribute {
 private:
  blender::MutableSpan<StructT> data_;

 public:
  DerivedArrayWriteAttribute(AttributeDomain domain, blender::MutableSpan<StructT> data)
      : WriteAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    StructT &struct_value = data_[index];
    const ElemT &typed_value = *reinterpret_cast<const ElemT *>(value);
    SetFunc(struct_value, typed_value);
  }
};

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

  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component) const = 0;
  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component) const = 0;
  virtual bool try_delete(GeometryComponent &component) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component)) const = 0;
  virtual bool exists(const GeometryComponent &component) const = 0;

  blender::StringRefNull name() const
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
  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                            const blender::StringRef attribute_name) const = 0;
  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                              const blender::StringRef attribute_name) const = 0;
  virtual bool try_delete(GeometryComponent &component,
                          const blender::StringRef attribute_name) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component),
                          const blender::StringRef UNUSED(attribute_name),
                          const AttributeDomain UNUSED(domain),
                          const CustomDataType UNUSED(data_type)) const
  {
    /* Some providers should not create new attributes. */
    return false;
  };

  virtual bool foreach_attribute(const GeometryComponent &component,
                                 const AttributeForeachCallback callback) const = 0;
  virtual void supported_domains(blender::Vector<AttributeDomain> &r_domains) const = 0;
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

  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const blender::StringRef attribute_name) const final;

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const blender::StringRef attribute_name) const final;

  bool try_delete(GeometryComponent &component,
                  const blender::StringRef attribute_name) const final;

  bool try_create(GeometryComponent &component,
                  const blender::StringRef attribute_name,
                  const AttributeDomain domain,
                  const CustomDataType data_type) const final;

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final;

  void supported_domains(blender::Vector<AttributeDomain> &r_domains) const final
  {
    r_domains.append_non_duplicates(domain_);
  }

 private:
  template<typename T>
  ReadAttributePtr layer_to_read_attribute(const CustomDataLayer &layer,
                                           const int domain_size) const
  {
    return std::make_unique<ArrayReadAttribute<T>>(
        domain_, Span(static_cast<const T *>(layer.data), domain_size));
  }

  template<typename T>
  WriteAttributePtr layer_to_write_attribute(CustomDataLayer &layer, const int domain_size) const
  {
    return std::make_unique<ArrayWriteAttribute<T>>(
        domain_, MutableSpan(static_cast<T *>(layer.data), domain_size));
  }

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
  using AsReadAttribute = ReadAttributePtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = WriteAttributePtr (*)(void *data, const int domain_size);
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

  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final;
  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final;
  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final;
  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final;
  void supported_domains(Vector<AttributeDomain> &r_domains) const final;
};

/**
 * This provider is used to provide access to builtin attributes. It supports making internal types
 * available as different types. For example, the vertex position attribute is stored as part of
 * the #MVert struct, but is exposed as float3 attribute.
 */
class BuiltinCustomDataLayerProvider final : public BuiltinAttributeProvider {
  using AsReadAttribute = ReadAttributePtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = WriteAttributePtr (*)(void *data, const int domain_size);
  using UpdateOnRead = void (*)(const GeometryComponent &component);
  using UpdateOnWrite = void (*)(GeometryComponent &component);
  const CustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;
  const UpdateOnRead update_on_read_;
  const UpdateOnWrite update_on_write_;

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
                                 const UpdateOnRead update_on_read,
                                 const UpdateOnWrite update_on_write)
      : BuiltinAttributeProvider(
            std::move(attribute_name), domain, attribute_type, creatable, writable, deletable),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute),
        update_on_read_(update_on_read),
        update_on_write_(update_on_write)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component) const final;
  WriteAttributePtr try_get_for_write(GeometryComponent &component) const final;
  bool try_delete(GeometryComponent &component) const final;
  bool try_create(GeometryComponent &component) const final;
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
  blender::Map<std::string, const BuiltinAttributeProvider *> builtin_attribute_providers_;
  /**
   * An ordered list of dynamic attribute providers. The order is important because that is order
   * in which they are checked when an attribute is looked up.
   */
  blender::Vector<const DynamicAttributesProvider *> dynamic_attribute_providers_;
  /**
   * All the domains that are supported by at least one of the providers above.
   */
  blender::Vector<AttributeDomain> supported_domains_;

 public:
  ComponentAttributeProviders(
      blender::Span<const BuiltinAttributeProvider *> builtin_attribute_providers,
      blender::Span<const DynamicAttributesProvider *> dynamic_attribute_providers)
      : dynamic_attribute_providers_(dynamic_attribute_providers)
  {
    blender::Set<AttributeDomain> domains;
    for (const BuiltinAttributeProvider *provider : builtin_attribute_providers) {
      /* Use #add_new to make sure that no two builtin attributes have the same name. */
      builtin_attribute_providers_.add_new(provider->name(), provider);
      supported_domains_.append_non_duplicates(provider->domain());
    }
    for (const DynamicAttributesProvider *provider : dynamic_attribute_providers) {
      provider->supported_domains(supported_domains_);
    }
  }

  const blender::Map<std::string, const BuiltinAttributeProvider *> &builtin_attribute_providers()
      const
  {
    return builtin_attribute_providers_;
  }

  blender::Span<const DynamicAttributesProvider *> dynamic_attribute_providers() const
  {
    return dynamic_attribute_providers_;
  }

  blender::Span<AttributeDomain> supported_domains() const
  {
    return supported_domains_;
  }
};

}  // namespace blender::bke