/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"
#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "ED_node.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geometry_nodes_log.hh"

#include "UI_string_search.hh"

#include "node_intern.hh"

#include <fmt/format.h>

using blender::nodes::geo_eval_log::GeometryInfoLog;
using blender::nodes::geo_eval_log::VolumeGridInfo;

namespace blender::ed::space_node {

struct GridSearchData {
  int32_t node_id;
  char socket_identifier[MAX_NAME];
  bool can_create_grid;
};

/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<GridSearchData>, "");

static Vector<const VolumeGridInfo *> get_grid_names_from_context(const bContext &C,
                                                                  GridSearchData &data)
{
  using namespace nodes::geo_eval_log;

  SpaceNode *snode = CTX_wm_space_node(&C);
  if (!snode) {
    BLI_assert_unreachable();
    return {};
  }
  bNodeTree *node_tree = snode->edittree;
  if (node_tree == nullptr) {
    BLI_assert_unreachable();
    return {};
  }
  const bNode *node = node_tree->node_by_id(data.node_id);
  if (node == nullptr) {
    BLI_assert_unreachable();
    return {};
  }
  const bke::bNodeTreeZones *tree_zones = node_tree->zones();
  if (!tree_zones) {
    return {};
  }
  const ContextualGeoTreeLogs tree_logs = GeoNodesLog::get_contextual_tree_logs(*snode);

  Set<StringRef> names;

  GeoTreeLog *tree_log = tree_logs.get_main_tree_log(*node);
  if (!tree_log) {
    return {};
  }
  tree_log->ensure_socket_values();
  GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node->identifier);
  if (node_log == nullptr) {
    return {};
  }

  Vector<const VolumeGridInfo *> grids;
  for (const bNodeSocket *input_socket : node->input_sockets()) {
    if (input_socket->type != SOCK_GEOMETRY) {
      continue;
    }
    const ValueLog *value_log = tree_log->find_socket_value_log(*input_socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(value_log)) {
      if (const std::optional<GeometryInfoLog::VolumeInfo> &volume_info = geo_log->volume_info) {
        for (const VolumeGridInfo &info : volume_info->grids) {
          if (names.add(info.name)) {
            grids.append(&info);
          }
        }
      }
    }
  }
  return grids;
}

static StringRef grid_data_type_string(const VolumeGridType type)
{
  const char *name = nullptr;
  RNA_enum_name_from_value(rna_enum_volume_grid_data_type_items, type, &name);
  return IFACE_(StringRef(name));
}

static bool grid_search_item_add(uiSearchItems &items, const VolumeGridInfo &item)
{
  std::string text = fmt::format(
      "{}" UI_SEP_CHAR_S "{}", item.name, grid_data_type_string(item.grid_type));
  return UI_search_item_add(&items, text, (void *)&item, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0);
}

static void volume_grid_search_add_items(const StringRef str,
                                         const bool can_create_grid,
                                         const Span<const VolumeGridInfo *> grids,
                                         uiSearchItems &seach_items,
                                         const bool is_first)
{
  static std::string dummy_str;

  /* Any string may be valid, so add the current search string along with the hints. */
  if (!str.is_empty()) {
    bool contained = false;
    for (const VolumeGridInfo *info : grids) {
      if (info->name == str) {
        contained = true;
      }
    }
    if (!contained) {
      dummy_str = str;
      UI_search_item_add(
          &seach_items, str, &dummy_str, can_create_grid ? ICON_ADD : ICON_NONE, 0, 0);
    }
  }

  if (str.is_empty() && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening a name field for the first time would show this search item. */
    dummy_str = str;
    UI_search_item_add(&seach_items, str, &dummy_str, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const StringRef string = is_first ? "" : str;

  ui::string_search::StringSearch<const VolumeGridInfo> search;
  for (const VolumeGridInfo *info : grids) {
    search.add(info->name, info);
  }

  const Vector<const VolumeGridInfo *> filtered_names = search.query(string);
  for (const VolumeGridInfo *info : filtered_names) {
    if (!grid_search_item_add(seach_items, *info)) {
      break;
    }
  }
}

static void grid_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }

  GridSearchData *data = static_cast<GridSearchData *>(arg);

  Vector<const VolumeGridInfo *> grids = get_grid_names_from_context(*C, *data);

  BLI_assert(items);
  volume_grid_search_add_items(str, data->can_create_grid, grids, *items, is_first);
}

static void grid_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }
  std::string *item = static_cast<std::string *>(item_v);
  if (item == nullptr) {
    return;
  }
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    BLI_assert_unreachable();
    return;
  }
  bNodeTree *node_tree = snode->edittree;
  if (node_tree == nullptr) {
    BLI_assert_unreachable();
    return;
  }
  GridSearchData *data = static_cast<GridSearchData *>(data_v);
  bNode *node = node_tree->node_by_id(data->node_id);
  if (node == nullptr) {
    BLI_assert_unreachable();
    return;
  }

  bNodeSocket *socket = bke::node_find_enabled_input_socket(*node, data->socket_identifier);
  if (socket == nullptr) {
    BLI_assert_unreachable();
    return;
  }
  BLI_assert(socket->type == SOCK_STRING);

  bNodeSocketValueString *value = static_cast<bNodeSocketValueString *>(socket->default_value);
  BLI_strncpy_utf8(value->value, item->c_str(), MAX_NAME);

  ED_undo_push(C, "Assign Grid Name");
}

void node_geometry_add_volume_grid_search_button(const bContext & /*C*/,
                                                 const bNode &node,
                                                 PointerRNA &socket_ptr,
                                                 uiLayout &layout,
                                                 const StringRef placeholder)
{
  uiBlock *block = layout.block();
  uiBut *but = uiDefIconTextButR(block,
                                 ButType::SearchMenu,
                                 0,
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
  UI_but_placeholder_set(but, placeholder);

  const bNodeSocket &socket = *static_cast<const bNodeSocket *>(socket_ptr.data);
  GridSearchData *data = MEM_callocN<GridSearchData>(__func__);
  data->node_id = node.identifier;
  data->can_create_grid = node.is_type("GeometryNodeStoreNamedGrid");
  STRNCPY_UTF8(data->socket_identifier, socket.identifier);

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         grid_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         grid_search_exec_fn,
                         nullptr);
}

}  // namespace blender::ed::space_node
