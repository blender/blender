/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

inline bool socket_type_supported_in_closure(const eNodeSocketDatatype socket_type,
                                             const int ntree_type)
{
  switch (ntree_type) {
    case NTREE_GEOMETRY:
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
    case NTREE_SHADER:
      return ELEM(socket_type,
                  SOCK_FLOAT,
                  SOCK_VECTOR,
                  SOCK_RGBA,
                  SOCK_SHADER,
                  SOCK_BUNDLE,
                  SOCK_CLOSURE,
                  SOCK_INT);
    default:
      return false;
  }
}

struct ClosureInputItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeClosureInputItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeClosureOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_closure_input_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_closure_input_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_closure_input_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_closure_input_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "input_items";
    static constexpr StringRefNull active_index = "active_input_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeClosureOutput *>(node.storage);
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

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int ntree_type)
  {
    return socket_type_supported_in_closure(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeClosureOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->input_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<ClosureInputItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

struct ClosureOutputItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeClosureOutputItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeClosureOutput";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_closure_output_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_closure_output_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_closure_output_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_closure_output_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "output_items";
    static constexpr StringRefNull active_index = "active_output_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeClosureOutput *>(node.storage);
    return {&storage->output_items.items,
            &storage->output_items.items_num,
            &storage->output_items.active_index};
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

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int ntree_type)
  {
    return socket_type_supported_in_closure(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeClosureOutput *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->output_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<ClosureOutputItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

struct EvaluateClosureInputItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeEvaluateClosureInputItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeEvaluateClosure";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_evaluate_closure_input_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_evaluate_closure_input_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_evaluate_closure_input_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_evaluate_closure_input_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "input_items";
    static constexpr StringRefNull active_index = "active_input_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeEvaluateClosure *>(node.storage);
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

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int ntree_type)
  {
    return socket_type_supported_in_closure(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeEvaluateClosure *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->input_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<EvaluateClosureInputItemsAccessor>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

struct EvaluateClosureOutputItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeEvaluateClosureOutputItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeEvaluateClosure";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_evaluate_closure_output_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_evaluate_closure_output_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_evaluate_closure_output_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_evaluate_closure_output_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "output_items";
    static constexpr StringRefNull active_index = "active_output_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeEvaluateClosure *>(node.storage);
    return {&storage->output_items.items,
            &storage->output_items.items_num,
            &storage->output_items.active_index};
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

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const ItemT &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(ItemT &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int ntree_type)
  {
    return socket_type_supported_in_closure(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeEvaluateClosure *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->output_items.next_identifier++;
    socket_items::set_item_name_and_make_unique<EvaluateClosureOutputItemsAccessor>(
        node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

/**
 * Gets an input socket that can be considered to be internally linked to the given output, or
 * null if there is none.
 */
const bNodeSocket *evaluate_closure_node_internally_linked_input(const bNodeSocket &output_socket);

}  // namespace blender::nodes
