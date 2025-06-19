/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_pointcloud.hh"

#include "attribute_access_intern.hh"

namespace blender::bke {

static void tag_position_changed(void *owner)
{
  PointCloud &points = *static_cast<PointCloud *>(owner);
  points.tag_positions_changed();
}

static void tag_radius_changed(void *owner)
{
  PointCloud &points = *static_cast<PointCloud *>(owner);
  points.tag_radii_changed();
}

using UpdateOnChange = void (*)(void *owner);

static const auto &changed_tags()
{
  static Map<StringRef, UpdateOnChange> attributes{{"position", tag_position_changed},
                                                   {"radius", tag_radius_changed}};
  return attributes;
}

namespace {

struct BuiltinInfo {
  bke::AttrDomain domain;
  bke::AttrType type;
  GPointer default_value = {};
  AttributeValidator validator = {};
  bool deletable = false;
  BuiltinInfo(bke::AttrDomain domain, bke::AttrType type) : domain(domain), type(type) {}
};

}  // namespace

static const auto &builtin_attributes()
{
  static auto attributes = []() {
    Map<StringRef, BuiltinInfo> map;

    BuiltinInfo position(bke::AttrDomain::Point, bke::AttrType::Float3);
    position.deletable = false;
    map.add_new("position", std::move(position));

    BuiltinInfo radius(bke::AttrDomain::Point, bke::AttrType::Float);
    map.add_new("radius", std::move(radius));

    return map;
  }();
  return attributes;
}

static GAttributeReader attribute_to_reader(const Attribute &attribute,
                                            const AttrDomain domain,
                                            const int64_t domain_size)
{
  const CPPType &cpp_type = attribute_type_to_cpp_type(attribute.data_type());
  switch (attribute.storage_type()) {
    case AttrStorageType::Array: {
      const auto &data = std::get<Attribute::ArrayData>(attribute.data());
      return GAttributeReader{GVArray::ForSpan(GSpan(cpp_type, data.data, data.size)),
                              domain,
                              data.sharing_info.get()};
    }
    case AttrStorageType::Single: {
      const auto &data = std::get<Attribute::SingleData>(attribute.data());
      return GAttributeReader{GVArray::ForSingleRef(cpp_type, domain_size, data.value),
                              domain,
                              data.sharing_info.get()};
    }
  }
  BLI_assert_unreachable();
  return {};
}

static GAttributeWriter attribute_to_writer(PointCloud &pointcloud,
                                            const int64_t domain_size,
                                            Attribute &attribute)
{
  const CPPType &cpp_type = attribute_type_to_cpp_type(attribute.data_type());
  switch (attribute.storage_type()) {
    case AttrStorageType::Array: {
      auto &data = std::get<Attribute::ArrayData>(attribute.data_for_write());
      BLI_assert(data.size == domain_size);

      std::function<void()> tag_modified_fn;
      if (const UpdateOnChange update_fn = changed_tags().lookup_default(attribute.name(),
                                                                         nullptr))
      {
        tag_modified_fn = [pointcloud = &pointcloud, update_fn]() { update_fn(pointcloud); };
      };

      return GAttributeWriter{
          GVMutableArray::ForSpan(GMutableSpan(cpp_type, data.data, domain_size)),
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

static Attribute::DataVariant attribute_init_to_data(const bke::AttrType data_type,
                                                     const int64_t domain_size,
                                                     const AttributeInit &initializer)
{
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::ForConstructed(type, domain_size);
    }
    case AttributeInit::Type::DefaultValue: {
      const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
      return Attribute::ArrayData::ForDefaultValue(type, domain_size);
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

static constexpr AttributeAccessorFunctions get_pointcloud_accessor_functions()
{
  AttributeAccessorFunctions fn{};
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return domain == AttrDomain::Point;
  };
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    return domain == AttrDomain::Point ? static_cast<const PointCloud *>(owner)->totpoint : 0;
  };
  fn.builtin_domain_and_type = [](const void * /*owner*/,
                                  const StringRef name) -> std::optional<AttributeDomainAndType> {
    const BuiltinInfo *info = builtin_attributes().lookup_ptr(name);
    if (!info) {
      return std::nullopt;
    }
    const std::optional<eCustomDataType> cd_type = attr_type_to_custom_data_type(info->type);
    BLI_assert(cd_type.has_value());
    return AttributeDomainAndType{info->domain, *cd_type};
  };
  fn.lookup = [](const void *owner, const StringRef name) -> GAttributeReader {
    const PointCloud &pointcloud = *static_cast<const PointCloud *>(owner);
    const AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    const Attribute *attribute = storage.lookup(name);
    if (!attribute) {
      return {};
    }
    return attribute_to_reader(*attribute, AttrDomain::Point, pointcloud.totpoint);
  };
  fn.adapt_domain = [](const void * /*owner*/,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) {
    if (from_domain == to_domain && from_domain == AttrDomain::Point) {
      return varray;
    }
    return GVArray{};
  };
  fn.foreach_attribute = [](const void *owner,
                            const FunctionRef<void(const AttributeIter &)> fn,
                            const AttributeAccessor &accessor) {
    const PointCloud &pointcloud = *static_cast<const PointCloud *>(owner);
    const AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    storage.foreach_with_stop([&](const Attribute &attribute) {
      const auto get_fn = [&]() {
        return attribute_to_reader(attribute, AttrDomain::Point, pointcloud.totpoint);
      };
      const std::optional<eCustomDataType> cd_type = attr_type_to_custom_data_type(
          attribute.data_type());
      BLI_assert(cd_type.has_value());
      AttributeIter iter(attribute.name(), attribute.domain(), *cd_type, get_fn);
      iter.is_builtin = builtin_attributes().contains(attribute.name());
      iter.accessor = &accessor;
      fn(iter);
      return !iter.is_stopped();
    });
  };
  fn.lookup_validator = [](const void * /*owner*/, const StringRef name) -> AttributeValidator {
    const BuiltinInfo *info = builtin_attributes().lookup_ptr(name);
    if (!info) {
      return {};
    }
    return info->validator;
  };
  fn.lookup_for_write = [](void *owner, const StringRef name) -> GAttributeWriter {
    PointCloud &pointcloud = *static_cast<PointCloud *>(owner);
    AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    Attribute *attribute = storage.lookup(name);
    if (!attribute) {
      return {};
    }
    return attribute_to_writer(pointcloud, pointcloud.totpoint, *attribute);
  };
  fn.remove = [](void *owner, const StringRef name) -> bool {
    PointCloud &pointcloud = *static_cast<PointCloud *>(owner);
    AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    if (const BuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (!info->deletable) {
        return false;
      }
    }
    const std::optional<UpdateOnChange> fn = changed_tags().lookup_try(name);
    const bool removed = storage.remove(name);
    if (!removed) {
      return false;
    }
    if (fn) {
      (*fn)(owner);
    }
    return true;
  };
  fn.add = [](void *owner,
              const StringRef name,
              const AttrDomain domain,
              const eCustomDataType data_type,
              const AttributeInit &initializer) {
    PointCloud &pointcloud = *static_cast<PointCloud *>(owner);
    const int domain_size = pointcloud.totpoint;
    AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    const std::optional<AttrType> type = custom_data_type_to_attr_type(data_type);
    BLI_assert(type.has_value());
    if (const BuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (info->domain != domain || info->type != type) {
        return false;
      }
    }
    if (storage.lookup(name)) {
      return false;
    }
    Attribute::DataVariant data = attribute_init_to_data(*type, domain_size, initializer);
    storage.add(name, domain, *type, std::move(data));
    return true;
  };

  return fn;
}

const AttributeAccessorFunctions &pointcloud_attribute_accessor_functions()
{
  static constexpr AttributeAccessorFunctions fn = get_pointcloud_accessor_functions();
  return fn;
}

}  // namespace blender::bke
