/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_socket_items.hh"

#include "WM_api.hh"

#include "BKE_context.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "ED_node.hh"

#include "DNA_space_types.h"

namespace blender::nodes::socket_items::ops {

inline PointerRNA get_active_node_to_operate_on(bContext *C, const int node_type)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    return PointerRNA_NULL;
  }
  if (!snode->edittree) {
    return PointerRNA_NULL;
  }
  if (ID_IS_LINKED(snode->edittree)) {
    return PointerRNA_NULL;
  }
  const bke::bNodeTreeZones *zones = snode->edittree->zones();
  if (!zones) {
    return PointerRNA_NULL;
  }
  bNode *active_node = nodeGetActive(snode->edittree);
  if (!active_node) {
    return PointerRNA_NULL;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(active_node->identifier);
  if (zone->input_node == active_node) {
    /* Assume the data is generally stored on the output and not the input node. */
    active_node = const_cast<bNode *>(zone->output_node);
  }
  if (active_node->type != node_type) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create(&snode->edittree->id, &RNA_Node, active_node);
}

inline void update_after_node_change(bContext *C, const PointerRNA node_ptr)
{
  bNode *node = static_cast<bNode *>(node_ptr.data);
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(node_ptr.owner_id);

  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, CTX_data_main(C), ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> inline bool editable_node_active_poll(bContext *C)
{
  return get_active_node_to_operate_on(C, Accessor::node_type).data != nullptr;
}

template<typename Accessor>
inline void remove_item(wmOperatorType *ot,
                        const char *name,
                        const char *idname,
                        const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator * /*op*/) -> int {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, Accessor::node_type);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    dna::array::remove_index(
        ref.items, ref.items_num, ref.active_index, *ref.active_index, Accessor::destruct_item);

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };
}

template<typename Accessor>
inline void remove_item_by_index(wmOperatorType *ot,
                                 const char *name,
                                 const char *idname,
                                 const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, Accessor::node_type);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    const int index_to_remove = RNA_int_get(op->ptr, "index");
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    dna::array::remove_index(
        ref.items, ref.items_num, ref.active_index, index_to_remove, Accessor::destruct_item);

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };

  RNA_def_int(ot->srna, "index", 0, 0, INT32_MAX, "Index", "Index to remove", 0, INT32_MAX);
}

template<typename Accessor>
inline void add_item_with_name_and_type(wmOperatorType *ot,
                                        const char *name,
                                        const char *idname,
                                        const char *description)
{
  static_assert(Accessor::has_type);
  static_assert(Accessor::has_name);

  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator * /*op*/) -> int {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, Accessor::node_type);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    const int old_active_index = *ref.active_index;

    eNodeSocketDatatype socket_type;
    std::string name;
    int dst_index;
    if (old_active_index >= 0 && old_active_index < *ref.items_num) {
      dst_index = old_active_index + 1;
      const typename Accessor::ItemT &active_item = (*ref.items)[old_active_index];
      socket_type = eNodeSocketDatatype(active_item.socket_type);
      name = active_item.name;
    }
    else {
      dst_index = *ref.items_num;
      socket_type = SOCK_GEOMETRY;
      /* Empty name so it is based on the type. */
      name = "";
    }
    add_item_with_socket_and_name<Accessor>(node, socket_type, name.c_str());
    dna::array::move_index(*ref.items, *ref.items_num, *ref.items_num - 1, dst_index);
    *ref.active_index = dst_index;

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };
}

template<typename Accessor>
inline void add_item(wmOperatorType *ot,
                     const char *name,
                     const char *idname,
                     const char *description)
{
  static_assert(!Accessor::has_type);
  static_assert(!Accessor::has_name);

  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator * /*op*/) -> int {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, Accessor::node_type);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    socket_items::add_item<Accessor>(node);

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };
}

enum class MoveDirection {
  Up = 0,
  Down = 1,
};

template<typename Accessor>
inline void move_item(wmOperatorType *ot,
                      const char *name,
                      const char *idname,
                      const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, Accessor::node_type);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    const MoveDirection direction = MoveDirection(RNA_enum_get(op->ptr, "direction"));

    SocketItemsRef ref = Accessor::get_items_from_node(node);
    const int old_active_index = *ref.active_index;
    if (direction == MoveDirection::Up && old_active_index > 0) {
      dna::array::move_index(*ref.items, *ref.items_num, old_active_index, old_active_index - 1);
      *ref.active_index -= 1;
    }
    else if (direction == MoveDirection::Down && old_active_index < *ref.items_num - 1) {
      dna::array::move_index(*ref.items, *ref.items_num, old_active_index, old_active_index + 1);
      *ref.active_index += 1;
    }

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };

  static const EnumPropertyItem direction_items[] = {
      {int(MoveDirection::Up), "UP", 0, "Up", ""},
      {int(MoveDirection::Down), "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna, "direction", direction_items, 0, "Direction", "Move direction");
}

}  // namespace blender::nodes::socket_items::ops
