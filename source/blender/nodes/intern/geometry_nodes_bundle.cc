/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

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

BundlePtr Bundle::create()
{
  return BundlePtr(MEM_new<Bundle>(__func__));
}

[[maybe_unused]] static bool is_valid_key(const StringRef key)
{
  return key.find('/') == StringRef::not_found;
}

void Bundle::add_new(const StringRef key, const BundleItemValue &value)
{
  BLI_assert(is_valid_key(key));
  items_.append(StoredItem{std::move(key), value});
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

void Bundle::add_path_override(const StringRef path, const BundleItemValue &value)
{
  BLI_assert(!path.is_empty());
  BLI_assert(!path.endswith("/"));
  BLI_assert(this->is_mutable());
  const int sep = path.find_first_of('/');
  if (sep == StringRef::not_found) {
    const StringRef key = path;
    this->remove(key);
    this->add_new(key, value);
    return;
  }
  const StringRef first_part = path.substr(0, sep);
  BundlePtr child_bundle = this->lookup<BundlePtr>(first_part).value_or(nullptr);
  if (!child_bundle) {
    child_bundle = Bundle::create();
  }
  this->remove(first_part);
  if (!child_bundle->is_mutable()) {
    child_bundle = child_bundle->copy();
  }
  child_bundle->tag_ensured_mutable();
  const_cast<Bundle &>(*child_bundle).add_path_override(path.substr(sep + 1), value);
  bke::SocketValueVariant child_bundle_value = bke::SocketValueVariant::From(
      std::move(child_bundle));
  this->add(
      first_part,
      BundleItemSocketValue{bke::node_socket_type_find_static(SOCK_BUNDLE), child_bundle_value});
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
  for (const StoredItem &item : items_) {
    if (item.key == key) {
      return &item.value;
    }
  }
  return nullptr;
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

static Vector<StringRef> split_path(const StringRef path)
{
  Vector<StringRef> path_elems;
  StringRef remaining = path;
  while (!remaining.is_empty()) {
    const int sep = remaining.find_first_of('/');
    if (sep == StringRef::not_found) {
      path_elems.append(remaining);
      break;
    }
    path_elems.append(remaining.substr(0, sep));
    remaining = remaining.substr(sep + 1);
  }
  return path_elems;
}

const BundleItemValue *Bundle::lookup_path(const StringRef path) const
{
  const Vector<StringRef> path_elems = split_path(path);
  return this->lookup_path(path_elems);
}

BundlePtr Bundle::copy() const
{
  BundlePtr copy_ptr = Bundle::create();
  Bundle &copy = const_cast<Bundle &>(*copy_ptr);
  for (const StoredItem &item : items_) {
    copy.add_new(item.key, item.value);
  }
  return copy_ptr;
}

bool Bundle::remove(const StringRef key)
{
  BLI_assert(is_valid_key(key));
  const int removed_num = items_.remove_if([&key](StoredItem &item) { return item.key == key; });
  return removed_num >= 1;
}

bool Bundle::contains(const StringRef key) const
{
  BLI_assert(is_valid_key(key));
  for (const StoredItem &item : items_) {
    if (item.key == key) {
      return true;
    }
  }
  return false;
}

bool Bundle::contains_path(const StringRef path) const
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
