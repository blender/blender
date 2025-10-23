/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_storage.hh"
#include "BKE_pointcloud.hh"

#include "attribute_storage_access.hh"

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

static const auto &changed_tags()
{
  static Map<StringRef, AttrUpdateOnChange> attributes{{"position", tag_position_changed},
                                                       {"radius", tag_radius_changed}};
  return attributes;
}

static const auto &builtin_attributes()
{
  static auto attributes = []() {
    Map<StringRef, AttrBuiltinInfo> map;

    AttrBuiltinInfo position(bke::AttrDomain::Point, bke::AttrType::Float3);
    position.deletable = false;
    map.add_new("position", std::move(position));

    AttrBuiltinInfo radius(bke::AttrDomain::Point, bke::AttrType::Float);
    map.add_new("radius", std::move(radius));

    return map;
  }();
  return attributes;
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
    const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name);
    if (!info) {
      return std::nullopt;
    }
    return AttributeDomainAndType{info->domain, info->type};
  };
  fn.get_builtin_default = [](const void * /*owner*/, StringRef name) -> GPointer {
    const AttrBuiltinInfo &info = builtin_attributes().lookup(name);
    return info.default_value;
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
      AttributeIter iter(attribute.name(), attribute.domain(), attribute.data_type(), get_fn);
      iter.is_builtin = builtin_attributes().contains(attribute.name());
      iter.accessor = &accessor;
      fn(iter);
      return !iter.is_stopped();
    });
  };
  fn.lookup_validator = [](const void * /*owner*/, const StringRef name) -> AttributeValidator {
    const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name);
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
    return attribute_to_writer(&pointcloud, changed_tags(), pointcloud.totpoint, *attribute);
  };
  fn.remove = [](void *owner, const StringRef name) -> bool {
    PointCloud &pointcloud = *static_cast<PointCloud *>(owner);
    AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    if (const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (!info->deletable) {
        return false;
      }
    }
    const std::optional<AttrUpdateOnChange> fn = changed_tags().lookup_try(name);
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
              const bke::AttrType type,
              const AttributeInit &initializer) {
    PointCloud &pointcloud = *static_cast<PointCloud *>(owner);
    const int domain_size = pointcloud.totpoint;
    AttributeStorage &storage = pointcloud.attribute_storage.wrap();
    if (const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (info->domain != domain || info->type != type) {
        return false;
      }
    }
    if (storage.lookup(name)) {
      return false;
    }
    storage.add(name, domain, type, attribute_init_to_data(type, domain_size, initializer));
    if (initializer.type != AttributeInit::Type::Construct) {
      if (const std::optional<AttrUpdateOnChange> fn = changed_tags().lookup_try(name)) {
        (*fn)(owner);
      }
    }
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
