/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_socket_value.hh"
#include "BLI_cpp_type.hh"

#include "BKE_node_runtime.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_bundle_signature.hh"

namespace blender::nodes {

SocketInterfaceKey::SocketInterfaceKey(std::string identifier)
{
  identifiers_.append(std::move(identifier));
}

SocketInterfaceKey::SocketInterfaceKey(Vector<std::string> identifiers)
    : identifiers_(std::move(identifiers))
{
  BLI_assert(!identifiers_.is_empty());
}

Span<std::string> SocketInterfaceKey::identifiers() const
{
  return identifiers_;
}

bool SocketInterfaceKey::matches(const SocketInterfaceKey &other) const
{
  for (const std::string &identifier : other.identifiers_) {
    if (identifiers_.contains(identifier)) {
      return true;
    }
  }
  return false;
}

bool SocketInterfaceKey::matches_exactly(const SocketInterfaceKey &other) const
{
  for (const std::string &identifier : identifiers_) {
    if (std::none_of(
            other.identifiers_.begin(),
            other.identifiers_.end(),
            [&](const std::string &other_identifier) { return other_identifier == identifier; }))
    {
      return false;
    }
  }
  return true;
}

bool BundleSignature::matches_exactly(const BundleSignature &other) const
{
  if (items.size() != other.items.size()) {
    return false;
  }
  for (const Item &item : items) {
    if (std::none_of(other.items.begin(), other.items.end(), [&](const Item &other_item) {
          return item.key.matches_exactly(other_item.key);
        }))
    {
      return false;
    }
  }
  return true;
}

bool BundleSignature::all_matching_exactly(const Span<BundleSignature> signatures)
{
  if (signatures.is_empty()) {
    return true;
  }
  for (const BundleSignature &signature : signatures.drop_front(1)) {
    if (!signatures[0].matches_exactly(signature)) {
      return false;
    }
  }
  return true;
}

Bundle::Bundle() = default;

Bundle::~Bundle()
{
  for (StoredItem &item : items_) {
    item.type->geometry_nodes_cpp_type->destruct(item.value);
  }
  for (void *buffer : buffers_) {
    MEM_freeN(buffer);
  }
}

Bundle::Bundle(const Bundle &other)
{
  for (const StoredItem &item : other.items_) {
    this->add_new(item.key, *item.type, item.value);
  }
}

Bundle::Bundle(Bundle &&other) noexcept
    : items_(std::move(other.items_)), buffers_(std::move(other.buffers_))
{
}

BundlePtr Bundle::create()
{
  return BundlePtr(MEM_new<Bundle>(__func__));
}

Bundle &Bundle::operator=(const Bundle &other)
{
  if (this == &other) {
    return *this;
  }
  this->~Bundle();
  new (this) Bundle(other);
  return *this;
}

Bundle &Bundle::operator=(Bundle &&other) noexcept
{
  if (this == &other) {
    return *this;
  }
  this->~Bundle();
  new (this) Bundle(std::move(other));
  return *this;
}

void Bundle::add_new(SocketInterfaceKey key, const bke::bNodeSocketType &type, const void *value)
{
  BLI_assert(!this->contains(key));
  BLI_assert(type.geometry_nodes_cpp_type);
  const CPPType &cpp_type = *type.geometry_nodes_cpp_type;
  void *buffer = MEM_mallocN_aligned(cpp_type.size, cpp_type.alignment, __func__);
  cpp_type.copy_construct(value, buffer);
  items_.append(StoredItem{std::move(key), &type, buffer});
  buffers_.append(buffer);
}

void Bundle::add_override(const SocketInterfaceKey &key,
                          const bke::bNodeSocketType &type,
                          const void *value)
{
  this->remove(key);
  this->add_new(key, type, value);
}

bool Bundle::add(const SocketInterfaceKey &key,
                 const bke::bNodeSocketType &type,
                 const void *value)
{
  if (this->contains(key)) {
    return false;
  }
  this->add_new(key, type, value);
  return true;
}

void Bundle::add_path_override(const StringRef path,
                               const bke::bNodeSocketType &type,
                               const void *value)
{
  BLI_assert(!path.is_empty());
  BLI_assert(!path.endswith("/"));
  BLI_assert(this->is_mutable());
  const int sep = path.find_first_of('/');
  if (sep == StringRef::not_found) {
    this->remove(SocketInterfaceKey{path});
    this->add_new(SocketInterfaceKey{path}, type, value);
    return;
  }
  const StringRef first_part = path.substr(0, sep);
  BundlePtr child_bundle;
  const std::optional<Bundle::Item> item = this->lookup(SocketInterfaceKey{first_part});
  if (item && item->type->type == SOCK_BUNDLE) {
    child_bundle = static_cast<const bke::SocketValueVariant *>(item->value)->get<BundlePtr>();
  }
  if (!child_bundle) {
    child_bundle = Bundle::create();
  }
  this->remove(SocketInterfaceKey{first_part});
  if (!child_bundle->is_mutable()) {
    child_bundle = child_bundle->copy();
  }
  child_bundle->tag_ensured_mutable();
  const_cast<Bundle &>(*child_bundle).add_path_override(path.substr(sep + 1), type, value);
  bke::SocketValueVariant child_bundle_value = bke::SocketValueVariant::From(
      std::move(child_bundle));
  this->add(SocketInterfaceKey{first_part},
            *bke::node_socket_type_find_static(SOCK_BUNDLE),
            &child_bundle_value);
}

bool Bundle::add_path(StringRef path, const bke::bNodeSocketType &type, const void *value)
{
  if (this->contains_path(path)) {
    return false;
  }
  this->add_path_new(path, type, value);
  return true;
}

void Bundle::add_path_new(StringRef path, const bke::bNodeSocketType &type, const void *value)
{
  BLI_assert(!this->contains_path(path));
  this->add_path_override(path, type, value);
}

std::optional<Bundle::Item> Bundle::lookup(const SocketInterfaceKey &key) const
{
  for (const StoredItem &item : items_) {
    if (item.key.matches(key)) {
      return Item{item.type, item.value};
    }
  }
  return std::nullopt;
}

std::optional<Bundle::Item> Bundle::lookup_path(const Span<StringRef> path) const
{
  BLI_assert(!path.is_empty());
  const StringRef first_elem = path[0];
  const std::optional<Bundle::Item> item = this->lookup(SocketInterfaceKey(first_elem));
  if (!item) {
    return std::nullopt;
  }
  if (path.size() == 1) {
    return item;
  }
  if (item->type->type != SOCK_BUNDLE) {
    return std::nullopt;
  }
  const BundlePtr child_bundle =
      static_cast<const bke::SocketValueVariant *>(item->value)->get<BundlePtr>();
  if (!child_bundle) {
    return std::nullopt;
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

std::optional<Bundle::Item> Bundle::lookup_path(const StringRef path) const
{
  const Vector<StringRef> path_elems = split_path(path);
  return this->lookup_path(path_elems);
}

BundlePtr Bundle::copy() const
{
  BundlePtr copy_ptr = Bundle::create();
  Bundle &copy = const_cast<Bundle &>(*copy_ptr);
  for (const StoredItem &item : items_) {
    copy.add_new(item.key, *item.type, item.value);
  }
  return copy_ptr;
}

bool Bundle::remove(const SocketInterfaceKey &key)
{
  const int removed_num = items_.remove_if([&key](StoredItem &item) {
    if (item.key.matches(key)) {
      item.type->geometry_nodes_cpp_type->destruct(item.value);
      return true;
    }
    return false;
  });
  return removed_num >= 1;
}

bool Bundle::contains(const SocketInterfaceKey &key) const
{
  for (const StoredItem &item : items_) {
    if (item.key.matches(key)) {
      return true;
    }
  }
  return false;
}

bool Bundle::contains_path(const StringRef path) const
{
  return this->lookup_path(path).has_value();
}

void Bundle::delete_self()
{
  MEM_delete(this);
}

BundleSignature BundleSignature::from_combine_bundle_node(const bNode &node)
{
  BLI_assert(node.is_type("GeometryNodeCombineBundle"));
  const auto &storage = *static_cast<const NodeGeometryCombineBundle *>(node.storage);
  BundleSignature signature;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryCombineBundleItem &item = storage.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.items.append({nodes::SocketInterfaceKey(item.name), stype});
    }
  }
  return signature;
}

BundleSignature BundleSignature::from_separate_bundle_node(const bNode &node)
{
  BLI_assert(node.is_type("GeometryNodeSeparateBundle"));
  const auto &storage = *static_cast<const NodeGeometrySeparateBundle *>(node.storage);
  BundleSignature signature;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometrySeparateBundleItem &item = storage.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.items.append({nodes::SocketInterfaceKey(item.name), stype});
    }
  }
  return signature;
}

}  // namespace blender::nodes
