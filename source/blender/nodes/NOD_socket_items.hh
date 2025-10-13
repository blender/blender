/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Some nodes have a dynamic number of sockets (e.g. simulation input/output). These nodes store an
 * array of items in their `bNode->storage` (e.g. `NodeSimulationItem`). Different nodes have
 * slightly different storage requirements, but a lot of the logic is still the same between nodes.
 * This file implements various shared functionality that can be used by different nodes to deal
 * with these item arrays.
 *
 * In order to use the functions, one has to implement an "accessor" which tells the shared code
 * how to deal with specific item arrays. Different functions have different requirements for the
 * accessor. It's easiest to just look at existing accessors like #SimulationItemsAccessor and
 * #RepeatItemsAccessor and to implement the same methods.
 */

#include <optional>

#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "DNA_array_utils.hh"

#include "NOD_socket.hh"

namespace blender::nodes::socket_items {

struct SocketItemsAccessorDefaults {
  static constexpr bool has_single_identifier_str = true;
  static constexpr bool has_name_validation = false;
  static constexpr bool has_custom_initial_name = false;
  static constexpr bool has_vector_dimensions = false;
  static constexpr bool can_have_empty_name = false;
  static constexpr char unique_name_separator = '.';
};

/**
 * References a "C-Array" that is stored elsewhere. This is different from a MutableSpan, because
 * one can even resize the array through this reference.
 */
template<typename T> struct SocketItemsRef {
  T **items;
  int *items_num;
  int *active_index;
};

/**
 * Iterates over the node tree to find the node that this item belongs to.
 */
template<typename Accessor>
inline bNode *find_node_by_item(bNodeTree &ntree, const typename Accessor::ItemT &item)
{
  ntree.ensure_topology_cache();
  for (bNode *node : ntree.nodes_by_type(Accessor::node_idname)) {
    SocketItemsRef array = Accessor::get_items_from_node(*node);
    if (&item >= *array.items && &item < *array.items + *array.items_num) {
      return node;
    }
  }
  return nullptr;
}

/** Find the item with the given identifier. */
template<typename Accessor>
inline typename Accessor::ItemT *find_item_by_identifier(bNode &node, const StringRef identifier)
{

  SocketItemsRef array = Accessor::get_items_from_node(node);
  for (const int i : IndexRange(*array.items_num)) {
    typename Accessor::ItemT &item = (*array.items)[i];
    if (Accessor::socket_identifier_for_item(item) == identifier) {
      return &item;
    }
  }
  return nullptr;
}

/**
 * Destruct all the items and the free the array itself.
 */
template<typename Accessor> inline void destruct_array(bNode &node)
{
  using ItemT = typename Accessor::ItemT;
  SocketItemsRef ref = Accessor::get_items_from_node(node);
  for (const int i : IndexRange(*ref.items_num)) {
    ItemT &item = (*ref.items)[i];
    Accessor::destruct_item(&item);
  }
  MEM_SAFE_FREE(*ref.items);
}

/**
 * Removes all items from the node.
 */
template<typename Accessor> inline void clear(bNode &node)
{
  destruct_array<Accessor>(node);
  SocketItemsRef ref = Accessor::get_items_from_node(node);
  *ref.items_num = 0;
  *ref.active_index = 0;
}

/**
 * Copy the items from the storage of the source node to the storage of the destination node.
 */
template<typename Accessor> inline void copy_array(const bNode &src_node, bNode &dst_node)
{
  using ItemT = typename Accessor::ItemT;
  SocketItemsRef src_ref = Accessor::get_items_from_node(const_cast<bNode &>(src_node));
  SocketItemsRef dst_ref = Accessor::get_items_from_node(dst_node);
  const int items_num = *src_ref.items_num;
  *dst_ref.items = MEM_calloc_arrayN<ItemT>(items_num, __func__);
  for (const int i : IndexRange(items_num)) {
    Accessor::copy_item((*src_ref.items)[i], (*dst_ref.items)[i]);
  }
}

/**
 * Enforce constraints on the name of the item.
 */
template<typename Accessor> inline std::string get_validated_name(const StringRef name)
{
  if constexpr (Accessor::has_name_validation) {
    return Accessor::validate_name(name);
  }
  else {
    return name;
  }
}

/**
 * Changes the name of an existing item and makes sure that the name is unique among other the
 * other items in the same array.
 */
template<typename Accessor>
inline void set_item_name_and_make_unique(bNode &node,
                                          typename Accessor::ItemT &item,
                                          const char *value)
{
  using ItemT = typename Accessor::ItemT;
  SocketItemsRef array = Accessor::get_items_from_node(node);

  std::string name = value;
  if constexpr (!Accessor::can_have_empty_name) {
    if (name.empty()) {
      if constexpr (Accessor::has_type) {
        name = *bke::node_static_socket_label(Accessor::get_socket_type(item), 0);
      }
      else {
        name = "Item";
      }
    }
  }

  const std::string validated_name = get_validated_name<Accessor>(name);

  const std::string unique_name = BLI_uniquename_cb(
      [&](const StringRef name) {
        for (ItemT &item_iter : blender::MutableSpan(*array.items, *array.items_num)) {
          if (&item_iter != &item) {
            if (*Accessor::get_name(item_iter) == name) {
              return true;
            }
          }
        }
        return false;
      },
      Accessor::unique_name_separator,
      validated_name);

  /* The unique name should still be valid. */
  BLI_assert(unique_name == get_validated_name<Accessor>(unique_name));

  char **item_name = Accessor::get_name(item);
  MEM_SAFE_FREE(*item_name);
  *item_name = BLI_strdup(unique_name.c_str());
}

namespace detail {

template<typename Accessor> inline typename Accessor::ItemT &add_item_to_array(bNode &node)
{
  using ItemT = typename Accessor::ItemT;
  SocketItemsRef array = Accessor::get_items_from_node(node);

  ItemT *old_items = *array.items;
  const int old_items_num = *array.items_num;
  const int new_items_num = old_items_num + 1;

  ItemT *new_items = MEM_calloc_arrayN<ItemT>(new_items_num, __func__);
  std::copy_n(old_items, old_items_num, new_items);
  ItemT &new_item = new_items[old_items_num];

  MEM_SAFE_FREE(old_items);
  *array.items = new_items;
  *array.items_num = new_items_num;
  if (array.active_index) {
    *array.active_index = old_items_num;
  }

  return new_item;
}

}  // namespace detail

/**
 * Add a new item at the end with the given socket type and name. The optional dimensions argument
 * can be provided for types that support multiple possible dimensions like Vector. It is expected
 * to be in the range [2, 4] and if not provided, 3 should be assumed.
 */
template<typename Accessor>
inline typename Accessor::ItemT *add_item_with_socket_type_and_name(
    bNodeTree &ntree,
    bNode &node,
    const eNodeSocketDatatype socket_type,
    const char *name,
    std::optional<int> dimensions = std::nullopt)
{
  using ItemT = typename Accessor::ItemT;
  BLI_assert(Accessor::supports_socket_type(socket_type, ntree.type));
  BLI_assert(!(dimensions.has_value() && socket_type != SOCK_VECTOR));
  BLI_assert(ELEM(dimensions.value_or(3), 2, 3, 4));
  UNUSED_VARS_NDEBUG(ntree);
  ItemT &new_item = detail::add_item_to_array<Accessor>(node);
  if constexpr (Accessor::has_vector_dimensions) {
    Accessor::init_with_socket_type_and_name(node, new_item, socket_type, name, dimensions);
  }
  else {
    Accessor::init_with_socket_type_and_name(node, new_item, socket_type, name);
  }
  return &new_item;
}

/**
 * Add a new item at the end with the given name.
 */
template<typename Accessor>
inline typename Accessor::ItemT *add_item_with_name(bNode &node, const char *name)
{
  using ItemT = typename Accessor::ItemT;
  ItemT &new_item = detail::add_item_to_array<Accessor>(node);
  Accessor::init_with_name(node, new_item, name);
  return &new_item;
}

/**
 * Add a new item at the end.
 */
template<typename Accessor> inline typename Accessor::ItemT *add_item(bNode &node)
{
  using ItemT = typename Accessor::ItemT;
  ItemT &new_item = detail::add_item_to_array<Accessor>(node);
  Accessor::init(node, new_item);
  return &new_item;
}

template<typename Accessor>
inline std::string get_socket_identifier(const typename Accessor::ItemT &item,
                                         const eNodeSocketInOut in_out)
{
  if constexpr (Accessor::has_single_identifier_str) {
    return Accessor::socket_identifier_for_item(item);
  }
  else {
    if (in_out == SOCK_IN) {
      return Accessor::input_socket_identifier_for_item(item);
    }
    return Accessor::output_socket_identifier_for_item(item);
  }
}

inline std::optional<eNodeSocketDatatype> get_socket_item_type_to_add(
    const eNodeSocketDatatype linked_type,
    const FunctionRef<bool(eNodeSocketDatatype type)> is_supported)
{
  if (is_supported(linked_type)) {
    return linked_type;
  }
  if (linked_type == SOCK_RGBA) {
    if (is_supported(SOCK_VECTOR)) {
      return SOCK_VECTOR;
    }
  }
  return std::nullopt;
}

/**
 * Check if the link connects to the `extend_socket`. If yes, create a new item for the linked
 * socket, update the node and then change the link to point to the new socket.
 * \return False if the link should be removed.
 */
template<typename Accessor>
[[nodiscard]] inline bool try_add_item_via_extend_socket(
    bNodeTree &ntree,
    bNode &extend_node,
    bNodeSocket &extend_socket,
    bNode &storage_node,
    bNodeLink &link,
    typename Accessor::ItemT **r_new_item = nullptr)
{
  using ItemT = typename Accessor::ItemT;
  bNodeSocket *src_socket = nullptr;
  if (link.tosock == &extend_socket) {
    src_socket = link.fromsock;
  }
  else if (link.fromsock == &extend_socket) {
    src_socket = link.tosock;
  }
  else {
    return false;
  }

  ItemT *item = nullptr;
  if constexpr (Accessor::has_name && Accessor::has_type) {
    const eNodeSocketDatatype src_socket_type = eNodeSocketDatatype(src_socket->type);
    const std::optional<eNodeSocketDatatype> added_socket_type = get_socket_item_type_to_add(
        src_socket_type, [&](const eNodeSocketDatatype type) {
          return Accessor::supports_socket_type(type, ntree.type);
        });
    if (!added_socket_type) {
      return false;
    }
    std::string name = src_socket->name;
    if constexpr (Accessor::has_custom_initial_name) {
      name = Accessor::custom_initial_name(storage_node, name);
    }
    std::optional<int> dimensions = std::nullopt;
    if (src_socket_type == SOCK_VECTOR && added_socket_type == SOCK_VECTOR) {
      dimensions = src_socket->default_value_typed<bNodeSocketValueVector>()->dimensions;
    }
    item = add_item_with_socket_type_and_name<Accessor>(
        ntree, storage_node, *added_socket_type, name.c_str(), dimensions);
  }
  else if constexpr (Accessor::has_name && !Accessor::has_type) {
    item = add_item_with_name<Accessor>(storage_node, src_socket->name);
  }
  else {
    item = add_item<Accessor>(storage_node);
  }
  if (item == nullptr) {
    return false;
  }
  if (r_new_item) {
    *r_new_item = item;
  }

  update_node_declaration_and_sockets(ntree, extend_node);
  if (extend_socket.is_input()) {
    const std::string item_identifier = get_socket_identifier<Accessor>(*item, SOCK_IN);
    bNodeSocket *new_socket = bke::node_find_socket(extend_node, SOCK_IN, item_identifier.c_str());
    link.tosock = new_socket;
  }
  else {
    const std::string item_identifier = get_socket_identifier<Accessor>(*item, SOCK_OUT);
    bNodeSocket *new_socket = bke::node_find_socket(
        extend_node, SOCK_OUT, item_identifier.c_str());
    link.fromsock = new_socket;
  }
  BKE_ntree_update_tag_node_property(&ntree, &storage_node);
  return true;
}

/**
 * Allow the item array to be extended from any extend-socket in the node.
 * \return False if the link should be removed.
 */
template<typename Accessor>
[[nodiscard]] inline bool try_add_item_via_any_extend_socket(
    bNodeTree &ntree,
    bNode &extend_node,
    bNode &storage_node,
    bNodeLink &link,
    const std::optional<StringRef> socket_identifier = std::nullopt,
    typename Accessor::ItemT **r_new_item = nullptr)
{
  bNodeSocket *possible_extend_socket = nullptr;
  if (link.fromnode == &extend_node) {
    possible_extend_socket = link.fromsock;
  }
  if (link.tonode == &extend_node) {
    possible_extend_socket = link.tosock;
  }
  if (possible_extend_socket == nullptr) {
    return true;
  }
  if (!STREQ(possible_extend_socket->idname, "NodeSocketVirtual")) {
    return true;
  }
  if (socket_identifier.has_value()) {
    if (possible_extend_socket->identifier != socket_identifier) {
      return true;
    }
  }
  return try_add_item_via_extend_socket<Accessor>(
      ntree, extend_node, *possible_extend_socket, storage_node, link, r_new_item);
}

}  // namespace blender::nodes::socket_items
