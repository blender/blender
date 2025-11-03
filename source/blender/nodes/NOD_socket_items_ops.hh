/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_socket_items.hh"

#include "WM_api.hh"

#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "ED_node.hh"

#include "DNA_space_types.h"

namespace blender::nodes::socket_items::ops {

inline PointerRNA get_active_node_to_operate_on(bContext *C,
                                                wmOperator *op,
                                                const StringRef node_idname)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    return PointerRNA_NULL;
  }
  if (!snode->edittree) {
    return PointerRNA_NULL;
  }
  if (!ID_IS_EDITABLE(snode->edittree)) {
    return PointerRNA_NULL;
  }

  bNode *node = nullptr;
  if (RNA_struct_property_is_set(op->ptr, "node_identifier")) {
    const int id = RNA_int_get(op->ptr, "node_identifier");
    node = snode->edittree->node_by_id(id);
  }
  else {
    node = bke::node_get_active(*snode->edittree);
  }
  if (!node) {
    return PointerRNA_NULL;
  }

  if (bke::zone_type_by_node_type(node->type_legacy) != nullptr) {
    const bke::bNodeTreeZones *zones = snode->edittree->zones();
    if (!zones) {
      return PointerRNA_NULL;
    }
    if (const bke::bNodeTreeZone *zone = zones->get_zone_by_node(node->identifier)) {
      if (zone->input_node() == node) {
        /* Assume the data is generally stored on the output and not the input node. */
        node = const_cast<bNode *>(zone->output_node());
      }
    }
  }

  if (node->idname != node_idname) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&snode->edittree->id, &RNA_Node, node);
}

inline void update_after_node_change(bContext *C, const PointerRNA node_ptr)
{
  bNode *node = static_cast<bNode *>(node_ptr.data);
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(node_ptr.owner_id);

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*CTX_data_main(C), ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> inline bool editable_node_active_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    return false;
  }
  if (!snode->edittree) {
    return false;
  }
  if (!ID_IS_EDITABLE(snode->edittree)) {
    return false;
  }
  return true;
}

inline void add_node_identifier_property(wmOperatorType *ot)
{
  RNA_def_int(ot->srna,
              "node_identifier",
              0,
              0,
              INT32_MAX,
              "Node Identifier",
              "Optional identifier of the node to operate on",
              0,
              INT32_MAX);
}

template<typename Accessor>
inline void remove_active_item(wmOperatorType *ot,
                               const char *name,
                               const char *idname,
                               const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    if (*ref.items_num > 0) {
      dna::array::remove_index(
          ref.items, ref.items_num, ref.active_index, *ref.active_index, Accessor::destruct_item);
      update_after_node_change(C, node_ptr);
    }
    return OPERATOR_FINISHED;
  };

  add_node_identifier_property(ot);
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

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
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
inline void add_item(wmOperatorType *ot,
                     const char *name,
                     const char *idname,
                     const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
    if (node_ptr.data == nullptr) {
      return OPERATOR_CANCELLED;
    }
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    const typename Accessor::ItemT *active_item = nullptr;
    int dst_index = *ref.items_num;
    if (ref.active_index) {
      const int old_active_index = *ref.active_index;
      if (old_active_index >= 0 && old_active_index < *ref.items_num) {
        active_item = &(*ref.items)[old_active_index];
        dst_index = active_item ? old_active_index + 1 : *ref.items_num;
      }
    }

    if constexpr (Accessor::has_type && Accessor::has_name) {
      std::string name = active_item ? active_item->name : "";
      if constexpr (Accessor::has_custom_initial_name) {
        name = Accessor::custom_initial_name(node, name);
      }
      bNodeTree *ntree = reinterpret_cast<bNodeTree *>(node_ptr.owner_id);
      socket_items::add_item_with_socket_type_and_name<Accessor>(
          *ntree,
          node,
          active_item ?
              Accessor::get_socket_type(*active_item) :
              (Accessor::supports_socket_type(SOCK_GEOMETRY, ntree->type) ? SOCK_GEOMETRY :
                                                                            SOCK_FLOAT),
          /* Empty name so it is based on the type. */
          name.c_str());
    }
    else if constexpr (!Accessor::has_type && Accessor::has_name) {
      socket_items::add_item_with_name<Accessor>(node, active_item ? active_item->name : "");
    }
    else if constexpr (!Accessor::has_type && !Accessor::has_name) {
      socket_items::add_item<Accessor>(node);
    }
    else {
      BLI_assert_unreachable();
    }

    dna::array::move_index(*ref.items, *ref.items_num, *ref.items_num - 1, dst_index);
    if (ref.active_index) {
      *ref.active_index = dst_index;
    }

    update_after_node_change(C, node_ptr);
    return OPERATOR_FINISHED;
  };

  add_node_identifier_property(ot);
}

enum class MoveDirection {
  Up = 0,
  Down = 1,
};

template<typename Accessor>
inline void move_active_item(wmOperatorType *ot,
                             const char *name,
                             const char *idname,
                             const char *description)
{
  ot->name = name;
  ot->idname = idname;
  ot->description = description;
  ot->poll = editable_node_active_poll<Accessor>;

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
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
  add_node_identifier_property(ot);
}

/**
 * Creates simple operators for adding, removing and moving items.
 * The idnames are passed in explicitly, so that they are more searchable compared to when they
 * would be computed automatically.
 */
template<typename Accessor> inline void make_common_operators()
{
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::add_item<Accessor>(ot,
                                          "Add Item",
                                          Accessor::operator_idnames::add_item.c_str(),
                                          "Add item below active item");
  });
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::remove_active_item<Accessor>(
        ot, "Remove Item", Accessor::operator_idnames::remove_item.c_str(), "Remove active item");
  });
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::move_active_item<Accessor>(
        ot, "Move Item", Accessor::operator_idnames::move_item.c_str(), "Move active item");
  });
}

}  // namespace blender::nodes::socket_items::ops
