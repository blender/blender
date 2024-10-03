/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

struct ForeachGeometryElementInputItemsAccessor {
  using ItemT = NodeForeachGeometryElementInputItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeForeachGeometryElementOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = true;
  struct operator_idnames {
    static constexpr const char *add_item = "NODE_OT_foreach_geometry_element_zone_input_item_add";
    static constexpr const char *remove_item =
        "NODE_OT_foreach_geometry_element_zone_input_item_remove";
    static constexpr const char *move_item =
        "NODE_OT_foreach_geometry_element_zone_input_item_move";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    return {&storage->input_items.items,
            &storage->input_items.items_num,
            &storage->input_items.active_index};
  }

  static void copy_item(const ItemT &src, ItemT &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(ItemT *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return ELEM(socket_type,
                SOCK_FLOAT,
                SOCK_VECTOR,
                SOCK_RGBA,
                SOCK_BOOLEAN,
                SOCK_ROTATION,
                SOCK_MATRIX,
                SOCK_INT,
                SOCK_MENU);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->generation_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<ForeachGeometryElementInputItemsAccessor>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Input_" + std::to_string(item.identifier);
  }
};

struct ForeachGeometryElementMainItemsAccessor {
  using ItemT = NodeForeachGeometryElementMainItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeForeachGeometryElementOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = true;
  struct operator_idnames {
    static constexpr const char *add_item = "NODE_OT_foreach_geometry_element_zone_main_item_add";
    static constexpr const char *remove_item =
        "NODE_OT_foreach_geometry_element_zone_main_item_remove";
    static constexpr const char *move_item =
        "NODE_OT_foreach_geometry_element_zone_main_item_move";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    return {&storage->main_items.items,
            &storage->main_items.items_num,
            &storage->main_items.active_index};
  }

  static void copy_item(const ItemT &src, ItemT &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(ItemT *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return ELEM(socket_type,
                SOCK_FLOAT,
                SOCK_VECTOR,
                SOCK_RGBA,
                SOCK_BOOLEAN,
                SOCK_ROTATION,
                SOCK_MATRIX,
                SOCK_INT);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->generation_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<ForeachGeometryElementMainItemsAccessor>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Main_" + std::to_string(item.identifier);
  }
};

struct ForeachGeometryElementGenerationItemsAccessor {
  using ItemT = NodeForeachGeometryElementGenerationItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeForeachGeometryElementOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = true;
  struct operator_idnames {
    static constexpr const char *add_item =
        "NODE_OT_foreach_geometry_element_zone_generation_item_add";
    static constexpr const char *remove_item =
        "NODE_OT_foreach_geometry_element_zone_generation_item_remove";
    static constexpr const char *move_item =
        "NODE_OT_foreach_geometry_element_zone_generation_item_move";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    return {&storage->generation_items.items,
            &storage->generation_items.items_num,
            &storage->generation_items.active_index};
  }

  static void copy_item(const ItemT &src, ItemT &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(ItemT *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return ELEM(socket_type,
                SOCK_FLOAT,
                SOCK_VECTOR,
                SOCK_RGBA,
                SOCK_BOOLEAN,
                SOCK_ROTATION,
                SOCK_MATRIX,
                SOCK_INT,
                SOCK_GEOMETRY);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->generation_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<ForeachGeometryElementGenerationItemsAccessor>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Generation_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
