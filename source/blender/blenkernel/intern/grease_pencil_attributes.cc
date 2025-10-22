/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_storage.hh"
#include "BKE_grease_pencil.hh"

#include "DNA_grease_pencil_types.h"

#include "attribute_storage_access.hh"

namespace blender::bke::greasepencil {

static const auto &changed_tags()
{
  static Map<StringRef, AttrUpdateOnChange> attributes;
  return attributes;
}

static const auto &builtin_attributes()
{
  static auto attributes = []() {
    Map<StringRef, AttrBuiltinInfo> map;
    return map;
  }();
  return attributes;
}

static int get_domain_size(const void *owner, const AttrDomain domain)
{
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
  return domain == AttrDomain::Layer ? grease_pencil.layers().size() : 0;
}

static AttributeAccessorFunctions get_grease_pencil_accessor_functions()
{
  AttributeAccessorFunctions fn{};
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return domain == AttrDomain::Layer;
  };
  fn.domain_size = get_domain_size;
  fn.builtin_domain_and_type = [](const void * /*owner*/, const StringRef /*name*/)
      -> std::optional<AttributeDomainAndType> { return std::nullopt; };
  fn.lookup = [](const void *owner, const StringRef name) -> GAttributeReader {
    const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
    const AttributeStorage &storage = grease_pencil.attribute_storage.wrap();
    const Attribute *attribute = storage.lookup(name);
    if (!attribute) {
      return {};
    }
    const int domain_size = get_domain_size(owner, AttrDomain::Layer);
    return attribute_to_reader(*attribute, AttrDomain::Layer, domain_size);
  };
  fn.get_builtin_default = [](const void * /*owner*/, StringRef name) -> GPointer {
    const AttrBuiltinInfo &info = builtin_attributes().lookup(name);
    return info.default_value;
  };
  fn.adapt_domain = [](const void * /*owner*/,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) {
    if (from_domain == to_domain && from_domain == AttrDomain::Layer) {
      return varray;
    }
    return GVArray{};
  };
  fn.foreach_attribute = [](const void *owner,
                            const FunctionRef<void(const AttributeIter &)> fn,
                            const AttributeAccessor &accessor) {
    const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
    const AttributeStorage &storage = grease_pencil.attribute_storage.wrap();
    storage.foreach_with_stop([&](const Attribute &attribute) {
      const auto get_fn = [&]() {
        const int domain_size = get_domain_size(owner, AttrDomain::Layer);
        return attribute_to_reader(attribute, AttrDomain::Layer, domain_size);
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
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(owner);
    AttributeStorage &storage = grease_pencil.attribute_storage.wrap();
    Attribute *attribute = storage.lookup(name);
    if (!attribute) {
      return {};
    }
    const int domain_size = get_domain_size(owner, AttrDomain::Layer);
    return attribute_to_writer(&grease_pencil, {}, domain_size, *attribute);
  };
  fn.remove = [](void *owner, const StringRef name) -> bool {
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(owner);
    AttributeStorage &storage = grease_pencil.attribute_storage.wrap();
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
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(owner);
    const int domain_size = get_domain_size(owner, domain);
    AttributeStorage &storage = grease_pencil.attribute_storage.wrap();
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

const AttributeAccessorFunctions &get_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_grease_pencil_accessor_functions();
  return fn;
}

}  // namespace blender::bke::greasepencil
