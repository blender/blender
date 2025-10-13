/* SPDX-FileCopyrightText: 2025 Blender Authors
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
struct FieldToGridItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = GeometryNodeFieldToGridItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeFieldToGrid";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = false;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_field_to_grid_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_field_to_grid_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_field_to_grid_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "NODE_UL_field_to_grid_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "grid_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<GeometryNodeFieldToGridItem> get_items_from_node(bNode &node)
  {
    auto &storage = *static_cast<GeometryNodeFieldToGrid *>(node.storage);
    return {&storage.items, &storage.items_num, &storage.active_index};
  }

  static void copy_item(const GeometryNodeFieldToGridItem &src, GeometryNodeFieldToGridItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(GeometryNodeFieldToGridItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.data_type);
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int /*ntree_type*/)
  {
    return socket_type_supports_grids(socket_type);
  }

  static char **get_name(GeometryNodeFieldToGridItem &item)
  {
    return &item.name;
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             GeometryNodeFieldToGridItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<GeometryNodeFieldToGrid *>(node.storage);
    item.data_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<FieldToGridItemsAccessor>(node, item, name);
  }

  static std::string input_socket_identifier_for_item(const GeometryNodeFieldToGridItem &item)
  {
    return "Field_" + std::to_string(item.identifier);
  }

  static std::string output_socket_identifier_for_item(const GeometryNodeFieldToGridItem &item)
  {
    return "Grid_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
