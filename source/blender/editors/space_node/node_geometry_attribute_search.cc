/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rect.h"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_search.h"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_node.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"

#include "NOD_geometry_nodes_eval_log.hh"

#include "node_intern.hh"

namespace geo_log = blender::nodes::geometry_nodes_eval_log;
using geo_log::GeometryAttributeInfo;

namespace blender::ed::space_node {

struct AttributeSearchData {
  char node_name[MAX_NAME];
  char socket_identifier[MAX_NAME];
};

/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<AttributeSearchData>, "");

static Vector<const GeometryAttributeInfo *> get_attribute_info_from_context(
    const bContext &C, AttributeSearchData &data)
{
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
  bNode *node = nodeFindNodebyName(node_tree, data.node_name);
  if (node == nullptr) {
    BLI_assert_unreachable();
    return {};
  }

  /* For the attribute input node, collect attribute information from all nodes in the group. */
  if (node->type == GEO_NODE_INPUT_NAMED_ATTRIBUTE) {
    const geo_log::TreeLog *tree_log = geo_log::ModifierLog::find_tree_by_node_editor_context(
        *snode);
    if (tree_log == nullptr) {
      return {};
    }

    Vector<const GeometryAttributeInfo *> attributes;
    Set<StringRef> names;
    tree_log->foreach_node_log([&](const geo_log::NodeLog &node_log) {
      for (const geo_log::SocketLog &socket_log : node_log.input_logs()) {
        const geo_log::ValueLog *value_log = socket_log.value();
        if (const geo_log::GeometryValueLog *geo_value_log =
                dynamic_cast<const geo_log::GeometryValueLog *>(value_log)) {
          for (const GeometryAttributeInfo &attribute : geo_value_log->attributes()) {
            if (names.add(attribute.name)) {
              attributes.append(&attribute);
            }
          }
        }
      }
    });
    return attributes;
  }

  const geo_log::NodeLog *node_log = geo_log::ModifierLog::find_node_by_node_editor_context(
      *snode, data.node_name);
  if (node_log == nullptr) {
    return {};
  }
  return node_log->lookup_available_attributes();
}

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }

  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);

  Vector<const GeometryAttributeInfo *> infos = get_attribute_info_from_context(*C, *data);

  /* Remove the deprecated normal attribute from the search. */
  for (const int i : infos.index_range()) {
    if (infos[i]->domain == ATTR_DOMAIN_FACE && infos[i]->name == "normal") {
      infos.remove(i);
      break;
    }
  }

  ui::attribute_search_add_items(str, true, infos, items, is_first);
}

/**
 * Some custom data types don't correspond to node types and therefore can't be
 * used by the named attribute input node. Find the best option or fallback to float.
 */
static CustomDataType data_type_in_attribute_input_node(const CustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
    case CD_PROP_INT32:
    case CD_PROP_FLOAT3:
    case CD_PROP_COLOR:
    case CD_PROP_BOOL:
      return type;
    case CD_PROP_BYTE_COLOR:
      return CD_PROP_COLOR;
    case CD_PROP_STRING:
      /* Unsupported currently. */
      return CD_PROP_FLOAT;
    case CD_PROP_FLOAT2:
      /* No 2D vector sockets currently. */
      return CD_PROP_FLOAT3;
    case CD_PROP_INT8:
      return CD_PROP_INT32;
    default:
      return CD_PROP_FLOAT;
  }
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }
  GeometryAttributeInfo *item = (GeometryAttributeInfo *)item_v;
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
  AttributeSearchData *data = static_cast<AttributeSearchData *>(data_v);
  bNode *node = nodeFindNodebyName(node_tree, data->node_name);
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

  /* For the attribute input node, also adjust the type and links connected to the output. */
  if (node->type == GEO_NODE_INPUT_NAMED_ATTRIBUTE) {
    NodeGeometryInputNamedAttribute &storage = *(NodeGeometryInputNamedAttribute *)node->storage;
    const CustomDataType new_type = data_type_in_attribute_input_node(item->data_type);
    if (new_type != storage.data_type) {
      storage.data_type = new_type;
      /* Make the output socket with the new type on the attribute input node active. */
      node->typeinfo->updatefunc(node_tree, node);

      /* Relink all node links to the newly active output socket. */
      bNodeSocket *output_socket = bke::node_find_enabled_output_socket(*node, "Attribute");
      LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
        if (link->fromnode == node) {
          link->fromsock = output_socket;
          BKE_ntree_update_tag_link_changed(node_tree);
        }
      }
    }
    BKE_ntree_update_tag_node_property(node_tree, node);
    ED_node_tree_propagate_change(C, CTX_data_main(C), node_tree);
  }

  bNodeSocketValueString *value = static_cast<bNodeSocketValueString *>(socket->default_value);
  BLI_strncpy(value->value, item->name.c_str(), MAX_NAME);

  ED_undo_push(C, "Assign Attribute Name");
}

void node_geometry_add_attribute_search_button(const bContext &UNUSED(C),
                                               const bNode &node,
                                               PointerRNA &socket_ptr,
                                               uiLayout &layout)
{
  uiBlock *block = uiLayoutGetBlock(&layout);
  uiBut *but = uiDefIconTextButR(block,
                                 UI_BTYPE_SEARCH_MENU,
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
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 "");

  const bNodeSocket &socket = *static_cast<const bNodeSocket *>(socket_ptr.data);
  AttributeSearchData *data = MEM_new<AttributeSearchData>(__func__);
  BLI_strncpy(data->node_name, node.name, sizeof(data->node_name));
  BLI_strncpy(data->socket_identifier, socket.identifier, sizeof(data->socket_identifier));

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);
}

}  // namespace blender::ed::space_node
