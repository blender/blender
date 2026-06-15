/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_deform.hh"

#include "BLI_listbase.hh"

#include "DNA_object_types.h"

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
      /* Just convert the stored type to an array for modification. It might not make sense to
       * implement editing of single values at this level. */
      const auto &data = std::get<Attribute::SingleData>(attribute.data());
      const GPointer value(cpp_type, data.value);
      attribute.assign_data(Attribute::ArrayData::from_value(value, domain_size));
      return attribute_to_writer(owner, changed_tags, domain_size, attribute);
    }
  }
  BLI_assert_unreachable();
  return {};
}

Attribute::DataVariant attribute_init_to_data(const bke::AttrType data_type,
                                              const int64_t domain_size,
                                              const AttributeInit &initializer,
                                              const bool require_array_data)
{
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::from_constructed(type, domain_size);
    }
    case AttributeInit::Type::Value: {
      const auto &init = static_cast<const AttributeInitValue &>(initializer);
      BLI_assert(*init.value.type() == bke::attribute_type_to_cpp_type(data_type));
      if (require_array_data) {
        return Attribute::ArrayData::from_value(init.value, domain_size);
      }
      return Attribute::SingleData::from_value(init.value);
    }
    case AttributeInit::Type::DefaultValue: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::from_default_value(type, domain_size);
    }
    case AttributeInit::Type::VArray: {
      const auto &init = static_cast<const AttributeInitVArray &>(initializer);
      const GVArray &varray = init.varray;
      BLI_assert(varray.size() == domain_size);
      if (!require_array_data) {
        const CommonVArrayInfo &info = varray.common_info();
        if (info.type == CommonVArrayInfo::Type::Single) {
          return Attribute::SingleData::from_value(GPointer(varray.type(), info.data));
        }
      }
      const CPPType &type = varray.type();
      Attribute::ArrayData data = Attribute::ArrayData::from_uninitialized(type, domain_size);
      varray.materialize_to_uninitialized(varray.index_range(), data.data);
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

std::optional<GSpan> get_span_attribute(const AttributeStorage &storage,
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

bool try_delete_vertex_group(ListBaseT<bDeformGroup> &vertex_groups,
                             const StringRef name,
                             FunctionRef<MutableSpan<MDeformVert>()> get_mutable_dverts)
{
  int index;
  bDeformGroup *group;
  if (!BKE_defgroup_listbase_name_find(&vertex_groups, name, &index, &group)) {
    return false;
  }
  BLI_remlink(&vertex_groups, group);
  MEM_delete(group);
  MutableSpan<MDeformVert> dverts = get_mutable_dverts();
  if (dverts.is_empty()) {
    return true;
  }
  remove_defgroup_index(dverts, index);
  return true;
}

static bDeformGroup *find_vertex_group(ListBaseT<bDeformGroup> &vertex_groups,
                                       const StringRef name)
{
  for (bDeformGroup &group : vertex_groups) {
    if (group.name == name) {
      return &group;
    }
  }
  return nullptr;
}

static void ensure_array_storage(Attribute &attr,
                                 const FunctionRef<int(AttrDomain)> domain_size_fn)
{
  switch (attr.storage_type()) {
    case AttrStorageType::Single: {
      const auto &data = std::get<Attribute::SingleData>(attr.data());
      const GPointer value(attribute_type_to_cpp_type(attr.data_type()), data.value);
      attr.assign_data(Attribute::ArrayData::from_value(value, domain_size_fn(attr.domain())));
      break;
    }
    case AttrStorageType::Array:
      break;
  }
}

static std::variant<std::monostate, Attribute *, bDeformGroup *> find_attr_or_group(
    AttributeStorage &storage,
    std::optional<ListBaseT<bDeformGroup> *> vertex_groups,
    const StringRef name)
{
  if (Attribute *attr = storage.lookup(name)) {
    return attr;
  }
  if (vertex_groups) {
    if (bDeformGroup *group = find_vertex_group(**vertex_groups, name)) {
      return group;
    }
  }
  return std::monostate();
}

Set<StringRef> rename_attributes(AttributeStorage &storage,
                                 const Map<StringRef, StringRef> &name_map,
                                 const bool overwrite,
                                 const Map<StringRef, AttrBuiltinInfo> &builtin_attributes,
                                 const Set<StringRef> &array_storage_required,
                                 const FunctionRef<int(AttrDomain)> domain_size_fn,
                                 std::optional<ListBaseT<bDeformGroup> *> vertex_groups,
                                 FunctionRef<MutableSpan<MDeformVert>()> get_mutable_dverts)
{
  Set<StringRef> names_to_remove;
  Set<StringRef> failed;
  Map<Attribute *, StringRef> map;
  Map<bDeformGroup *, StringRef> vertex_group_map;
  map.reserve(name_map.size());
  for (const auto &[old_name, new_name] : name_map.items()) {
    if (new_name.is_empty()) {
      failed.add_new(old_name);
      continue;
    }
    const std::variant<std::monostate, Attribute *, bDeformGroup *> old_attr = find_attr_or_group(
        storage, vertex_groups, old_name);
    if (std::holds_alternative<std::monostate>(old_attr)) {
      failed.add_new(old_name);
      continue;
    }
    const bke::AttrDomain old_domain = std::holds_alternative<Attribute *>(old_attr) ?
                                           std::get<Attribute *>(old_attr)->domain() :
                                           bke::AttrDomain::Point;
    const bke::AttrType old_type = std::holds_alternative<Attribute *>(old_attr) ?
                                       std::get<Attribute *>(old_attr)->data_type() :
                                       bke::AttrType::Float;

    if (const AttrBuiltinInfo *old_info = builtin_attributes.lookup_ptr(old_name)) {
      if (!old_info->deletable) {
        failed.add_new(old_name);
        continue;
      }
    }
    if (const AttrBuiltinInfo *new_info = builtin_attributes.lookup_ptr(new_name)) {
      if (new_info->domain != old_domain || new_info->type != old_type) {
        failed.add_new(old_name);
        continue;
      }
    }
    if (array_storage_required.contains(new_name)) {
      if (std::holds_alternative<Attribute *>(old_attr)) {
        ensure_array_storage(*std::get<Attribute *>(old_attr), domain_size_fn);
      }
      else {
        /* Converting vertex groups to real attributes remains unsupported here. */
        failed.add_new(old_name);
        continue;
      }
    }
    if (overwrite) {
      names_to_remove.add_new(new_name);
    }
    else {
      const std::variant<std::monostate, Attribute *, bDeformGroup *> new_attr =
          find_attr_or_group(storage, vertex_groups, new_name);
      if (!std::holds_alternative<std::monostate>(new_attr)) {
        failed.add_new(old_name);
        continue;
      }
    }

    if (std::holds_alternative<Attribute *>(old_attr)) {
      map.add_new(std::get<Attribute *>(old_attr), new_name);
    }
    else {
      vertex_group_map.add_new(std::get<bDeformGroup *>(old_attr), new_name);
    }
  }

  if (!names_to_remove.is_empty()) {
    const int start_attrs_num = storage.count();
    storage.remove(names_to_remove);
    if (vertex_groups && (start_attrs_num - storage.count()) != names_to_remove.size()) {
      /* Also remove vertex groups if names weren't all removed as attributes. */
      for (bDeformGroup &defgroup : (*vertex_groups)->items_mutable()) {
        if (names_to_remove.contains_as(defgroup.name)) {
          try_delete_vertex_group(**vertex_groups, defgroup.name, get_mutable_dverts);
        }
      }
    }
  }

  if (!map.is_empty()) {
    storage.rename(map);
  }
  if (vertex_groups && !vertex_group_map.is_empty()) {
    for (bDeformGroup &defgroup : **vertex_groups) {
      if (const std::optional<StringRef> name = vertex_group_map.lookup_try(&defgroup)) {
        name->copy_utf8_truncated(defgroup.name);
      }
    }
  }
  return failed;
}

}  // namespace blender::bke
