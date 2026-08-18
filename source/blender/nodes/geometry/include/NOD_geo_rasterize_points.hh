/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) with
 * grid items.
 */
struct RasterizePointsItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeGeometryRasterizePointsItem;
  static StructRNA **item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeRasterizePoints";
  static constexpr bool has_type = false;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_rasterize_points_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_rasterize_points_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_rasterize_points_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_rasterize_points_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "rasterize_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<NodeGeometryRasterizePointsItem> get_items_from_node(
      bNode &node)
  {
    auto *storage = static_cast<NodeGeometryRasterizePoints *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }

  static void copy_item(const NodeGeometryRasterizePointsItem &src,
                        NodeGeometryRasterizePointsItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(NodeGeometryRasterizePointsItem *item)
  {
    MEM_SAFE_DELETE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static char **get_name(NodeGeometryRasterizePointsItem &item)
  {
    return &item.name;
  }

  static void init_with_name(bNode &node, NodeGeometryRasterizePointsItem &item, const char *name)
  {
    auto *storage = static_cast<NodeGeometryRasterizePoints *>(node.storage);
    item.type = GEO_NODE_RASTERIZE_POINTS_ITEM_TYPE_SCALAR;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<RasterizePointsItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const NodeGeometryRasterizePointsItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
