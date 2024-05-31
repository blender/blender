/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for index
 * switch items.
 */
struct IndexSwitchItemsAccessor {
  using ItemT = IndexSwitchItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeIndexSwitch";
  static constexpr bool has_type = false;
  static constexpr bool has_name = false;
  static constexpr bool has_single_identifier_str = true;

  static socket_items::SocketItemsRef<IndexSwitchItem> get_items_from_node(bNode &node)
  {
    auto &storage = *static_cast<NodeIndexSwitch *>(node.storage);
    return {&storage.items, &storage.items_num, nullptr};
  }
  static void copy_item(const IndexSwitchItem &src, IndexSwitchItem &dst)
  {
    dst = src;
  }
  static void destruct_item(IndexSwitchItem * /*item*/) {}
  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);
  static void init(bNode &node, IndexSwitchItem &item)
  {
    auto &storage = *static_cast<NodeIndexSwitch *>(node.storage);
    item.identifier = storage.next_identifier++;
  }
  static std::string socket_identifier_for_item(const IndexSwitchItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
