/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "ED_node.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_socket.hh"

#include "node_intern.hh"

using blender::nodes::geo_eval_log::GeometryInfoLog;

namespace blender::ed::space_node {

struct LayerSearchData {
  int32_t node_id;
  char socket_identifier[MAX_NAME];
};

/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<LayerSearchData>, "");

static Vector<const std::string *> get_layer_names_from_context(const bContext &C,
                                                                LayerSearchData &data)
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

  /* For the named layer selection input node, collect layer names from all nodes in the group. */
  if (node->type_legacy == GEO_NODE_INPUT_NAMED_LAYER_SELECTION) {
    Vector<const std::string *> layer_names;
    tree_logs.foreach_tree_log([&](GeoTreeLog &tree_log) {
      tree_log.ensure_socket_values();
      tree_log.ensure_layer_names();
      for (const std::string &name : tree_log.all_layer_names) {
        if (!names.add(name)) {
          continue;
        }
        layer_names.append(&name);
      }
    });
    return layer_names;
  }
  GeoTreeLog *tree_log = tree_logs.get_main_tree_log(*node);
  if (!tree_log) {
    return {};
  }
  tree_log->ensure_socket_values();
  GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node->identifier);
  if (node_log == nullptr) {
    return {};
  }

  Vector<const std::string *> layer_names;
  for (const bNodeSocket *input_socket : node->input_sockets()) {
    if (input_socket->type != SOCK_GEOMETRY) {
      continue;
    }
    const ValueLog *value_log = tree_log->find_socket_value_log(*input_socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(value_log)) {
      if (const std::optional<GeometryInfoLog::GreasePencilInfo> &grease_pencil_info =
              geo_log->grease_pencil_info)
      {
        for (const std::string &name : grease_pencil_info->layer_names) {
          if (names.add(name)) {
            layer_names.append(&name);
          }
        }
      }
    }
  }
  return layer_names;
}

static void layer_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }

  LayerSearchData *data = static_cast<LayerSearchData *>(arg);

  Vector<const std::string *> names = get_layer_names_from_context(*C, *data);

  BLI_assert(items);
  ui::grease_pencil_layer_search_add_items(str, names, *items, is_first);
}

static void layer_search_exec_fn(bContext *C, void *data_v, void *item_v)
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
  LayerSearchData *data = static_cast<LayerSearchData *>(data_v);
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

  ED_undo_push(C, "Assign Layer Name");
}

void node_geometry_add_layer_search_button(const bContext & /*C*/,
                                           const bNode &node,
                                           PointerRNA &socket_ptr,
                                           uiLayout &layout,
                                           const StringRef placeholder)
{
  uiBlock *block = layout.block();
  uiBut *but = uiDefIconTextButR(block,
                                 ButType::SearchMenu,
                                 0,
                                 ICON_OUTLINER_DATA_GP_LAYER,
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
  LayerSearchData *data = MEM_callocN<LayerSearchData>(__func__);
  data->node_id = node.identifier;
  STRNCPY_UTF8(data->socket_identifier, socket.identifier);

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         layer_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         layer_search_exec_fn,
                         nullptr);
}

}  // namespace blender::ed::space_node
