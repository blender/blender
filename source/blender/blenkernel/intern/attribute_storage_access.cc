/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

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
  const GList &data = attribute.data();
  BLI_assert(data.size() == domain_size);
  UNUSED_VARS_NDEBUG(domain_size);
  const ImplicitSharingInfo *sharing_info = std::visit(
      [](const auto &storage) { return storage.sharing_info.get(); }, data.data());
  return GAttributeReader{data.varray(), domain, sharing_info};
}

GAttributeWriter attribute_to_writer(void *owner,
                                     const Map<StringRef, AttrUpdateOnChange> &changed_tags,
                                     const int64_t domain_size,
                                     Attribute &attribute)
{
  GList &data = attribute.data();
  BLI_assert(data.size() == domain_size);
  UNUSED_VARS_NDEBUG(domain_size);
  /* Just convert the stored type to an array for modification. It might not make sense to
   * implement editing of single values at this level. */
  data.ensure_array_storage();

  std::function<void()> tag_modified_fn;
  if (const AttrUpdateOnChange update_fn = changed_tags.lookup_default(attribute.name(), nullptr))
  {
    tag_modified_fn = [owner, update_fn]() { update_fn(owner); };
  };

  return GAttributeWriter{
      GVMutableArray::from_span(std::get<GMutableSpan>(data.values_for_write())),
      attribute.domain(),
      std::move(tag_modified_fn)};
}

GList attribute_init_to_list(const bke::AttrType data_type,
                             const int64_t domain_size,
                             const AttributeInit &initializer,
                             const bool require_array_data)
{
  const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      return GList(type, GList::ArrayData::ForConstructed(type, domain_size), domain_size);
    }
    case AttributeInit::Type::Value: {
      const auto &init = static_cast<const AttributeInitValue &>(initializer);
      BLI_assert(*init.value.type() == type);
      if (require_array_data) {
        return GList(type, GList::ArrayData::ForValue(init.value, domain_size), domain_size);
      }
      return GList(type, GList::SingleData::ForValue(init.value), domain_size);
    }
    case AttributeInit::Type::DefaultValue: {
      return Attribute::default_array_data(type, domain_size);
    }
    case AttributeInit::Type::VArray: {
      const auto &init = static_cast<const AttributeInitVArray &>(initializer);
      const GVArray &varray = init.varray;
      BLI_assert(varray.size() == domain_size);
      if (!require_array_data) {
        const CommonVArrayInfo &info = varray.common_info();
        if (info.type == CommonVArrayInfo::Type::Single) {
          return GList(
              type, GList::SingleData::ForValue(GPointer(varray.type(), info.data)), domain_size);
        }
      }
      GArray<> data(varray.type(), domain_size, NoInitialization());
      varray.materialize_to_uninitialized(varray.index_range(), data.data());
      return GList(std::move(data));
    }
    case AttributeInit::Type::Shared: {
      const auto &init = static_cast<const AttributeInitShared &>(initializer);
      GList::ArrayData data;
      data.data = init.data;
      data.sharing_info = ImplicitSharingPtr<>(init.sharing_info);
      data.sharing_info->add_user();
      return GList(type, std::move(data), domain_size);
    }
  }
  BLI_assert_unreachable();
  return GList(type);
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
  BLI_assert(attr->data().size() == domain_size);
  return attr->data().varray();
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
  const std::variant<GSpan, GPointer> values = attr->data().values();
  if (const auto *span = std::get_if<GSpan>(&values)) {
    BLI_assert(span->size() == domain_size);
    UNUSED_VARS_NDEBUG(domain_size);
    return GSpan(cpp_type, span->data(), span->size());
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
      GList &data = attr->data();
      BLI_assert(data.size() == domain_size);
      data.ensure_array_storage();
      return std::get<GMutableSpan>(data.values_for_write());
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
      GList(cpp_type,
            GList::ArrayData::ForValue({cpp_type, default_value}, domain_size),
            domain_size));
  return std::get<GMutableSpan>(attr.data().values_for_write());
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
        Attribute &attr = *std::get<Attribute *>(old_attr);
        BLI_assert(attr.data().size() == domain_size_fn(attr.domain()));
        UNUSED_VARS_NDEBUG(domain_size_fn);
        attr.data().ensure_array_storage();
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
