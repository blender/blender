/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for viewer
 * node items.
 */
struct GeoViewerItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeGeometryViewerItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "GeometryNodeViewer";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_geometry_nodes_viewer_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_geometry_nodes_viewer_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_geometry_nodes_viewer_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "NODE_UL_geometry_nodes_viewer_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "viewer_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<NodeGeometryViewerItem> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryViewer *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }

  static void copy_item(const NodeGeometryViewerItem &src, NodeGeometryViewerItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(NodeGeometryViewerItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const NodeGeometryViewerItem &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(NodeGeometryViewerItem &item)
  {
    return &item.name;
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             NodeGeometryViewerItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryViewer *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<GeoViewerItemsAccessor>(node, item, name);
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int /*ntree_type*/)
  {
    return ELEM(socket_type,
                SOCK_FLOAT,
                SOCK_VECTOR,
                SOCK_RGBA,
                SOCK_BOOLEAN,
                SOCK_ROTATION,
                SOCK_MATRIX,
                SOCK_INT,
                SOCK_STRING,
                SOCK_GEOMETRY,
                SOCK_OBJECT,
                SOCK_MATERIAL,
                SOCK_IMAGE,
                SOCK_COLLECTION,
                SOCK_BUNDLE,
                SOCK_CLOSURE);
  }

  static std::string socket_identifier_for_item(const NodeGeometryViewerItem &item)
  {
    /* These special cases exist for compatibility with older Blender versions when the viewer did
     * not have a dynamic number of inputs yet. */
    if (item.identifier == 0 && item.socket_type == SOCK_GEOMETRY) {
      return "Geometry";
    }
    if (item.identifier == 1 && item.socket_type != SOCK_GEOMETRY) {
      return "Value";
    }
    return "Item_" + std::to_string(item.identifier);
  }
};

void geo_viewer_node_log(const bNode &node,
                         const Span<bke::SocketValueVariant *> input_values,
                         geo_eval_log::ViewerNodeLog &r_log);

}  // namespace blender::nodes
