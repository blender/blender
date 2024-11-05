/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`).
 */
template<typename NodeOutputT, typename NodeItemT> struct ShZoneItemsAccessorBase {
  using ItemT = NodeItemT;
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = true;

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeOutputT *>(node.storage);
    return {&storage->items, &storage->items_num, &storage->active_index};
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
    return ELEM(socket_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeShaderRepeatOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<ShZoneItemsAccessorBase<NodeOutputT, ItemT>>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) with
 * repeat items.
 */
struct ShRepeatItemsAccessor
    : ShZoneItemsAccessorBase<NodeShaderRepeatOutput, NodeShaderRepeatItem> {
  static constexpr const char *node_idname = "ShaderNodeRepeatOutput";
  struct operator_idnames {
    static constexpr const char *add_item = "NODE_OT_sh_repeat_zone_item_add";
    static constexpr const char *remove_item = "NODE_OT_sh_repeat_zone_item_remove";
    static constexpr const char *move_item = "NODE_OT_sh_repeat_zone_item_move";
  };
  struct ui_idnames {
    static constexpr const char *list = "DATA_UL_sh_repeat_zone_state";
  };
  struct rna_names {
    static constexpr const char *items = "repeat_items";
    static constexpr const char *active_index = "active_index";
  };
  /* Defined in node_shader_repeat.cc */
  static StructRNA *item_srna;
  static int node_type;
  static int item_dna_type;
  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);
};

/**
 * Makes it possible to use various functions (e.g. the ones in `NOD_socket_items.hh`) with
 * light loop items.
 */
struct ShLightLoopItemsAccessor
    : ShZoneItemsAccessorBase<NodeShaderLightLoopOutput, NodeShaderLightLoopItem> {
  static constexpr const char *node_idname = "ShaderNodeLightLoopOutput";
  struct operator_idnames {
    static constexpr const char *add_item = "NODE_OT_sh_light_loop_zone_item_add";
    static constexpr const char *remove_item = "NODE_OT_sh_light_loop_zone_item_remove";
    static constexpr const char *move_item = "NODE_OT_sh_light_loop_zone_item_move";
  };
  struct ui_idnames {
    static constexpr const char *list = "DATA_UL_sh_light_loop_zone_state";
  };
  struct rna_names {
    static constexpr const char *items = "light_loop_items";
    static constexpr const char *active_index = "active_index";
  };
  /* Defined in node_shader_light_loop.cc */
  static StructRNA *item_srna;
  static int node_type;
  static int item_dna_type;
  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);
};

}  // namespace blender::nodes
