/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "WM_api.hh"

#include "UI_interface.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_screen.hh"

#include "NOD_socket_items.hh"

namespace blender::nodes::socket_items::ui {

template<typename Accessor>
static void draw_item_in_list(uiList * /*ui_list*/,
                              const bContext *C,
                              uiLayout *layout,
                              PointerRNA * /*idataptr*/,
                              PointerRNA *itemptr,
                              int /*icon*/,
                              PointerRNA * /*active_dataptr*/,
                              const char * /*active_propname*/,
                              int /*index*/,
                              int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  if constexpr (Accessor::has_type) {
    float4 color;
    RNA_float_get_array(itemptr, "color", color);
    uiTemplateNodeSocket(row, const_cast<bContext *>(C), color);
  }
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemR(row, itemptr, "name", UI_ITEM_NONE, "", ICON_NONE);
}

/**
 * Draws a ui-list that contains the items. The list also has operators to add, remove and reorder
 * items.
 */
template<typename Accessor>
static void draw_items_list_with_operators(const bContext *C,
                                           uiLayout *layout,
                                           const bNodeTree &tree,
                                           const bNode &node)
{
  BLI_assert(Accessor::node_type == node.type_legacy);
  PointerRNA node_ptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&tree.id), &RNA_Node, const_cast<bNode *>(&node));

  static const uiListType *items_list = []() {
    uiListType *list = MEM_cnew<uiListType>(Accessor::ui_idnames::list);
    STRNCPY(list->idname, Accessor::ui_idnames::list);
    list->draw_item = draw_item_in_list<Accessor>;
    WM_uilisttype_add(list);
    return list;
  }();

  uiLayout *row = uiLayoutRow(layout, false);
  uiTemplateList(row,
                 C,
                 items_list->idname,
                 "",
                 &node_ptr,
                 Accessor::rna_names::items,
                 &node_ptr,
                 Accessor::rna_names::active_index,
                 nullptr,
                 3,
                 5,
                 UILST_LAYOUT_DEFAULT,
                 0,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  uiLayout *ops_col = uiLayoutColumn(row, false);
  {
    uiLayout *add_remove_col = uiLayoutColumn(ops_col, true);
    uiItemO(add_remove_col, "", ICON_ADD, Accessor::operator_idnames::add_item);
    uiItemO(add_remove_col, "", ICON_REMOVE, Accessor::operator_idnames::remove_item);
  }
  {
    uiLayout *up_down_col = uiLayoutColumn(ops_col, true);
    uiItemEnumO(
        up_down_col, Accessor::operator_idnames::move_item, "", ICON_TRIA_UP, "direction", 0);
    uiItemEnumO(
        up_down_col, Accessor::operator_idnames::move_item, "", ICON_TRIA_DOWN, "direction", 1);
  }
}

/** Draw properties of the active item if there is any. */
template<typename Accessor>
static void draw_active_item_props(const bNodeTree &tree,
                                   const bNode &node,
                                   const FunctionRef<void(PointerRNA *item_ptr)> draw_item)
{
  using ItemT = typename Accessor::ItemT;
  BLI_assert(Accessor::node_type == node.type_legacy);

  SocketItemsRef<ItemT> ref = Accessor::get_items_from_node(const_cast<bNode &>(node));
  if (*ref.active_index < 0) {
    return;
  }
  if (*ref.active_index >= *ref.items_num) {
    return;
  }

  ItemT &item = (*ref.items)[*ref.active_index];
  PointerRNA item_ptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&tree.id), Accessor::item_srna, &item);
  draw_item(&item_ptr);
}

}  // namespace blender::nodes::socket_items::ui
