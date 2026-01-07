/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "BKE_node_socket_value.hh"
#include "NOD_geometry_nodes_bundle_fwd.hh"
#include "NOD_geometry_nodes_values.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

struct BundleItemSocketValue {
  /** The type of referenced data. */
  const bke::bNodeSocketType *type;
  bke::SocketValueVariant value;
};

/**
 * Other classes can derive from this to be able to store custom internal data in a bundle.
 */
class BundleItemInternalValueMixin : public ImplicitSharingMixin {
 public:
  /** UI name for the type. */
  virtual StringRefNull type_name() const = 0;
};

struct BundleItemInternalValue {
  ImplicitSharingPtr<BundleItemInternalValueMixin> value;
};

struct BundleItemValue {
  std::variant<BundleItemSocketValue, BundleItemInternalValue> value;

  /**
   * Attempts to cast the stored value to the given type. This may do implicit conversions.
   */
  template<typename T>
  std::optional<T> as_socket_value(const bke::bNodeSocketType &socket_type) const;
  template<typename T> std::optional<T> as() const;

  /**
   * Get a pointer to the underlying stored single value.
   */
  template<typename T> T *as_pointer();
  template<typename T> const T *as_pointer() const;
};

/**
 * A bundle is a map containing keys and their corresponding values.
 *
 * The API also supports working with paths in nested bundles like `root/child/data`.
 */
class Bundle : public ImplicitSharingMixin {
 public:
  using BundleItemMap = Map<std::string, BundleItemValue>;

 private:
  BundleItemMap items_;

 public:
  static BundlePtr create();

  bool add(StringRef key, const BundleItemValue &value);
  void add_new(StringRef key, const BundleItemValue &value);
  void add_override(StringRef key, const BundleItemValue &value);
  bool add_path(StringRef path, const BundleItemValue &value);
  void add_path_new(StringRef path, const BundleItemValue &value);
  void add_path_override(StringRef path, const BundleItemValue &value);

  template<typename T> void add(StringRef key, T value);
  template<typename T> void add_override(StringRef key, T value);
  template<typename T> void add_path(StringRef path, T value);
  template<typename T> void add_path_override(StringRef path, T value);

  bool remove(StringRef key);
  bool remove_path(StringRef path);
  bool remove_path(Span<StringRef> path);
  bool contains(StringRef key) const;
  bool contains_path(StringRef path) const;
  bool contains_path(Span<StringRef> path) const;

  const BundleItemValue *lookup(StringRef key) const;
  const BundleItemValue *lookup_path(Span<StringRef> path) const;
  const BundleItemValue *lookup_path(StringRef path) const;
  template<typename T> std::optional<T> lookup(StringRef key) const;
  template<typename T> std::optional<T> lookup_path(StringRef path) const;

  bool is_empty() const;
  int64_t size() const;

  /** Also see #GeometrySet.ensure_owns_direct_data. */
  void ensure_owns_direct_data();
  bool owns_direct_data() const;

  BundleItemMap::ItemIterator items() const;

  BundlePtr copy() const;

  void delete_self() override;

  /** Create the combined path by inserting '/' between each element. */
  static std::string combine_path(const Span<StringRef> path);

  /* Disallow certain characters so that we can use them to e.g. build a bundle path or
   * expressions referencing multiple bundle items. We might not need all of them in the future,
   * but better reserve them now while we still can. */
  static constexpr StringRefNull forbidden_key_chars = "/*&|\"^~!,{}()+$#@[];:?<>.-%\\=";
  static bool is_valid_key(const StringRef key);
  static bool is_valid_path(const StringRef path);
  static std::optional<Vector<StringRef>> split_path(const StringRef path);
};

template<typename T>
inline std::optional<T> BundleItemValue::as_socket_value(
    const bke::bNodeSocketType &dst_socket_type) const
{
  const BundleItemSocketValue *socket_value = std::get_if<BundleItemSocketValue>(&this->value);
  if (!socket_value) {
    return std::nullopt;
  }
  if (socket_value->type->type == dst_socket_type.type) {
    return socket_value->value.get<T>();
  }
  if (std::optional<bke::SocketValueVariant> converted_value = implicitly_convert_socket_value(
          *socket_value->type, socket_value->value, dst_socket_type))
  {
    return converted_value->get<T>();
  }
  return std::nullopt;
}

template<typename T> inline T *BundleItemValue::as_pointer()
{
  return const_cast<T *>(std::as_const(*this).as_pointer<T>());
}
template<typename T> inline const T *BundleItemValue::as_pointer() const
{
  const BundleItemSocketValue *socket_value = std::get_if<BundleItemSocketValue>(&this->value);
  if (!socket_value) {
    return nullptr;
  }
  if (!socket_value->value.is_single()) {
    return nullptr;
  }
  const GPointer ptr = socket_value->value.get_single_ptr();
  if (!ptr.is_type<T>()) {
    return nullptr;
  }
  return ptr.get<T>();
}

template<typename T> inline const bke::bNodeSocketType *socket_type_info_by_static_type()
{
  if constexpr (fn::is_field_v<T>) {
    const std::optional<eNodeSocketDatatype> socket_type =
        bke::geo_nodes_base_cpp_type_to_socket_type(CPPType::get<typename T::base_type>());
    BLI_assert(socket_type);
    const bke::bNodeSocketType *socket_type_info = bke::node_socket_type_find_static(*socket_type);
    BLI_assert(socket_type_info);
    return socket_type_info;
  }
  else {
    const std::optional<eNodeSocketDatatype> socket_type =
        bke::geo_nodes_base_cpp_type_to_socket_type(CPPType::get<T>());
    if (!socket_type) {
      return nullptr;
    }
    return bke::node_socket_type_find_static(*socket_type);
  }
}

template<typename T> constexpr bool is_valid_internal_bundle_item_type()
{
  if constexpr (is_ImplicitSharingPtr_strong_v<T>) {
    if constexpr (std::is_base_of_v<BundleItemInternalValueMixin, typename T::element_type>) {
      return true;
    }
  }
  return false;
}

template<typename T> inline std::optional<T> BundleItemValue::as() const
{
  if constexpr (is_valid_internal_bundle_item_type<T>()) {
    using SharingInfoT = typename T::element_type;
    const auto *internal_value = std::get_if<BundleItemInternalValue>(&this->value);
    if (!internal_value) {
      return std::nullopt;
    }
    const BundleItemInternalValueMixin *sharing_info = internal_value->value.get();
    const SharingInfoT *converted_value = dynamic_cast<const SharingInfoT *>(sharing_info);
    if (!converted_value) {
      return std::nullopt;
    }
    sharing_info->add_user();
    return ImplicitSharingPtr<SharingInfoT>{converted_value};
  }
  else if constexpr (std::is_same_v<T, ListPtr>) {
    const BundleItemSocketValue *socket_value = std::get_if<BundleItemSocketValue>(&this->value);
    if (!socket_value) {
      return std::nullopt;
    }
    if (socket_value->value.is_list()) {
      return socket_value->value.get<ListPtr>();
    }
    return std::nullopt;
  }
  else if (const bke::bNodeSocketType *dst_socket_type = socket_type_info_by_static_type<T>()) {
    return this->as_socket_value<T>(*dst_socket_type);
  }
  /* Can't lookup this type directly currently. */
  BLI_assert_unreachable();
  return std::nullopt;
}

template<typename T> inline std::optional<T> Bundle::lookup(const StringRef key) const
{
  const BundleItemValue *item = this->lookup(key);
  if (!item) {
    return std::nullopt;
  }
  return item->as<T>();
}

template<typename T> inline std::optional<T> Bundle::lookup_path(const StringRef path) const
{
  const BundleItemValue *item = this->lookup_path(path);
  if (!item) {
    return std::nullopt;
  }
  return item->as<T>();
}

template<typename T, typename Fn> inline void to_stored_type(T &&value, Fn &&fn)
{
  using DecayT = std::decay_t<T>;
  if constexpr (std::is_same_v<DecayT, BundleItemValue>) {
    fn(std::forward<T>(value));
  }
  else if constexpr (std::is_same_v<DecayT, BundleItemSocketValue>) {
    fn(BundleItemValue{std::forward<T>(value)});
  }
  else if constexpr (std::is_same_v<DecayT, BundleItemInternalValue>) {
    fn(BundleItemValue{std::forward<T>(value)});
  }
  else if constexpr (is_valid_internal_bundle_item_type<DecayT>()) {
    const BundleItemInternalValueMixin *sharing_info = value.get();
    if (sharing_info) {
      sharing_info->add_user();
    }
    fn(BundleItemValue{BundleItemInternalValue{ImplicitSharingPtr{sharing_info}}});
  }
  else if (const bke::bNodeSocketType *socket_type = socket_type_info_by_static_type<DecayT>()) {
    auto value_variant = bke::SocketValueVariant::From(std::forward<T>(value));
    fn(BundleItemValue{BundleItemSocketValue{socket_type, value_variant}});
  }
  else {
    /* All allowed types should be handled above already. */
    BLI_assert_unreachable();
  }
}

template<typename T> inline void Bundle::add(const StringRef key, T value)
{
  to_stored_type(value, [&](const BundleItemValue &item_value) { this->add(key, item_value); });
}

template<typename T> inline void Bundle::add_path(const StringRef path, T value)
{
  to_stored_type(value,
                 [&](const BundleItemValue &item_value) { this->add_path(path, item_value); });
}

template<typename T> inline void Bundle::add_override(const StringRef key, T value)
{
  to_stored_type(value,
                 [&](const BundleItemValue &item_value) { this->add_override(key, item_value); });
}

template<typename T> inline void Bundle::add_path_override(const StringRef path, T value)
{
  to_stored_type(value, [&](const BundleItemValue &item_value) {
    this->add_path_override(path, item_value);
  });
}

inline Bundle::BundleItemMap::ItemIterator Bundle::items() const
{
  return items_.items();
}

inline bool Bundle::is_empty() const
{
  return items_.is_empty();
}

inline int64_t Bundle::size() const
{
  return items_.size();
}

}  // namespace blender::nodes
