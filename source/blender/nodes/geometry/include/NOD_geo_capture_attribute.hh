/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "NOD_socket_items.hh"

#include "BKE_customdata.hh"

namespace blender::nodes {

struct CaptureAttributeItemsAccessor {
  using ItemT = NodeGeometryAttributeCaptureItem;
  static StructRNA *item_srna;
  static int node_type;
  static constexpr const char *node_idname = "GeometryNodeCaptureAttribute";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = false;

  static socket_items::SocketItemsRef<NodeGeometryAttributeCaptureItem> get_items_from_node(
      bNode &node)
  {
    auto *storage = static_cast<NodeGeometryAttributeCapture *>(node.storage);
    return {&storage->capture_items, &storage->capture_items_num, &storage->active_index};
  }

  static void copy_item(const NodeGeometryAttributeCaptureItem &src,
                        NodeGeometryAttributeCaptureItem &dst)
  {
    dst = src;
    dst.name = BLI_strdup_null(dst.name);
  }

  static void destruct_item(NodeGeometryAttributeCaptureItem *item)
  {
    MEM_SAFE_FREE(item->name);
  }

  static void blend_write(BlendWriter *writer, const bNode &node);
  static void blend_read_data(BlendDataReader *reader, bNode &node);

  static eNodeSocketDatatype get_socket_type(const NodeGeometryAttributeCaptureItem &item)
  {
    return *bke::custom_data_type_to_socket_type(eCustomDataType(item.data_type));
  }

  static char **get_name(NodeGeometryAttributeCaptureItem &item)
  {
    return &item.name;
  }

  static bool supports_socket_type(const eNodeSocketDatatype socket_type)
  {
    return bke::socket_type_to_custom_data_type(socket_type).has_value();
  }

  static void init_with_socket_type_and_name(bNode &node,
                                             NodeGeometryAttributeCaptureItem &item,
                                             const eNodeSocketDatatype socket_type,
                                             const char *name)
  {
    auto *storage = static_cast<NodeGeometryAttributeCapture *>(node.storage);
    item.data_type = *bke::socket_type_to_custom_data_type(socket_type);
    item.identifier = storage->next_identifier++;
    socket_items::set_item_name_and_make_unique<CaptureAttributeItemsAccessor>(node, item, name);
  }

  static std::string input_socket_identifier_for_item(const NodeGeometryAttributeCaptureItem &item)
  {
    if (item.identifier == 0) {
      /* This special case exists for compatibility. */
      return "Value";
    }
    return "Value_" + std::to_string(item.identifier);
  }

  static std::string output_socket_identifier_for_item(
      const NodeGeometryAttributeCaptureItem &item)
  {
    if (item.identifier == 0) {
      /* This special case exists for compatibility. */
      return "Attribute";
    }
    return "Attribute_" + std::to_string(item.identifier);
  }
};

}  // namespace blender::nodes
