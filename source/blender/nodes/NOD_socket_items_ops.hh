/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLT_translation.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_items.hh"

#include "WM_api.hh"

#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_node.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

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
  return RNA_pointer_create_discrete(&snode->edittree->id, RNA_Node, node);
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
  ot->flag = OPTYPE_UNDO;

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
    bNode &node = *static_cast<bNode *>(node_ptr.data);
    SocketItemsRef ref = Accessor::get_items_from_node(node);
    if (*ref.items_num > 0 && ref.active_index) {
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
  ot->flag = OPTYPE_UNDO;

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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static const char *socket_type_id = "socket_type";
  static const char *item_name_id = "item_name";

  ot->invoke = [](bContext *C, wmOperator *op, const wmEvent *event) {
    const bool show_dialog = RNA_boolean_get(op->ptr, "show_dialog");
    const bool needs_dialog = Accessor::has_type || Accessor::has_name;
    if (show_dialog && needs_dialog) {
      return WM_operator_props_popup_confirm_ex(C, op, event, IFACE_("Add Item"));
    }
    return op->type->exec(C, op);
  };

  ot->ui = [](bContext * /*C*/, wmOperator *op) {
    ui::Layout &layout = *op->layout;
    if constexpr (Accessor::has_name) {
      PropertyRNA *prop = RNA_struct_find_property(op->ptr, item_name_id);
      ui::Layout &row = layout.row(true);
      row.activate_init_set(true);
      row.prop(op->ptr, prop, RNA_NO_INDEX, 0, UI_ITEM_NONE, "", ICON_NONE, IFACE_("Name"));
    }
    if constexpr (Accessor::has_type) {
      layout.prop(op->ptr, socket_type_id, UI_ITEM_NONE, "", ICON_NONE);
    }
  };

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    PointerRNA node_ptr = get_active_node_to_operate_on(C, op, Accessor::node_idname);
    if (node_ptr.data == nullptr) {
      return OPERATOR_CANCELLED;
    }
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(node_ptr.owner_id);
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

    /* Determine name if necessary. */
    const bool init_from_active = RNA_boolean_get(op->ptr, "init_from_active");
    std::optional<std::string> name;
    if constexpr (Accessor::has_name) {
      if (!init_from_active) {
        name = RNA_string_get(op->ptr, item_name_id);
      }
      else {
        name = active_item ? active_item->name : "";
        if constexpr (Accessor::has_custom_initial_name) {
          name = Accessor::custom_initial_name(node, *name);
        }
      }
    }

    /* Determine socket type if necessary. */
    std::optional<eNodeSocketDatatype> socket_type;
    if constexpr (Accessor::has_type) {
      if (!init_from_active) {
        socket_type = eNodeSocketDatatype(RNA_enum_get(op->ptr, socket_type_id));
      }
      else if (active_item) {
        socket_type = Accessor::get_socket_type(*active_item);
      }
      else {
        socket_type = Accessor::supports_socket_type(SOCK_GEOMETRY, ntree->type) ? SOCK_GEOMETRY :
                                                                                   SOCK_FLOAT;
      }
    }

    if constexpr (Accessor::has_type && Accessor::has_name) {
      socket_items::add_item_with_socket_type_and_name<Accessor>(
          *ntree, node, *socket_type, name->c_str());
    }
    else if constexpr (!Accessor::has_type && Accessor::has_name) {
      socket_items::add_item_with_name<Accessor>(node, name->c_str());
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

  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "show_dialog",
                         false,
                         "Show Dialog",
                         "Show a dialog to edit the initial properties");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "init_from_active",
      true,
      "Init from Active",
      "Instead of using the provided name or type, copy the state of the active item");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  if constexpr (Accessor::has_type) {
    prop = RNA_def_enum(ot->srna,
                        socket_type_id,
                        rna_enum_node_socket_data_type_items,
                        SOCK_FLOAT,
                        "Socket Type",
                        "Type of the new socket item");
    RNA_def_property_enum_funcs_runtime(
        prop,
        nullptr,
        nullptr,
        [](bContext *C, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
          if (!C) {
            *r_free = false;
            return rna_enum_node_socket_data_type_items;
          }
          *r_free = true;
          SpaceNode *snode = CTX_wm_space_node(C);
          return enum_items_filter(rna_enum_node_socket_data_type_items,
                                   [&](const EnumPropertyItem &item) -> bool {
                                     return Accessor::supports_socket_type(
                                         eNodeSocketDatatype(item.value), snode->edittree->type);
                                   });
        },
        nullptr,
        nullptr);
  }
  if constexpr (Accessor::has_name) {
    RNA_def_string(ot->srna, item_name_id, nullptr, 0, "Item Name", "Name of the new socket item");
  }
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
  ot->flag = OPTYPE_UNDO;

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
 * Creates operator to add a node item.
 * The idname is passed in explicitly, so that it is more searchable compared to when it would be
 * computed automatically.
 */
template<typename Accessor> inline void make_add_item_operator()
{
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::add_item<Accessor>(ot,
                                          "Add Item",
                                          Accessor::operator_idnames::add_item.c_str(),
                                          "Add item below active item");
  });
}

/**
 * Creates operator to remove the active item.
 * The idname is passed in explicitly, so that it is more searchable compared to when it would be
 * computed automatically.
 */
template<typename Accessor> inline void make_remove_active_item_operator()
{
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::remove_active_item<Accessor>(
        ot, "Remove Item", Accessor::operator_idnames::remove_item.c_str(), "Remove active item");
  });
}

/**
 * Creates operator to remove an item by index.
 * The idname is passed in explicitly, so that it is more searchable compared to when it would be
 * computed automatically.
 */
template<typename Accessor> inline void make_remove_item_by_index_operator()
{
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::remove_item_by_index<Accessor>(
        ot, "Remove Item", Accessor::operator_idnames::remove_item.c_str(), "Remove active item");
  });
}

/**
 * Creates operator to move a node item.
 * The idname is passed in explicitly, so that it is more searchable compared to when it would be
 * computed automatically.
 */
template<typename Accessor> inline void make_move_item_operator()
{
  WM_operatortype_append([](wmOperatorType *ot) {
    socket_items::ops::move_active_item<Accessor>(
        ot, "Move Item", Accessor::operator_idnames::move_item.c_str(), "Move active item");
  });
}

/**
 * Creates simple operators for adding, removing and moving items.
 * The idnames are passed in explicitly, so that they are more searchable compared to when they
 * would be computed automatically.
 */
template<typename Accessor> inline void make_common_operators()
{
  make_add_item_operator<Accessor>();
  make_remove_active_item_operator<Accessor>();
  make_move_item_operator<Accessor>();
}

}  // namespace blender::nodes::socket_items::ops
