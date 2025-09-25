/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_math_base.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "BKE_image_format.hh"

#include "NOD_socket_items.hh"

namespace blender::nodes {

struct FileOutputItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeCompositorFileOutputItem;
  static StructRNA *item_srna;
  static constexpr StringRefNull node_idname = "CompositorNodeOutputFile";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_name_validation = true;
  static constexpr bool has_vector_dimensions = true;
  static constexpr bool can_have_empty_name = true;
  static constexpr char unique_name_separator = '_';
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_file_output_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_file_output_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_file_output_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "DATA_UL_file_output_items";
  };
  struct rna_names {
    static constexpr StringRefNull items = "file_output_items";
    static constexpr StringRefNull active_index = "active_item_index";
  };

  static socket_items::SocketItemsRef<NodeCompositorFileOutputItem> get_items_from_node(
      bNode &node)
  {
    auto *storage = static_cast<NodeCompositorFileOutput *>(node.storage);
    return {&storage->items, &storage->items_count, &storage->active_item_index};
  }

  static void copy_item(const NodeCompositorFileOutputItem &source,
                        NodeCompositorFileOutputItem &destination)
  {
    destination = source;
    destination.name = BLI_strdup_null(destination.name);
    BKE_image_format_copy(&destination.format, &source.format);
  }

  static void destruct_item(NodeCompositorFileOutputItem *item)
  {
    MEM_SAFE_FREE(item->name);
    BKE_image_format_free(&item->format);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const NodeCompositorFileOutputItem &item)
  {
    return eNodeSocketDatatype(item.socket_type);
  }

  static char **get_name(NodeCompositorFileOutputItem &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int /*ntree_type*/)
  {
    return ELEM(socket_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
  }

  static int find_available_identifier(const NodeCompositorFileOutputItem *items,
                                       const int items_count)
  {
    if (items_count == 0) {
      return 0;
    }
    int max_identifier = items[0].identifier;
    for (int i = 0; i < items_count; i++) {
      max_identifier = math::max(items[i].identifier, max_identifier);
    }
    return max_identifier + 1;
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             NodeCompositorFileOutputItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name,
                                             std::optional<int> dimensions = std::nullopt)
  {
    auto *storage = static_cast<NodeCompositorFileOutput *>(node.storage);
    item.identifier = FileOutputItemsAccessor::find_available_identifier(storage->items,
                                                                         storage->items_count);

    item.socket_type = socket_type;
    item.vector_socket_dimensions = dimensions.value_or(3);
    socket_items::set_item_name_and_make_unique<FileOutputItemsAccessor>(node, item, name);

    item.save_as_render = true;
    BKE_image_format_init(&item.format);
    BKE_image_format_update_color_space_for_type(&item.format);
  }

  static std::string validate_name(const StringRef name);

  static std::string socket_identifier_for_item(const NodeCompositorFileOutputItem &item)
  {
    return "Item_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
