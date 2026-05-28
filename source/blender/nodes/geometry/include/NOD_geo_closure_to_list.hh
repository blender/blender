/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for field
 * to grid items.
 */
struct ClosureToListItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = GeometryNodeClosureToListItem;
  static StructRNA **item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeClosureToList";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = false;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_closure_to_list_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_closure_to_list_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_closure_to_list_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "NODE_UL_closure_to_list_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "list_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<GeometryNodeClosureToListItem> get_items_from_node(
      bNode &node)
  {
    auto &storage = *static_cast<GeometryNodeClosureToList *>(node.storage);
    return {&storage.items, &storage.items_num, &storage.active_index};
  }

  static void copy_item(const GeometryNodeClosureToListItem &src,
                        GeometryNodeClosureToListItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(GeometryNodeClosureToListItem *item)
  {
    MEM_SAFE_DELETE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int ntree_type)
  {
    return bke::node_tree_type_supports_socket_type_static(ntree_type, socket_type);
  }

  static char **get_name(GeometryNodeClosureToListItem &item)
  {
    return &item.name;
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             GeometryNodeClosureToListItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<GeometryNodeClosureToList *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    item.structure_type = NodeSocketInterfaceStructureType::Single;
    socket_items::set_item_name_and_make_unique<ClosureToListItemsAccessor>(node, item, name);
  }

  static std::string input_socket_identifier_for_item(
      const GeometryNodeClosureToListItem & /*item*/)
  {
    return {};
  }
  static std::string output_socket_identifier_for_item(const GeometryNodeClosureToListItem &item)
  {
    return "Grid_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
