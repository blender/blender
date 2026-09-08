/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for combine
 * list items.
 */
struct CombineListItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = CombineListItem;
  static StructRNA **item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeCombineList";
  static constexpr bool has_type = false;
  static constexpr bool has_name = false;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_combine_list_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_combine_list_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_combine_list_item_move";
  };

  static socket_items::SocketItemsRef<CombineListItem> get_items_from_node(bNode &node)
  {
    auto &storage = *static_cast<NodeCombineList *>(node.storage);
    return {&storage.items, &storage.items_num, nullptr};
  }

  static void copy_item(const CombineListItem &src, CombineListItem &dst)
  {
    dst = src;
  }

  static void destruct_item(CombineListItem * /*item*/) {}

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static void init(bNode &node, CombineListItem &item)
  {
    auto &storage = *static_cast<NodeCombineList *>(node.storage);
    item.identifier = storage.next_identifier++;
  }

  static std::string socket_identifier_for_item(const CombineListItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
