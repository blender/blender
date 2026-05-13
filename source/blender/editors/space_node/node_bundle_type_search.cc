/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "BLI_string_utf8.h"

#include "ED_screen.hh"

#include "NOD_bundle_type.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_sync_sockets.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_string_search.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

struct BundleTypeSocketSearchData {
  int32_t node_id;

  bNode *find_node(const bContext &C) const
  {
    SpaceNode *snode = CTX_wm_space_node(&C);
    if (!snode) {
      return nullptr;
    }
    bNodeTree *node_tree = snode->edittree;
    if (!node_tree) {
      return nullptr;
    }
    node_tree->ensure_topology_cache();
    return node_tree->node_by_id(this->node_id);
  }
};
/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
static_assert(std::is_trivially_destructible_v<BundleTypeSocketSearchData>);

static Vector<std::string> get_type_names_from_context(const bContext &C,
                                                       const BundleTypeSocketSearchData &data)
{
  const bNode *node = data.find_node(C);
  if (!node) {
    return {};
  }

  return nodes::BundleTypeRegistry::get_all_flat_type_names();
}

static void bundle_type_string_search(
    const bContext *C, void *arg, const char *str_ptr, ui::SearchItems *items, const bool is_first)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }

  const StringRef str = str_ptr;

  const auto *data = static_cast<BundleTypeSocketSearchData *>(arg);
  const Vector<std::string> names = get_type_names_from_context(*C, *data);

  /* Any string is valid, so add the current search string along with the hints. */
  if (!str.is_empty()) {
    if (!names.contains(str)) {
      ui::search_item_add(items, str, nullptr, ICON_NONE, 0, 0);
    }
  }

  if (str.is_empty() && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass. */
    ui::search_item_add(items, str, nullptr, ICON_X, 0, 0);
  }

  const StringRef search_string = is_first ? "" : str;
  ui::string_search::StringSearch<const std::string> search;
  for (const std::string &name : names) {
    search.add(name, &name);
  }
  const Vector<const std::string *> filtered_items = search.query(search_string);

  for (const std::string *item : filtered_items) {
    if (!ui::search_item_add(items, *item, nullptr, ICON_NONE, 0, 0)) {
      break;
    }
  }
}

static void bundle_type_string_search_exec(bContext *C, void *data_v, void * /*item_v*/)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }
  const auto &data = *static_cast<BundleTypeSocketSearchData *>(data_v);
  bNode *node = data.find_node(*C);
  if (!node) {
    return;
  }
  if (!node->is_type("NodeCombineBundle"_ustr)) {
    return;
  }
  const auto &storage = *static_cast<NodeCombineBundle *>(node->storage);
  if (storage.items_num != 1) {
    return;
  }
  const NodeCombineBundleItem &item = storage.items[0];
  if (item.socket_type != SOCK_STRING) {
    return;
  }
  if (item.name != nodes::Bundle::type_item_name) {
    return;
  }
  nodes::sync_node(*C, *node, nullptr);
  BKE_main_ensure_invariants(*CTX_data_main(C));
}

void node_bundle_type_add_string_search_button(const bContext & /*C*/,
                                               const bNode &node,
                                               PointerRNA &socket_ptr,
                                               ui::Layout &layout,
                                               const StringRef placeholder)
{
  ui::Block *block = layout.block();
  ui::Button *but = uiDefIconTextButR(block,
                                      ui::ButtonType::SearchMenu,
                                      ICON_NONE,
                                      "",
                                      0,
                                      0,
                                      10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                      UI_UNIT_Y,
                                      &socket_ptr,
                                      "default_value",
                                      0,
                                      "");
  ui::button_placeholder_set(but, placeholder);

  BundleTypeSocketSearchData *data = MEM_new_zeroed<BundleTypeSocketSearchData>(__func__);
  data->node_id = node.identifier;

  ui::button_func_search_set_results_are_suggestions(but, true);
  ui::button_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  ui::button_func_search_set(but,
                             nullptr,
                             bundle_type_string_search,
                             data,
                             true,
                             nullptr,
                             bundle_type_string_search_exec,
                             nullptr);
}

}  // namespace blender::ed::space_node
