/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for menu
 * switch node items.
 */

struct MenuSwitchItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeEnumItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeMenuSwitch";
  static constexpr bool has_type = false;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_enum_definition_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_enum_definition_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_enum_definition_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "NODE_UL_enum_definition_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "enum_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<NodeEnumItem> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeMenuSwitch *>(node.storage);
    return {&storage->enum_definition.items_array,
            &storage->enum_definition.items_num,
            &storage->enum_definition.active_index};
  }

  static void copy_item(const NodeEnumItem &src, NodeEnumItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
    dst.description = BLI_strdup_null(dst.description);
  }

  static void destruct_item(NodeEnumItem *item)
  {
    MEM_SAFE_FREE(item->name);
    MEM_SAFE_FREE(item->description);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static char **get_name(NodeEnumItem &item)
  {
    return &item.name;
  }

  static void init_with_name(bNode &node, NodeEnumItem &item, const char *name)
  {
    auto *storage = static_cast<NodeMenuSwitch *>(node.storage);
    item.identifier = storage->enum_definition.next_identifier++;
    socket_items::set_item_name_and_make_unique<MenuSwitchItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const NodeEnumItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
