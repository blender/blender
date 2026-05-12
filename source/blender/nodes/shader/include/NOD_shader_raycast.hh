/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "BLI_assert.h"

#include "BKE_customdata.hh"

#include "NOD_socket_items.hh"

namespace blender::nodes {

struct RaycastSampleAttributeItemsAccessor : public socket_items::SocketItemsAccessorDefaults {
  using ItemT = NodeRaycastSampleAttributeItem;
  static StructRNA **item_srna;
  static int node_type;
  static constexpr StringRefNull node_idname = "ShaderNodeRaycast";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = false;
  struct operator_idnames {
    static constexpr StringRefNull add_item = "NODE_OT_sample_attribute_items_item_add";
    static constexpr StringRefNull remove_item = "NODE_OT_sample_attribute_items_item_remove";
    static constexpr StringRefNull move_item = "NODE_OT_sample_attribute_items_item_move";
  };
  struct ui_idnames {
    static constexpr StringRefNull list = "NODE_UL_capture_items_list";
  };
  struct rna_names {
    static constexpr StringRefNull items = "sample_attribute_items";
    static constexpr StringRefNull active_index = "active_index";
  };

  static socket_items::SocketItemsRef<NodeRaycastSampleAttributeItem> get_items_from_node(
      bNode &node)
  {
    auto *storage = static_cast<NodeShaderRaycast *>(node.storage);
    return {&storage->sample_attribute_items,
            &storage->sample_attribute_items_num,
            &storage->active_index};
  }

  static void copy_item(const NodeRaycastSampleAttributeItem &src,
                        NodeRaycastSampleAttributeItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(NodeRaycastSampleAttributeItem *item)
  {
    MEM_SAFE_DELETE(item->name);
  }

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

  static eNodeSocketDatatype get_socket_type(const NodeRaycastSampleAttributeItem &item)
  {
    return *bke::custom_data_type_to_socket_type(eCustomDataType(item.data_type));
  }

  static char **get_name(NodeRaycastSampleAttributeItem &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type, const int /*ntree_type*/)
  {
    return ELEM(socket_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             NodeRaycastSampleAttributeItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeShaderRaycast *>(node.storage);
    item.data_type = *bke::socket_type_to_custom_data_type(socket_type);
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<RaycastSampleAttributeItemsAccessor>(
        node, item, name);
  }

  static std::string input_socket_identifier_for_item(const NodeRaycastSampleAttributeItem &item)
  {
    return "Value_" + std::to_string(item.identifier);
  }
  static std::string output_socket_identifier_for_item(const NodeRaycastSampleAttributeItem &item)
  {
    return "Attribute_" + std::to_string(item.identifier);
  }
  static std::string output_socket_identifier_for_item_alpha(
      const NodeRaycastSampleAttributeItem &item)
  {
    BLI_assert(item.data_type == CD_PROP_COLOR);
    return "Attribute_" + std::to_string(item.identifier) + "_Alpha";
  }
};

}  // namespace blender::nodes
