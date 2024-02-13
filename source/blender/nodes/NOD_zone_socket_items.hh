/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

struct BlendWriter;
struct BlendDataReader;

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) with
 * simulation items.
 */
struct SimulationItemsAccessor {
  using ItemT = NodeSimulationItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeSimulationOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;

  static socket_items::SocketItemsRef<NodeSimulationItem> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometrySimulationOutput *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }
  static void copy_item(const NodeSimulationItem &src, NodeSimulationItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }
  static void destruct_item(NodeSimulationItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }
  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);
  static short *get_socket_type(NodeSimulationItem &item)
  {
    return &item.socket_type;
  }
  static char **get_name(NodeSimulationItem &item)
  {
    return &item.name;
  }
  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    if (socket_type == SOCK_MATRIX) {
      return U.experimental.use_new_matrix_socket;
    }
    return ELEM(socket_type,
                SOCK_FLOAT,
                SOCK_VECTOR,
                SOCK_RGBA,
                SOCK_BOOLEAN,
                SOCK_ROTATION,
                SOCK_MATRIX,
                SOCK_INT,
                SOCK_STRING,
                SOCK_GEOMETRY);
  }
  static void init_with_socket_type_and_name(bNode &node,
                                             NodeSimulationItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometrySimulationOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<SimulationItemsAccessor>(node, item, name);
  }
  static std::string socket_identifier_for_item(const NodeSimulationItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) with
 * repeat items.
 */
struct RepeatItemsAccessor {
  using ItemT = NodeRepeatItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeRepeatOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;

  static socket_items::SocketItemsRef<NodeRepeatItem> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryRepeatOutput *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }
  static void copy_item(const NodeRepeatItem &src, NodeRepeatItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }
  static void destruct_item(NodeRepeatItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }
  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);
  static short *get_socket_type(NodeRepeatItem &item)
  {
    return &item.socket_type;
  }
  static char **get_name(NodeRepeatItem &item)
  {
    return &item.name;
  }
  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    if (socket_type == SOCK_MATRIX) {
      return U.experimental.use_new_matrix_socket;
    }
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
                SOCK_COLLECTION);
  }
  static void init_with_socket_type_and_name(bNode &node,
                                             NodeRepeatItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryRepeatOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<RepeatItemsAccessor>(node, item, name);
  }
  static std::string socket_identifier_for_item(const NodeRepeatItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

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

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) for index
 * bake node items.
 */
struct BakeItemsAccessor {
  using ItemT = NodeGeometryBakeItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeBake";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;

  static socket_items::SocketItemsRef<NodeGeometryBakeItem> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryBake *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
  }
  static void copy_item(const NodeGeometryBakeItem &src, NodeGeometryBakeItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }
  static void destruct_item(NodeGeometryBakeItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }
  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);
  static short *get_socket_type(NodeGeometryBakeItem &item)
  {
    return &item.socket_type;
  }
  static char **get_name(NodeGeometryBakeItem &item)
  {
    return &item.name;
  }
  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return SimulationItemsAccessor::supports_socket_type(socket_type);
  }
  static void init_with_socket_type_and_name(bNode &node,
                                             NodeGeometryBakeItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryBake *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<BakeItemsAccessor>(node, item, name);
  }
  static std::string socket_identifier_for_item(const NodeGeometryBakeItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
