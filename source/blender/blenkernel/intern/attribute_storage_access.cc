/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"

#include "attribute_storage_access.hh"

namespace blender::bke {

GAttributeReader attribute_to_reader(const Attribute &attribute,
                                     const AttrDomain domain,
                                     const int64_t domain_size)
{
  const CPPType &cpp_type = attribute_type_to_cpp_type(attribute.data_type());
  switch (attribute.storage_type()) {
    case AttrStorageType::Array: {
      const auto &data = std::get<Attribute::ArrayData>(attribute.data());
      return GAttributeReader{GVArray::from_span(GSpan(cpp_type, data.data, data.size)),
                              domain,
                              data.sharing_info.get()};
    }
    case AttrStorageType::Single: {
      const auto &data = std::get<Attribute::SingleData>(attribute.data());
      return GAttributeReader{GVArray::from_single_ref(cpp_type, domain_size, data.value),
                              domain,
                              data.sharing_info.get()};
    }
  }
  BLI_assert_unreachable();
  return {};
}

GAttributeWriter attribute_to_writer(void *owner,
                                     const Map<StringRef, AttrUpdateOnChange> &changed_tags,
                                     const int64_t domain_size,
                                     Attribute &attribute)
{
  const CPPType &cpp_type = attribute_type_to_cpp_type(attribute.data_type());
  switch (attribute.storage_type()) {
    case AttrStorageType::Array: {
      auto &data = std::get<Attribute::ArrayData>(attribute.data_for_write());
      BLI_assert(data.size == domain_size);

      std::function<void()> tag_modified_fn;
      if (const AttrUpdateOnChange update_fn = changed_tags.lookup_default(attribute.name(),
                                                                           nullptr))
      {
        tag_modified_fn = [owner, update_fn]() { update_fn(owner); };
      };

      return GAttributeWriter{
          GVMutableArray::from_span(GMutableSpan(cpp_type, data.data, domain_size)),
          attribute.domain(),
          std::move(tag_modified_fn)};
    }
    case AttrStorageType::Single: {
      /* Not yet implemented. */
      BLI_assert_unreachable();
    }
  }
  BLI_assert_unreachable();
  return {};
}

Attribute::DataVariant attribute_init_to_data(const bke::AttrType data_type,
                                              const int64_t domain_size,
                                              const AttributeInit &initializer)
{
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::from_constructed(type, domain_size);
    }
    case AttributeInit::Type::DefaultValue: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::from_default_value(type, domain_size);
    }
    case AttributeInit::Type::VArray: {
      const auto &init = static_cast<const AttributeInitVArray &>(initializer);
      const GVArray &varray = init.varray;
      BLI_assert(varray.size() == domain_size);
      const CPPType &type = varray.type();
      Attribute::ArrayData data;
      data.data = MEM_malloc_arrayN_aligned(domain_size, type.size, type.alignment, __func__);
      varray.materialize_to_uninitialized(varray.index_range(), data.data);
      data.size = domain_size;
      data.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data.data));
      return data;
    }
    case AttributeInit::Type::MoveArray: {
      const auto &init = static_cast<const AttributeInitMoveArray &>(initializer);
      Attribute::ArrayData data;
      data.data = init.data;
      data.size = domain_size;
      data.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data.data));
      return data;
    }
    case AttributeInit::Type::Shared: {
      const auto &init = static_cast<const AttributeInitShared &>(initializer);
      Attribute::ArrayData data;
      data.data = const_cast<void *>(init.data);
      data.size = domain_size;
      data.sharing_info = ImplicitSharingPtr<>(init.sharing_info);
      data.sharing_info->add_user();
      return data;
    }
  }
  BLI_assert_unreachable();
  return {};
}

GVArray get_varray_attribute(const AttributeStorage &storage,
                             AttrDomain domain,
                             const CPPType &cpp_type,
                             StringRef name,
                             int64_t domain_size,
                             const void *default_value)
{
  const bke::Attribute *attr = storage.wrap().lookup(name);

  const auto return_default = [&]() {
    return GVArray::from_single(cpp_type, domain_size, default_value);
  };

  if (!attr) {
    return return_default();
  }
  if (attr->domain() != domain) {
    return return_default();
  }
  if (attr->data_type() != cpp_type_to_attribute_type(cpp_type)) {
    return return_default();
  }
  switch (attr->storage_type()) {
    case bke::AttrStorageType::Array: {
      const auto &data = std::get<bke::Attribute::ArrayData>(attr->data());
      const GSpan span(cpp_type, data.data, data.size);
      return GVArray::from_span(span);
    }
    case bke::AttrStorageType::Single: {
      const auto &data = std::get<bke::Attribute::SingleData>(attr->data());
      return GVArray::from_single(cpp_type, domain_size, data.value);
    }
  }
  return return_default();
}

GSpan get_span_attribute(const AttributeStorage &storage,
                         const AttrDomain domain,
                         const CPPType &cpp_type,
                         const StringRef name,
                         const int64_t domain_size)
{
  const bke::Attribute *attr = storage.wrap().lookup(name);
  if (!attr) {
    return {};
  }
  if (attr->domain() != domain) {
    return {};
  }
  if (const auto *array_data = std::get_if<bke::Attribute::ArrayData>(&attr->data())) {
    BLI_assert(array_data->size == domain_size);
    UNUSED_VARS_NDEBUG(domain_size);
    return GSpan(cpp_type, array_data->data, array_data->size);
  }
  return {};
}

GMutableSpan get_mutable_attribute(AttributeStorage &storage,
                                   const AttrDomain domain,
                                   const CPPType &cpp_type,
                                   const StringRef name,
                                   const int64_t domain_size,
                                   const void *custom_default_value)
{
  if (domain_size <= 0) {
    return {};
  }
  const bke::AttrType type = bke::cpp_type_to_attribute_type(cpp_type);
  if (bke::Attribute *attr = storage.wrap().lookup(name)) {
    if (attr->data_type() == type) {
      if (const auto *single_data = std::get_if<bke::Attribute::SingleData>(&attr->data())) {
        /* Convert single value storage to array storage. */
        const GPointer g_value(cpp_type, single_data->value);
        attr->assign_data(bke::Attribute::ArrayData::from_value(g_value, domain_size));
      }
      auto &array_data = std::get<bke::Attribute::ArrayData>(attr->data_for_write());
      return GMutableSpan(cpp_type, array_data.data, domain_size);
    }
    /* The attribute has the wrong type. This shouldn't happen for builtin attributes, but just
     * in case, remove it. */
    storage.wrap().remove(name);
  }
  const void *default_value = custom_default_value ? custom_default_value :
                                                     cpp_type.default_value();
  bke::Attribute &attr = storage.wrap().add(
      name,
      domain,
      type,
      bke::Attribute::ArrayData::from_value({cpp_type, default_value}, domain_size));
  auto &array_data = std::get<bke::Attribute::ArrayData>(attr.data_for_write());
  BLI_assert(array_data.size == domain_size);
  return GMutableSpan(cpp_type, array_data.data, domain_size);
}

}  // namespace blender::bke
