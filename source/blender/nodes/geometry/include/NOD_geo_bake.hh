/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "DNA_modifier_types.h"
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
  static int item_dna_type;
  static constexpr const char *node_idname = "GeometryNodeBake";
  static constexpr bool has_type = true;
  static constexpr bool has_name = true;
  static constexpr bool has_single_identifier_str = true;
  struct operator_idnames {
    static constexpr const char *add_item = "NODE_OT_bake_node_item_add";
    static constexpr const char *remove_item = "NODE_OT_bake_node_item_remove";
    static constexpr const char *move_item = "NODE_OT_bake_node_item_move";
  };
  struct ui_idnames {
    static constexpr const char *list = "DATA_UL_bake_node_items";
  };
  struct rna_names {
    static constexpr const char *items = "bake_items";
    static constexpr const char *active_index = "active_index";
  };

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

  static void blend_write_item(BlendWriter *writer, const ItemT &item);
  static void blend_read_data_item(BlendDataReader *reader, ItemT &item);

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
  std::optional<IndexRange> frame_range;
  bool bake_still;
  bool is_baked;
  std::optional<NodesModifierBakeTarget> bake_target;
};

[[nodiscard]] bool get_bake_draw_context(const bContext *C,
                                         const bNode &node,
                                         BakeDrawContext &r_ctx);

std::string get_baked_string(const BakeDrawContext &ctx);

std::optional<std::string> get_bake_state_string(const BakeDrawContext &ctx);
void draw_common_bake_settings(bContext *C, BakeDrawContext &ctx, uiLayout *layout);
void draw_bake_button_row(const BakeDrawContext &ctx,
                          uiLayout *layout,
                          bool is_in_sidebar = false);

}  // namespace blender::nodes
