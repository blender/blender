/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "BKE_node_socket_value.hh"
#include "BLI_cpp_type.hh"

#include "BKE_node_runtime.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_bundle_signature.hh"

namespace blender::nodes {

bool operator==(const BundleSignature &a, const BundleSignature &b)
{
  return a.items.as_span() == b.items.as_span();
}

bool operator!=(const BundleSignature &a, const BundleSignature &b)
{
  return !(a == b);
}

void BundleSignature::set_auto_structure_types()
{
  for (const BundleSignature::Item &item : this->items) {
    const_cast<BundleSignature::Item &>(item).structure_type =
        NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
  }
}

bool Bundle::is_valid_key(const StringRef key)
{
  if (key.is_empty()) {
    return false;
  }
  if (key != key.trim()) {
    /* Keys must not have leading or trailing white-space. This simplifies potentially using these
     * keys in expressions later on (or even just have a comma separated list of keys). */
    return false;
  }
  return key.find_first_of(Bundle::forbidden_key_chars) == StringRef::not_found;
}

bool Bundle::is_valid_path(const StringRef path)
{
  return split_path(path).has_value();
}

std::optional<Vector<StringRef>> Bundle::split_path(const StringRef path)
{
  if (path.is_empty()) {
    return std::nullopt;
  }
  Vector<StringRef> path_elems;
  StringRef remaining = path;
  while (!remaining.is_empty()) {
    const int sep = remaining.find_first_of('/');
    if (sep == StringRef::not_found) {
      const StringRef key = remaining;
      if (!is_valid_key(key)) {
        return std::nullopt;
      }
      path_elems.append(key);
      break;
    }
    const StringRef key = remaining.substr(0, sep);
    if (!is_valid_key(key)) {
      return std::nullopt;
    }
    path_elems.append(key);
    remaining = remaining.substr(sep + 1);
  }
  return path_elems;
}

BundlePtr Bundle::create()
{
  return BundlePtr(MEM_new<Bundle>(__func__));
}

void Bundle::add_new(const StringRef key, const BundleItemValue &value)
{
  BLI_assert(is_valid_key(key));
  items_.add_new_as(key, value);
}

void Bundle::add_override(const StringRef key, const BundleItemValue &value)
{
  this->remove(key);
  this->add_new(key, value);
}

bool Bundle::add(const StringRef key, const BundleItemValue &value)
{
  if (this->contains(key)) {
    return false;
  }
  this->add_new(key, value);
  return true;
}

static BundleItemValue create_nested_bundle_item()
{
  static const bke::bNodeSocketType *bundle_socket_type = bke::node_socket_type_find_static(
      SOCK_BUNDLE);
  return {
      BundleItemSocketValue{bundle_socket_type, bke::SocketValueVariant::From(Bundle::create())}};
}

void Bundle::add_path_override(const StringRef path, const BundleItemValue &value)
{
  BLI_assert(is_valid_path(path));
  const Vector<StringRef> path_elems = *split_path(path);
  Bundle *current = this;
  for (const StringRef path_elem : path_elems.as_span().drop_back(1)) {
    BundleItemValue &item = current->items_.lookup_or_add_cb_as(
        path_elem, [&]() { return create_nested_bundle_item(); });
    BundlePtr *child_bundle_ptr = item.as_pointer<BundlePtr>();
    if (!child_bundle_ptr) {
      /* Override the items content with a new bundle. */
      item = create_nested_bundle_item();
      child_bundle_ptr = item.as_pointer<BundlePtr>();
    }
    current = &child_bundle_ptr->ensure_mutable_inplace();
  }
  current->items_.add_overwrite_as(path_elems.last(), value);
}

bool Bundle::add_path(StringRef path, const BundleItemValue &value)
{
  if (this->contains_path(path)) {
    return false;
  }
  this->add_path_new(path, value);
  return true;
}

void Bundle::add_path_new(StringRef path, const BundleItemValue &value)
{
  BLI_assert(!this->contains_path(path));
  this->add_path_override(path, value);
}

const BundleItemValue *Bundle::lookup(const StringRef key) const
{
  return items_.lookup_ptr_as(key);
}

const BundleItemValue *Bundle::lookup_path(const Span<StringRef> path) const
{
  BLI_assert(!path.is_empty());
  const StringRef first_elem = path[0];
  const BundleItemValue *item = this->lookup(first_elem);
  if (!item) {
    return nullptr;
  }
  if (path.size() == 1) {
    return item;
  }
  const BundlePtr child_bundle = item->as<BundlePtr>().value_or(nullptr);
  if (!child_bundle) {
    return nullptr;
  }
  return child_bundle->lookup_path(path.drop_front(1));
}

const BundleItemValue *Bundle::lookup_path(const StringRef path) const
{
  BLI_assert(is_valid_path(path));
  const Vector<StringRef> path_elems = *split_path(path);
  return this->lookup_path(path_elems);
}

void Bundle::merge(const Bundle &other)
{
  for (const auto &item : other.items_.items()) {
    this->add(item.key, item.value);
  }
}

void Bundle::merge_override(const Bundle &other)
{
  for (const auto &item : other.items_.items()) {
    this->add_override(item.key, item.value);
  }
}

void Bundle::ensure_owns_direct_data()
{
  for (const auto &item : items_.items()) {
    if (auto *socket_value = std::get_if<BundleItemSocketValue>(&item.value.value)) {
      socket_value->value.ensure_owns_direct_data();
    }
  }
}

bool Bundle::owns_direct_data() const
{
  for (const auto &item : items_.items()) {
    if (const auto *socket_value = std::get_if<BundleItemSocketValue>(&item.value.value)) {
      if (!socket_value->value.owns_direct_data()) {
        return false;
      }
    }
  }
  return true;
}

BundlePtr Bundle::copy() const
{
  BundlePtr copy_ptr = Bundle::create();
  Bundle &copy = const_cast<Bundle &>(*copy_ptr);
  copy.items_ = items_;
  return copy_ptr;
}

bool Bundle::remove(const StringRef key)
{
  BLI_assert(is_valid_key(key));
  return items_.remove_as(key);
}

bool Bundle::remove_path(const StringRef path)
{
  BLI_assert(is_valid_path(path));
  const Vector<StringRef> path_elems = *split_path(path);
  return this->remove_path(path_elems);
}

bool Bundle::remove_path(const Span<StringRef> path)
{
  BLI_assert(this->is_mutable());
  BLI_assert(!path.is_empty());
  if (!this->contains_path(path)) {
    return false;
  }
  Bundle *current = this;
  for (const StringRef path_elem : path.drop_back(1)) {
    BundleItemValue &item = current->items_.lookup_as(path_elem);
    BundlePtr *child_bundle_ptr = item.as_pointer<BundlePtr>();
    current = &child_bundle_ptr->ensure_mutable_inplace();
  }
  current->items_.remove_contained_as(path.last());
  return true;
}

bool Bundle::contains(const StringRef key) const
{
  BLI_assert(is_valid_key(key));
  return items_.contains_as(key);
}

bool Bundle::contains_path(const StringRef path) const
{
  return this->lookup_path(path) != nullptr;
}

bool Bundle::contains_path(const Span<StringRef> path) const
{
  return this->lookup_path(path) != nullptr;
}

std::string Bundle::combine_path(const Span<StringRef> path)
{
  return fmt::format("{}", fmt::join(path, "/"));
}

void Bundle::delete_self()
{
  MEM_delete(this);
}

NodeSocketInterfaceStructureType get_structure_type_for_bundle_signature(
    const bNodeSocket &socket,
    const NodeSocketInterfaceStructureType stored_structure_type,
    const bool allow_auto_structure_type)
{
  if (stored_structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
    return stored_structure_type;
  }
  if (allow_auto_structure_type) {
    return NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
  }
  return NodeSocketInterfaceStructureType(socket.runtime->inferred_structure_type);
}

void BundleSignature::add(std::string key, const eNodeSocketDatatype socket_type)
{
  const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(socket_type);
  BLI_assert(stype);
  items.add({std::move(key), stype});
}

BundleSignature BundleSignature::from_combine_bundle_node(const bNode &node,
                                                          const bool allow_auto_structure_type)
{
  BLI_assert(node.is_type("NodeCombineBundle"));
  const auto &storage = *static_cast<const NodeCombineBundle *>(node.storage);
  BundleSignature signature;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeCombineBundleItem &item = storage.items[i];
    const bNodeSocket &socket = node.input_socket(i);
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      const NodeSocketInterfaceStructureType structure_type =
          get_structure_type_for_bundle_signature(
              socket,
              NodeSocketInterfaceStructureType(item.structure_type),
              allow_auto_structure_type);
      signature.items.add({item.name, stype, structure_type});
    }
  }
  return signature;
}

BundleSignature BundleSignature::from_separate_bundle_node(const bNode &node,
                                                           const bool allow_auto_structure_type)
{
  BLI_assert(node.is_type("NodeSeparateBundle"));
  const auto &storage = *static_cast<const NodeSeparateBundle *>(node.storage);
  BundleSignature signature;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeSeparateBundleItem &item = storage.items[i];
    const bNodeSocket &socket = node.output_socket(i);
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      const NodeSocketInterfaceStructureType structure_type =
          get_structure_type_for_bundle_signature(
              socket,
              NodeSocketInterfaceStructureType(item.structure_type),
              allow_auto_structure_type);
      signature.items.add({item.name, stype, structure_type});
    }
  }
  return signature;
}

bool LinkedBundleSignatures::has_type_definition() const
{
  for (const Item &item : this->items) {
    if (item.is_signature_definition) {
      return true;
    }
  }
  return false;
}

std::optional<BundleSignature> LinkedBundleSignatures::get_merged_signature() const
{
  BundleSignature signature;
  for (const Item &src_signature : this->items) {
    for (const BundleSignature::Item &item : src_signature.signature.items) {
      if (!signature.items.add(item)) {
        const BundleSignature::Item &existing_item = *signature.items.lookup_key_ptr_as(item.key);
        if (item.type->type != existing_item.type->type) {
          return std::nullopt;
        }
        if (existing_item.structure_type != item.structure_type) {
          const_cast<BundleSignature::Item &>(existing_item).structure_type =
              NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
    }
  }
  return signature;
}

}  // namespace blender::nodes
