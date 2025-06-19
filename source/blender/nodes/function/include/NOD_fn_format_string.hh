/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for format
 * string items.
 */
struct FormatStringItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeFunctionFormatStringItem;
  static StructRNA *item_srna;
  static constexpr StringRefNull node_idname = "FunctionNodeFormatString";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_name_validation = true;
  static constexpr bool has_custom_initial_name = true;
  static constexpr char unique_name_separator = '_';
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_format_string_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_format_string_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_format_string_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_format_string_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "format_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<NodeFunctionFormatStringItem> get_items_from_node(
      bNode &node)
  {
    auto *storage = static_cast<NodeFunctionFormatString *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }

  static void copy_item(const NodeFunctionFormatStringItem &src, NodeFunctionFormatStringItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(NodeFunctionFormatStringItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const NodeFunctionFormatStringItem &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(NodeFunctionFormatStringItem &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return ELEM(socket_type, SOCK_INT, SOCK_FLOAT, SOCK_STRING);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             NodeFunctionFormatStringItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeFunctionFormatString *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<FormatStringItemsAccessor>(node, item, name);
  }

  static std::string custom_initial_name(const bNode &node, StringRef src_name);
  static std::string validate_name(const StringRef name);

  static std::string socket_identifier_for_item(const NodeFunctionFormatStringItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
