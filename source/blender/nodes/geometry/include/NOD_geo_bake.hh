/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "DNA_node_types.h"

#include "NOD_geo_simulation.hh"
#include "NOD_socket_items.hh"

#include "RNA_access.hh"

#include "BLI_index_range.hh"

struct NodesModifierData;
struct NodesModifierBake;
struct SpaceNode;
struct Object;

namespace blender::nodes {

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
  static eNodeSocketDatatype get_socket_type(const NodeGeometryBakeItem &item)
  {
    return eNodeSocketDatatype(item.socket_type);
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

struct BakeDrawContext {
  const bNode *node;
  SpaceNode *snode;
  const Object *object;
  const NodesModifierData *nmd;
  const NodesModifierBake *bake;
  PointerRNA bake_rna;
  std::optional<IndexRange> baked_range;
  bool bake_still;
  bool is_baked;
};

[[nodiscard]] bool get_bake_draw_context(const bContext *C,
                                         const bNode &node,
                                         BakeDrawContext &r_ctx);

std::string get_baked_string(const BakeDrawContext &ctx);

}  // namespace blender::nodes
