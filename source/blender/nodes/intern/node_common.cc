/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstddef>
#include <cstring>

#include "DNA_asset_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"

#include "MEM_guardedalloc.h"

#include "NOD_common.h"
#include "NOD_node_declaration.hh"
#include "NOD_register.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"
#include "node_common.h"
#include "node_util.hh"

using blender::Map;
using blender::MultiValueMap;
using blender::Set;
using blender::Stack;
using blender::StringRef;
using blender::Vector;

namespace node_interface = blender::bke::node_interface;

/* -------------------------------------------------------------------- */
/** \name Node Group
 * \{ */

static bNodeSocket *find_matching_socket(ListBase &sockets, StringRef identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &sockets) {
    if (socket->identifier == identifier) {
      return socket;
    }
  }
  return nullptr;
}

bNodeSocket *node_group_find_input_socket(bNode *groupnode, const char *identifier)
{
  return find_matching_socket(groupnode->inputs, identifier);
}

bNodeSocket *node_group_find_output_socket(bNode *groupnode, const char *identifier)
{
  return find_matching_socket(groupnode->outputs, identifier);
}

void node_group_label(const bNodeTree * /*ntree*/,
                      const bNode *node,
                      char *label,
                      int label_maxncpy)
{
  BLI_strncpy(
      label, (node->id) ? node->id->name + 2 : IFACE_("Missing Data-Block"), label_maxncpy);
}

int node_group_ui_class(const bNode *node)
{
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
  if (!group) {
    return NODE_CLASS_GROUP;
  }
  switch (blender::bke::NodeGroupColorTag(group->color_tag)) {
    case blender::bke::NodeGroupColorTag::None:
      return NODE_CLASS_GROUP;
    case blender::bke::NodeGroupColorTag::Attribute:
      return NODE_CLASS_ATTRIBUTE;
    case blender::bke::NodeGroupColorTag::Color:
      return NODE_CLASS_OP_COLOR;
    case blender::bke::NodeGroupColorTag::Converter:
      return NODE_CLASS_CONVERTER;
    case blender::bke::NodeGroupColorTag::Distort:
      return NODE_CLASS_DISTORT;
    case blender::bke::NodeGroupColorTag::Filter:
      return NODE_CLASS_OP_FILTER;
    case blender::bke::NodeGroupColorTag::Geometry:
      return NODE_CLASS_GEOMETRY;
    case blender::bke::NodeGroupColorTag::Input:
      return NODE_CLASS_INPUT;
    case blender::bke::NodeGroupColorTag::Matte:
      return NODE_CLASS_MATTE;
    case blender::bke::NodeGroupColorTag::Output:
      return NODE_CLASS_OUTPUT;
    case blender::bke::NodeGroupColorTag::Script:
      return NODE_CLASS_SCRIPT;
    case blender::bke::NodeGroupColorTag::Shader:
      return NODE_CLASS_SHADER;
    case blender::bke::NodeGroupColorTag::Texture:
      return NODE_CLASS_TEXTURE;
    case blender::bke::NodeGroupColorTag::Vector:
      return NODE_CLASS_OP_VECTOR;
  }
  return NODE_CLASS_GROUP;
}

bool node_group_poll_instance(const bNode *node,
                              const bNodeTree *nodetree,
                              const char **r_disabled_hint)
{
  if (!node->typeinfo->poll(node->typeinfo, nodetree, r_disabled_hint)) {
    return false;
  }
  const bNodeTree *grouptree = reinterpret_cast<const bNodeTree *>(node->id);
  if (!grouptree) {
    return true;
  }
  return blender::bke::node_group_poll(nodetree, grouptree, r_disabled_hint);
}

std::string node_group_ui_description(const bNode &node)
{
  if (!node.id) {
    return "";
  }
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id);
  if (group->id.asset_data) {
    if (group->id.asset_data->description) {
      return group->id.asset_data->description;
    }
  }
  if (!group->description) {
    return "";
  }
  return group->description;
}

bool blender::bke::node_group_poll(const bNodeTree *nodetree,
                                   const bNodeTree *grouptree,
                                   const char **r_disabled_hint)
{
  /* unspecified node group, generally allowed
   * (if anything, should be avoided on operator level)
   */
  if (grouptree == nullptr) {
    return true;
  }

  if (nodetree == grouptree) {
    if (r_disabled_hint) {
      *r_disabled_hint = RPT_("Nesting a node group inside of itself is not allowed");
    }
    return false;
  }
  if (nodetree->type != grouptree->type) {
    if (r_disabled_hint) {
      *r_disabled_hint = RPT_("Node group has different type");
    }
    return false;
  }

  for (const bNode *node : grouptree->all_nodes()) {
    if (node->typeinfo->poll_instance &&
        !node->typeinfo->poll_instance(node, nodetree, r_disabled_hint))
    {
      return false;
    }
  }
  return true;
}

namespace blender::nodes {

static std::function<ID *(const bNode &node)> get_default_id_getter(
    const bNodeTreeInterface &tree_interface, const bNodeTreeInterfaceSocket &io_socket)
{
  const int item_index = tree_interface.find_item_index(io_socket.item);
  BLI_assert(item_index >= 0);

  /* Avoid capturing pointers that can become dangling. */
  return [item_index](const bNode &node) -> ID * {
    if (node.id == nullptr) {
      return nullptr;
    }
    if (GS(node.id->name) != ID_NT) {
      return nullptr;
    }
    const bNodeTree &ntree = *reinterpret_cast<const bNodeTree *>(node.id);
    const bNodeTreeInterfaceItem *io_item = ntree.tree_interface.get_item_at_index(item_index);
    if (io_item == nullptr || io_item->item_type != NODE_INTERFACE_SOCKET) {
      return nullptr;
    }
    const bNodeTreeInterfaceSocket &io_socket =
        node_interface::get_item_as<bNodeTreeInterfaceSocket>(*io_item);
    return *static_cast<ID **>(io_socket.socket_data);
  };
}

static std::function<void(bNode &node, bNodeSocket &socket, const char *data_path)>
get_init_socket_fn(const bNodeTreeInterface &interface, const bNodeTreeInterfaceSocket &io_socket)
{
  const int item_index = interface.find_item_index(io_socket.item);
  BLI_assert(item_index >= 0);

  /* Avoid capturing pointers that can become dangling. */
  return [item_index](bNode &node, bNodeSocket &socket, const char *data_path) {
    if (node.id == nullptr) {
      return;
    }
    if (GS(node.id->name) != ID_NT) {
      return;
    }
    bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(node.id);
    const bNodeTreeInterfaceItem *io_item = ntree.tree_interface.get_item_at_index(item_index);
    if (io_item == nullptr || io_item->item_type != NODE_INTERFACE_SOCKET) {
      return;
    }
    const bNodeTreeInterfaceSocket &io_socket =
        node_interface::get_item_as<bNodeTreeInterfaceSocket>(*io_item);
    blender::bke::bNodeSocketType *typeinfo = io_socket.socket_typeinfo();
    if (typeinfo && typeinfo->interface_init_socket) {
      typeinfo->interface_init_socket(&ntree.id, &io_socket, &node, &socket, data_path);
    }
  };
}

static BaseSocketDeclarationBuilder &build_interface_socket_declaration(
    const bNodeTree &tree,
    const bNodeTreeInterfaceSocket &io_socket,
    const eNodeSocketInOut in_out,
    DeclarationListBuilder &b)
{
  blender::bke::bNodeSocketType *base_typeinfo = blender::bke::node_socket_type_find(
      io_socket.socket_type);
  eNodeSocketDatatype datatype = SOCK_CUSTOM;

  const StringRef name = io_socket.name;
  const StringRef identifier = io_socket.identifier;

  BaseSocketDeclarationBuilder *decl = nullptr;
  if (base_typeinfo) {
    datatype = eNodeSocketDatatype(base_typeinfo->type);
    switch (datatype) {
      case SOCK_FLOAT: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueFloat>(io_socket);
        decl = &b.add_socket<decl::Float>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_VECTOR: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueVector>(io_socket);
        decl = &b.add_socket<decl::Vector>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_RGBA: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRGBA>(io_socket);
        decl = &b.add_socket<decl::Color>(name, identifier, in_out).default_value(value.value);
        break;
      }
      case SOCK_SHADER: {
        decl = &b.add_socket<decl::Shader>(name, identifier, in_out);
        break;
      }
      case SOCK_BOOLEAN: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueBoolean>(io_socket);
        decl = &b.add_socket<decl::Bool>(name, identifier, in_out).default_value(value.value);
        break;
      }
      case SOCK_ROTATION: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRotation>(
            io_socket);
        decl = &b.add_socket<decl::Rotation>(name, identifier, in_out)
                    .default_value(math::EulerXYZ(float3(value.value_euler)));
        break;
      }
      case SOCK_MATRIX: {
        decl = &b.add_socket<decl::Matrix>(name, identifier, in_out);
        break;
      }
      case SOCK_INT: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueInt>(io_socket);
        decl = &b.add_socket<decl::Int>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_STRING: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueString>(io_socket);
        decl = &b.add_socket<decl::String>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value);
        break;
      }
      case SOCK_MENU: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueMenu>(io_socket);
        decl = &b.add_socket<decl::Menu>(name, identifier, in_out).default_value(value.value);
        break;
      }
      case SOCK_OBJECT: {
        decl = &b.add_socket<decl::Object>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_IMAGE: {
        decl = &b.add_socket<decl::Image>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_GEOMETRY:
        decl = &b.add_socket<decl::Geometry>(name, identifier, in_out);
        break;
      case SOCK_COLLECTION: {
        decl = &b.add_socket<decl::Collection>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_TEXTURE: {
        decl = &b.add_socket<decl::Texture>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_MATERIAL: {
        decl = &b.add_socket<decl::Material>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_CUSTOM: {
        decl = &b.add_socket<decl::Custom>(name, identifier, in_out)
                    .idname(io_socket.socket_type)
                    .init_socket_fn(get_init_socket_fn(tree.tree_interface, io_socket));
        break;
      }
    }
  }
  else {
    decl = &b.add_socket<decl::Custom>(name, identifier, in_out)
                .idname(io_socket.socket_type)
                .init_socket_fn(get_init_socket_fn(tree.tree_interface, io_socket));
  }
  decl->description(io_socket.description ? io_socket.description : "");
  decl->hide_value(io_socket.flag & NODE_INTERFACE_SOCKET_HIDE_VALUE);
  decl->compact(io_socket.flag & NODE_INTERFACE_SOCKET_COMPACT);
  return *decl;
}

static void set_default_input_field(const bNodeTreeInterfaceSocket &input, SocketDeclaration &decl)
{
  if (decl.socket_type == SOCK_VECTOR) {
    if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_NORMAL_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::normal);
      decl.hide_value = true;
    }
    else if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_POSITION_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::position);
      decl.hide_value = true;
    }
  }
  else if (decl.socket_type == SOCK_INT) {
    if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_INDEX_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::index);
      decl.hide_value = true;
    }
    else if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_ID_INDEX_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::id_or_index);
      decl.hide_value = true;
    }
  }
  else if (decl.socket_type == SOCK_MATRIX) {
    decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
        implicit_field_inputs::instance_transform);
    decl.hide_value = true;
  }
}

static void node_group_declare_panel_recursive(DeclarationListBuilder &b,
                                               const bNodeTree &group,
                                               const bNodeTreeInterfacePanel &io_parent_panel,
                                               const bool is_root)
{
  bool layout_added = false;
  auto add_layout_if_needed = [&]() {
    if (is_root && !layout_added) {
      b.add_default_layout();
      layout_added = true;
    }
  };

  for (const bNodeTreeInterfaceItem *item : io_parent_panel.items()) {
    switch (item->item_type) {
      case NODE_INTERFACE_SOCKET: {
        const auto &io_socket = node_interface::get_item_as<bNodeTreeInterfaceSocket>(*item);
        const eNodeSocketInOut in_out = (io_socket.flag & NODE_INTERFACE_SOCKET_INPUT) ? SOCK_IN :
                                                                                         SOCK_OUT;
        if (in_out == SOCK_IN) {
          add_layout_if_needed();
        }
        build_interface_socket_declaration(group, io_socket, in_out, b);
        break;
      }
      case NODE_INTERFACE_PANEL: {
        add_layout_if_needed();
        const auto &io_panel = node_interface::get_item_as<bNodeTreeInterfacePanel>(*item);
        auto &panel_b = b.add_panel(StringRef(io_panel.name), io_panel.identifier)
                            .description(StringRef(io_panel.description))
                            .default_closed(io_panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED);
        node_group_declare_panel_recursive(panel_b, group, io_panel, false);
        break;
      }
    }
  }

  add_layout_if_needed();
}

void node_group_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }
  NodeDeclaration &r_declaration = b.declaration();
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
  if (!group) {
    return;
  }
  if (ID_IS_LINKED(&group->id) && (group->id.tag & ID_TAG_MISSING)) {
    r_declaration.skip_updating_sockets = true;
    return;
  }
  r_declaration.skip_updating_sockets = false;

  /* Allow the node group interface to define the socket order. */
  r_declaration.use_custom_socket_order = true;

  node_group_declare_panel_recursive(b, *group, group->tree_interface.root_panel, true);

  if (group->type == NTREE_GEOMETRY) {
    group->ensure_interface_cache();
    const Span<const bNodeTreeInterfaceSocket *> inputs = group->interface_inputs();
    const FieldInferencingInterface &field_interface =
        *group->runtime->field_inferencing_interface;
    for (const int i : inputs.index_range()) {
      SocketDeclaration &decl = *r_declaration.inputs[i];
      decl.input_field_type = field_interface.inputs[i];
      set_default_input_field(*inputs[i], decl);
    }

    for (const int i : r_declaration.outputs.index_range()) {
      r_declaration.outputs[i]->output_field_dependency = field_interface.outputs[i];
    }
  }
}

}  // namespace blender::nodes

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Frame
 * \{ */

static void node_frame_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeFrame *data = MEM_cnew<NodeFrame>("frame node storage");
  node->storage = data;

  data->flag |= NODE_FRAME_SHRINK;

  data->label_size = 20;
}

void register_node_type_frame()
{
  /* frame type is used for all tree types, needs dynamic allocation */
  blender::bke::bNodeType *ntype = MEM_cnew<blender::bke::bNodeType>("frame node type");
  ntype->free_self = (void (*)(blender::bke::bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_FRAME, "Frame", NODE_CLASS_LAYOUT);
  ntype->initfunc = node_frame_init;
  blender::bke::node_type_storage(
      ntype, "NodeFrame", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_type_size(ntype, 150, 100, 0);
  ntype->flag |= NODE_BACKGROUND;

  blender::bke::node_register_type(ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Re-Route
 * \{ */

static void node_reroute_declare(blender::nodes::NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const blender::StringRefNull socket_idname(
      static_cast<const NodeReroute *>(node->storage)->type_idname);
  b.add_input<blender::nodes::decl::Custom>("Input").idname(socket_idname.c_str());
  b.add_output<blender::nodes::decl::Custom>("Output").idname(socket_idname.c_str());
}

static void node_reroute_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeReroute *data = MEM_cnew<NodeReroute>(__func__);
  STRNCPY(data->type_idname, "NodeSocketColor");
  node->storage = data;
}

void register_node_type_reroute()
{
  /* frame type is used for all tree types, needs dynamic allocation */
  blender::bke::bNodeType *ntype = MEM_cnew<blender::bke::bNodeType>("frame node type");
  ntype->free_self = (void (*)(blender::bke::bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_REROUTE, "Reroute", NODE_CLASS_LAYOUT);
  ntype->declare = node_reroute_declare;
  ntype->initfunc = node_reroute_init;
  node_type_storage(ntype, "NodeReroute", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}

static void propagate_reroute_type_from_start_socket(
    bNodeSocket *start_socket,
    const MultiValueMap<bNodeSocket *, bNodeLink *> &links_map,
    Map<bNode *, const blender::bke::bNodeSocketType *> &r_reroute_types)
{
  Stack<bNode *> nodes_to_check;
  for (bNodeLink *link : links_map.lookup(start_socket)) {
    if (link->tonode->type == NODE_REROUTE) {
      nodes_to_check.push(link->tonode);
    }
    if (link->fromnode->type == NODE_REROUTE) {
      nodes_to_check.push(link->fromnode);
    }
  }
  const blender::bke::bNodeSocketType *current_type = start_socket->typeinfo;
  while (!nodes_to_check.is_empty()) {
    bNode *reroute_node = nodes_to_check.pop();
    BLI_assert(reroute_node->type == NODE_REROUTE);
    if (r_reroute_types.add(reroute_node, current_type)) {
      for (bNodeLink *link : links_map.lookup((bNodeSocket *)reroute_node->inputs.first)) {
        if (link->fromnode->type == NODE_REROUTE) {
          nodes_to_check.push(link->fromnode);
        }
      }
      for (bNodeLink *link : links_map.lookup((bNodeSocket *)reroute_node->outputs.first)) {
        if (link->tonode->type == NODE_REROUTE) {
          nodes_to_check.push(link->tonode);
        }
      }
    }
  }
}

void ntree_update_reroute_nodes(bNodeTree *ntree)
{
  /* Contains nodes that are linked to at least one reroute node. */
  Set<bNode *> nodes_linked_with_reroutes;
  /* Contains all links that are linked to at least one reroute node. */
  MultiValueMap<bNodeSocket *, bNodeLink *> links_map;
  /* Build acceleration data structures for the algorithm below. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == nullptr || link->tosock == nullptr) {
      continue;
    }
    if (link->fromnode->type != NODE_REROUTE && link->tonode->type != NODE_REROUTE) {
      continue;
    }
    if (link->fromnode->type != NODE_REROUTE) {
      nodes_linked_with_reroutes.add(link->fromnode);
    }
    if (link->tonode->type != NODE_REROUTE) {
      nodes_linked_with_reroutes.add(link->tonode);
    }
    links_map.add(link->fromsock, link);
    links_map.add(link->tosock, link);
  }

  /* Will contain the socket type for every linked reroute node. */
  Map<bNode *, const blender::bke::bNodeSocketType *> reroute_types;

  /* Propagate socket types from left to right. */
  for (bNode *start_node : nodes_linked_with_reroutes) {
    LISTBASE_FOREACH (bNodeSocket *, output_socket, &start_node->outputs) {
      propagate_reroute_type_from_start_socket(output_socket, links_map, reroute_types);
    }
  }

  /* Propagate socket types from right to left. This affects reroute nodes that haven't been
   * changed in the loop above. */
  for (bNode *start_node : nodes_linked_with_reroutes) {
    LISTBASE_FOREACH (bNodeSocket *, input_socket, &start_node->inputs) {
      propagate_reroute_type_from_start_socket(input_socket, links_map, reroute_types);
    }
  }

  /* Actually update reroute nodes with changed types. */
  for (const auto item : reroute_types.items()) {
    bNode *reroute_node = item.key;
    const blender::bke::bNodeSocketType *socket_type = item.value;
    NodeReroute *storage = static_cast<NodeReroute *>(reroute_node->storage);
    STRNCPY(storage->type_idname, socket_type->idname);
    blender::nodes::update_node_declaration_and_sockets(*ntree, *reroute_node);
  }
}

bool blender::bke::node_is_connected_to_output(const bNodeTree *ntree, const bNode *node)
{
  ntree->ensure_topology_cache();
  Stack<const bNode *> nodes_to_check;
  for (const bNodeSocket *socket : node->output_sockets()) {
    for (const bNodeLink *link : socket->directly_linked_links()) {
      nodes_to_check.push(link->tonode);
    }
  }
  while (!nodes_to_check.is_empty()) {
    const bNode *next_node = nodes_to_check.pop();
    for (const bNodeSocket *socket : next_node->output_sockets()) {
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (link->tonode->typeinfo->nclass == NODE_CLASS_OUTPUT &&
            link->tonode->flag & NODE_DO_OUTPUT)
        {
          return true;
        }
        nodes_to_check.push(link->tonode);
      }
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node #GROUP_INPUT / #GROUP_OUTPUT
 * \{ */

bNodeSocket *node_group_input_find_socket(bNode *node, const char *identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

namespace blender::nodes {

static void group_input_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *node_tree = b.tree_or_null();
  if (node_tree == nullptr) {
    return;
  }
  node_tree->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          build_interface_socket_declaration(*node_tree, socket, SOCK_OUT, b);
        }
        break;
      }
    }
    return true;
  });
  b.add_output<decl::Extend>("", "__extend__");
}

static void group_output_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *node_tree = b.tree_or_null();
  if (node_tree == nullptr) {
    return;
  }
  node_tree->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          build_interface_socket_declaration(*node_tree, socket, SOCK_IN, b);
        }
        break;
      }
    }
    return true;
  });
  b.add_input<decl::Extend>("", "__extend__");
}

static bool group_input_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  BLI_assert(link->tonode != node);
  BLI_assert(link->tosock->in_out == SOCK_IN);
  if (!StringRef(link->fromsock->identifier).startswith("__extend__")) {
    return true;
  }
  if (StringRef(link->tosock->identifier).startswith("__extend__")) {
    /* Don't connect to other "extend" sockets. */
    return false;
  }
  const bNodeTreeInterfaceSocket *io_socket = node_interface::add_interface_socket_from_node(
      *ntree, *link->tonode, *link->tosock);
  if (!io_socket) {
    return false;
  }
  update_node_declaration_and_sockets(*ntree, *node);
  link->fromsock = node_group_input_find_socket(node, io_socket->identifier);
  return true;
}

static bool group_output_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  BLI_assert(link->fromnode != node);
  BLI_assert(link->fromsock->in_out == SOCK_OUT);
  if (!StringRef(link->tosock->identifier).startswith("__extend__")) {
    return true;
  }
  if (StringRef(link->fromsock->identifier).startswith("__extend__")) {
    /* Don't connect to other "extend" sockets. */
    return false;
  }
  const bNodeTreeInterfaceSocket *io_socket = node_interface::add_interface_socket_from_node(
      *ntree, *link->fromnode, *link->fromsock);
  if (!io_socket) {
    return false;
  }
  update_node_declaration_and_sockets(*ntree, *node);
  link->tosock = node_group_output_find_socket(node, io_socket->identifier);
  return true;
}

}  // namespace blender::nodes

void register_node_type_group_input()
{
  /* used for all tree types, needs dynamic allocation */
  blender::bke::bNodeType *ntype = MEM_cnew<blender::bke::bNodeType>("node type");
  ntype->free_self = (void (*)(blender::bke::bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_GROUP_INPUT, "Group Input", NODE_CLASS_INTERFACE);
  blender::bke::node_type_size(ntype, 140, 80, 400);
  ntype->declare = blender::nodes::group_input_declare;
  ntype->insert_link = blender::nodes::group_input_insert_link;

  blender::bke::node_register_type(ntype);
}

bNodeSocket *node_group_output_find_socket(bNode *node, const char *identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

void register_node_type_group_output()
{
  /* used for all tree types, needs dynamic allocation */
  blender::bke::bNodeType *ntype = MEM_cnew<blender::bke::bNodeType>("node type");
  ntype->free_self = (void (*)(blender::bke::bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_GROUP_OUTPUT, "Group Output", NODE_CLASS_INTERFACE);
  blender::bke::node_type_size(ntype, 140, 80, 400);
  ntype->declare = blender::nodes::group_output_declare;
  ntype->insert_link = blender::nodes::group_output_insert_link;

  ntype->no_muting = true;

  blender::bke::node_register_type(ntype);
}

/** \} */
