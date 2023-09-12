/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstddef>
#include <cstring>

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_euler.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"
#include "BKE_node_tree_update.h"

#include "RNA_types.hh"

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

bool node_group_poll_instance(const bNode *node,
                              const bNodeTree *nodetree,
                              const char **disabled_hint)
{
  if (!node->typeinfo->poll(node->typeinfo, nodetree, disabled_hint)) {
    return false;
  }
  const bNodeTree *grouptree = reinterpret_cast<const bNodeTree *>(node->id);
  if (!grouptree) {
    return true;
  }
  return nodeGroupPoll(nodetree, grouptree, disabled_hint);
}

bool nodeGroupPoll(const bNodeTree *nodetree,
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
      *r_disabled_hint = TIP_("Nesting a node group inside of itself is not allowed");
    }
    return false;
  }
  if (nodetree->type != grouptree->type) {
    if (r_disabled_hint) {
      *r_disabled_hint = TIP_("Node group has different type");
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
    bNodeSocketType *typeinfo = io_socket.socket_typeinfo();
    if (typeinfo && typeinfo->interface_init_socket) {
      typeinfo->interface_init_socket(&ntree.id, &io_socket, &node, &socket, data_path);
    }
  };
}

/* in_out overrides the socket declaration in/out type (bNodeTreeInterfaceSocket::flag)
 * because a node group input is turned into an output socket for group input nodes. */
static SocketDeclarationPtr declaration_for_interface_socket(
    const bNodeTree &ntree,
    const bNodeTreeInterfaceSocket &io_socket,
    const eNodeSocketInOut in_out)
{
  SocketDeclarationPtr dst;

  bNodeSocketType *base_typeinfo = nodeSocketTypeFind(io_socket.socket_type);
  if (base_typeinfo == nullptr) {
    return dst;
  }

  eNodeSocketDatatype datatype = eNodeSocketDatatype(base_typeinfo->type);

  switch (datatype) {
    case SOCK_FLOAT: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueFloat>(io_socket);
      std::unique_ptr<decl::Float> decl = std::make_unique<decl::Float>();
      decl->subtype = PropertySubType(value.subtype);
      decl->default_value = value.value;
      decl->soft_min_value = value.min;
      decl->soft_max_value = value.max;
      dst = std::move(decl);
      break;
    }
    case SOCK_VECTOR: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueVector>(io_socket);
      std::unique_ptr<decl::Vector> decl = std::make_unique<decl::Vector>();
      decl->subtype = PropertySubType(value.subtype);
      decl->default_value = value.value;
      decl->soft_min_value = value.min;
      decl->soft_max_value = value.max;
      dst = std::move(decl);
      break;
    }
    case SOCK_RGBA: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRGBA>(io_socket);
      std::unique_ptr<decl::Color> decl = std::make_unique<decl::Color>();
      decl->default_value = value.value;
      dst = std::move(decl);
      break;
    }
    case SOCK_SHADER: {
      std::unique_ptr<decl::Shader> decl = std::make_unique<decl::Shader>();
      dst = std::move(decl);
      break;
    }
    case SOCK_BOOLEAN: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueBoolean>(io_socket);
      std::unique_ptr<decl::Bool> decl = std::make_unique<decl::Bool>();
      decl->default_value = value.value;
      dst = std::move(decl);
      break;
    }
    case SOCK_ROTATION: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRotation>(io_socket);
      std::unique_ptr<decl::Rotation> decl = std::make_unique<decl::Rotation>();
      decl->default_value = math::EulerXYZ(float3(value.value_euler));
      dst = std::move(decl);
      break;
    }
    case SOCK_INT: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueInt>(io_socket);
      std::unique_ptr<decl::Int> decl = std::make_unique<decl::Int>();
      decl->subtype = PropertySubType(value.subtype);
      decl->default_value = value.value;
      decl->soft_min_value = value.min;
      decl->soft_max_value = value.max;
      dst = std::move(decl);
      break;
    }
    case SOCK_STRING: {
      const auto &value = node_interface::get_socket_data_as<bNodeSocketValueString>(io_socket);
      std::unique_ptr<decl::String> decl = std::make_unique<decl::String>();
      decl->default_value = value.value;
      dst = std::move(decl);
      break;
    }
    case SOCK_OBJECT: {
      auto value = std::make_unique<decl::Object>();
      value->default_value_fn = get_default_id_getter(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
    }
    case SOCK_IMAGE: {
      auto value = std::make_unique<decl::Image>();
      value->default_value_fn = get_default_id_getter(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
    }
    case SOCK_GEOMETRY:
      dst = std::make_unique<decl::Geometry>();
      break;
    case SOCK_COLLECTION: {
      auto value = std::make_unique<decl::Collection>();
      value->default_value_fn = get_default_id_getter(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
    }
    case SOCK_TEXTURE: {
      auto value = std::make_unique<decl::Texture>();
      value->default_value_fn = get_default_id_getter(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
    }
    case SOCK_MATERIAL: {
      auto value = std::make_unique<decl::Material>();
      value->default_value_fn = get_default_id_getter(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
    }
    case SOCK_CUSTOM:
      auto value = std::make_unique<decl::Custom>();
      value->init_socket_fn = get_init_socket_fn(ntree.tree_interface, io_socket);
      dst = std::move(value);
      break;
  }
  dst->name = io_socket.name;
  dst->identifier = io_socket.identifier;
  dst->in_out = in_out;
  dst->description = io_socket.description ? io_socket.description : "";
  dst->hide_value = io_socket.flag & NODE_INTERFACE_SOCKET_HIDE_VALUE;
  dst->compact = io_socket.flag & NODE_INTERFACE_SOCKET_COMPACT;
  return dst;
}

/* Socket items can be both input and output, generating 2 declarations for 1 item. Count the
 * actual declarations generated by panel content to get the true size for UI drawing. */
static int count_panel_declaration_children(const bNodeTreeInterfacePanel &io_panel)
{
  int num_child_decls = 0;
  io_panel.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_PANEL:
        num_child_decls++;
        break;
      case NODE_INTERFACE_SOCKET:
        const bNodeTreeInterfaceSocket &socket =
            reinterpret_cast<const bNodeTreeInterfaceSocket &>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          num_child_decls++;
        }
        if (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          num_child_decls++;
        }
        break;
    }
    return true;
  });
  return num_child_decls;
}

static PanelDeclarationPtr declaration_for_interface_panel(const bNodeTree & /*ntree*/,
                                                           const bNodeTreeInterfacePanel &io_panel)
{
  if (io_panel.items_num == 0) {
    return nullptr;
  }

  PanelDeclarationPtr dst = std::make_unique<PanelDeclaration>();
  dst->identifier = io_panel.identifier;
  dst->name = io_panel.name ? io_panel.name : "";
  dst->description = io_panel.description ? io_panel.description : "";
  dst->default_collapsed = (io_panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED);
  dst->num_child_decls = count_panel_declaration_children(io_panel);
  return dst;
}

void node_group_declare_dynamic(const bNodeTree & /*node_tree*/,
                                const bNode &node,
                                NodeDeclaration &r_declaration)
{
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id);
  if (!group) {
    return;
  }
  if (ID_IS_LINKED(&group->id) && (group->id.tag & LIB_TAG_MISSING)) {
    r_declaration.skip_updating_sockets = true;
    return;
  }
  r_declaration.skip_updating_sockets = false;

  /* Allow the node group interface to define the socket order. */
  r_declaration.use_custom_socket_order = true;

  group->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          if (SocketDeclarationPtr socket_decl = declaration_for_interface_socket(
                  *group, socket, SOCK_OUT)) {
            r_declaration.outputs.append(socket_decl.get());
            r_declaration.items.append(std::move(socket_decl));
          }
        }
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          if (SocketDeclarationPtr socket_decl = declaration_for_interface_socket(
                  *group, socket, SOCK_IN)) {
            r_declaration.inputs.append(socket_decl.get());
            r_declaration.items.append(std::move(socket_decl));
          }
        }
        break;
      }
      case NODE_INTERFACE_PANEL: {
        const bNodeTreeInterfacePanel &panel =
            node_interface::get_item_as<bNodeTreeInterfacePanel>(item);
        if (PanelDeclarationPtr panel_decl = declaration_for_interface_panel(*group, panel)) {
          r_declaration.items.append(std::move(panel_decl));
        }
        break;
      }
    }
    return true;
  });
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
  bNodeType *ntype = MEM_cnew<bNodeType>("frame node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_FRAME, "Frame", NODE_CLASS_LAYOUT);
  ntype->initfunc = node_frame_init;
  node_type_storage(ntype, "NodeFrame", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_type_size(ntype, 150, 100, 0);
  ntype->flag |= NODE_BACKGROUND;

  nodeRegisterType(ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Re-Route
 * \{ */

static void node_reroute_init(bNodeTree *ntree, bNode *node)
{
  /* NOTE: Cannot use socket templates for this, since it would reset the socket type
   * on each file read via the template verification procedure.
   */
  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Input", "Input");
  nodeAddStaticSocket(ntree, node, SOCK_OUT, SOCK_RGBA, PROP_NONE, "Output", "Output");
}

void register_node_type_reroute()
{
  /* frame type is used for all tree types, needs dynamic allocation */
  bNodeType *ntype = MEM_cnew<bNodeType>("frame node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_REROUTE, "Reroute", NODE_CLASS_LAYOUT);
  ntype->initfunc = node_reroute_init;

  nodeRegisterType(ntype);
}

static void propagate_reroute_type_from_start_socket(
    bNodeSocket *start_socket,
    const MultiValueMap<bNodeSocket *, bNodeLink *> &links_map,
    Map<bNode *, const bNodeSocketType *> &r_reroute_types)
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
  const bNodeSocketType *current_type = start_socket->typeinfo;
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
  Map<bNode *, const bNodeSocketType *> reroute_types;

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
    const bNodeSocketType *socket_type = item.value;
    bNodeSocket *input_socket = (bNodeSocket *)reroute_node->inputs.first;
    bNodeSocket *output_socket = (bNodeSocket *)reroute_node->outputs.first;

    if (input_socket->typeinfo != socket_type) {
      blender::bke::nodeModifySocketType(ntree, reroute_node, input_socket, socket_type->idname);
    }
    if (output_socket->typeinfo != socket_type) {
      blender::bke::nodeModifySocketType(ntree, reroute_node, output_socket, socket_type->idname);
    }
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
            link->tonode->flag & NODE_DO_OUTPUT) {
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

static void group_input_declare_dynamic(const bNodeTree &node_tree,
                                        const bNode & /*node*/,
                                        NodeDeclaration &r_declaration)
{
  node_tree.tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          SocketDeclarationPtr socket_decl = declaration_for_interface_socket(
              node_tree, socket, SOCK_OUT);
          r_declaration.outputs.append(socket_decl.get());
          r_declaration.items.append(std::move(socket_decl));
        }
        break;
      }
    }
    return true;
  });
  SocketDeclarationPtr extend_decl = decl::create_extend_declaration(SOCK_OUT);
  r_declaration.outputs.append(extend_decl.get());
  r_declaration.items.append(std::move(extend_decl));
}

static void group_output_declare_dynamic(const bNodeTree &node_tree,
                                         const bNode & /*node*/,
                                         NodeDeclaration &r_declaration)
{
  node_tree.tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (item.item_type) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          SocketDeclarationPtr socket_decl = declaration_for_interface_socket(
              node_tree, socket, SOCK_IN);
          r_declaration.inputs.append(socket_decl.get());
          r_declaration.items.append(std::move(socket_decl));
        }
        break;
      }
    }
    return true;
  });
  SocketDeclarationPtr extend_decl = decl::create_extend_declaration(SOCK_IN);
  r_declaration.inputs.append(extend_decl.get());
  r_declaration.items.append(std::move(extend_decl));
}

static bool group_input_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  BLI_assert(link->tonode != node);
  BLI_assert(link->tosock->in_out == SOCK_IN);
  if (link->fromsock->identifier != StringRef("__extend__")) {
    return true;
  }
  if (link->tosock->identifier == StringRef("__extend__")) {
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
  if (link->tosock->identifier != StringRef("__extend__")) {
    return true;
  }
  if (link->fromsock->identifier == StringRef("__extend__")) {
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
  bNodeType *ntype = MEM_cnew<bNodeType>("node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_GROUP_INPUT, "Group Input", NODE_CLASS_INTERFACE);
  blender::bke::node_type_size(ntype, 140, 80, 400);
  ntype->declare_dynamic = blender::nodes::group_input_declare_dynamic;
  ntype->insert_link = blender::nodes::group_input_insert_link;

  nodeRegisterType(ntype);
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
  bNodeType *ntype = MEM_cnew<bNodeType>("node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  blender::bke::node_type_base(ntype, NODE_GROUP_OUTPUT, "Group Output", NODE_CLASS_INTERFACE);
  blender::bke::node_type_size(ntype, 140, 80, 400);
  ntype->declare_dynamic = blender::nodes::group_output_declare_dynamic;
  ntype->insert_link = blender::nodes::group_output_insert_link;

  ntype->no_muting = true;

  nodeRegisterType(ntype);
}

/** \} */
