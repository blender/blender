/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"

namespace blender::bke {

using AttrUpdateOnChange = void (*)(void *owner);

struct AttrBuiltinInfo {
  AttrDomain domain;
  AttrType type;
  GPointer default_value = {};
  AttributeValidator validator = {};
  bool deletable = true;
  AttrBuiltinInfo(AttrDomain domain, AttrType type) : domain(domain), type(type) {}
};

GAttributeReader attribute_to_reader(const Attribute &attribute,
                                     const AttrDomain domain,
                                     const int64_t domain_size);

GAttributeWriter attribute_to_writer(void *owner,
                                     const Map<StringRef, AttrUpdateOnChange> &changed_tags,
                                     const int64_t domain_size,
                                     Attribute &attribute);

Attribute::DataVariant attribute_init_to_data(const bke::AttrType data_type,
                                              const int64_t domain_size,
                                              const AttributeInit &initializer);

GVArray get_varray_attribute(const AttributeStorage &storage,
                             AttrDomain domain,
                             const CPPType &cpp_type,
                             StringRef name,
                             int64_t domain_size,
                             const void *default_value);

template<typename T>
inline VArray<T> get_varray_attribute(const AttributeStorage &storage,
                                      const AttrDomain domain,
                                      const StringRef name,
                                      const int64_t domain_size,
                                      const T &default_value)
{
  GVArray varray = get_varray_attribute(
      storage, domain, CPPType::get<T>(), name, domain_size, &default_value);
  return varray.typed<T>();
}

GSpan get_span_attribute(const AttributeStorage &storage,
                         AttrDomain domain,
                         const CPPType &cpp_type,
                         StringRef name,
                         const int64_t domain_size);

template<typename T>
inline Span<T> get_span_attribute(const AttributeStorage &storage,
                                  const AttrDomain domain,
                                  const StringRef name,
                                  const int64_t domain_size)
{
  const GSpan span = get_span_attribute(storage, domain, CPPType::get<T>(), name, domain_size);
  return span.typed<T>();
}

GMutableSpan get_mutable_attribute(AttributeStorage &storage,
                                   const AttrDomain domain,
                                   const CPPType &cpp_type,
                                   const StringRef name,
                                   const int64_t domain_size,
                                   const void *default_value);

template<typename T>
inline MutableSpan<T> get_mutable_attribute(AttributeStorage &storage,
                                            const AttrDomain domain,
                                            const StringRef name,
                                            const int64_t domain_size,
                                            const T &default_value = T())
{
  const GMutableSpan span = get_mutable_attribute(
      storage, domain, CPPType::get<T>(), name, domain_size, &default_value);
  return span.typed<T>();
}

}  // namespace blender::bke
