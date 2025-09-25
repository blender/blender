/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

namespace blender::nodes {

inline bool socket_type_supported_in_bundle(const eNodeSocketDatatype socket_type,
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

struct CombineBundleItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeCombineBundleItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeCombineBundle";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_name_validation = true;
  static constexpr char unique_name_separator = '_';
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_combine_bundle_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_combine_bundle_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_combine_bundle_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_combine_bundle_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "bundle_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeCombineBundle *>(node.storage);
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
    return socket_type_supported_in_bundle(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeCombineBundle *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<CombineBundleItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }

  static std::string validate_name(const StringRef name);
};

struct SeparateBundleItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeSeparateBundleItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "NodeSeparateBundle";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_name_validation = true;
  static constexpr char unique_name_separator = '_';
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_separate_bundle_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_separate_bundle_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_separate_bundle_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_separate_bundle_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "bundle_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<ItemT> get_items_from_node(bNode &node)
  {
    auto *storage = static_cast<NodeSeparateBundle *>(node.storage);
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
    return socket_type_supported_in_bundle(socket_type, ntree_type);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             ItemT &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeSeparateBundle *>(node.storage);
    item.socket_type = socket_type;
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<SeparateBundleItemsAccessor>(node, item, name);
  }

  static std::string socket_identifier_for_item(const ItemT &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }

  static std::string validate_name(const StringRef name)
  {
    return CombineBundleItemsAccessor::validate_name(name);
  }
};

}  // namespace blender::nodes
