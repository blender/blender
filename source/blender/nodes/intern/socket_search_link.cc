/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_set.hh"

#include "BKE_context.hh"
#include "BKE_node.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes {

void GatherLinkSearchOpParams::add_item(std::string socket_name,
                                        SocketLinkOperation::LinkSocketFn fn,
                                        const int weight)
{
  std::string name = fmt::format("{}{} " UI_MENU_ARROW_SEP " {}",
                                 IFACE_(node_type_.ui_name),
                                 node_type_.deprecation_notice ? IFACE_(" (Deprecated)") : "",
                                 socket_name);
  this->add_item_full_name(std::move(name), std::move(fn), weight);
}

void GatherLinkSearchOpParams::add_item_full_name(std::string name,
                                                  SocketLinkOperation::LinkSocketFn fn,
                                                  int weight)
{
  items_.append({std::move(name), std::move(fn), weight});
}

const bNodeSocket &GatherLinkSearchOpParams::other_socket() const
{
  return other_socket_;
}

const SpaceNode &GatherLinkSearchOpParams::space_node() const
{
  return snode_;
}

const bNodeTree &GatherLinkSearchOpParams::node_tree() const
{
  return node_tree_;
}

const bke::bNodeType &GatherLinkSearchOpParams::node_type() const
{
  return node_type_;
}

eNodeSocketInOut GatherLinkSearchOpParams::in_out() const
{
  return other_socket_.in_out == SOCK_IN ? SOCK_OUT : SOCK_IN;
}

void LinkSearchOpParams::connect_available_socket(bNode &new_node, StringRef socket_name)
{
  const eNodeSocketInOut in_out = socket.in_out == SOCK_IN ? SOCK_OUT : SOCK_IN;
  bNodeSocket *new_node_socket = bke::node_find_enabled_socket(new_node, in_out, socket_name);
  if (new_node_socket == nullptr) {
    /* If the socket isn't found, some node's search gather functions probably aren't configured
     * properly. It's likely enough that it's worth avoiding a crash in a release build though. */
    BLI_assert_unreachable();
    return;
  }
  this->connect_socket(new_node, *new_node_socket);
}

void LinkSearchOpParams::connect_available_socket_by_identifier(bNode &new_node,
                                                                const StringRef socket_identifier)
{
  const eNodeSocketInOut in_out = this->socket.in_out == SOCK_IN ? SOCK_OUT : SOCK_IN;
  bNodeSocket *new_node_socket = bke::node_find_socket(new_node, in_out, socket_identifier);
  BLI_assert(new_node_socket);
  this->connect_socket(new_node, *new_node_socket);
}

void LinkSearchOpParams::connect_socket(bNode &new_node, bNodeSocket &new_socket)
{
  bke::node_add_link(this->node_tree, new_node, new_socket, this->node, this->socket);
  if (new_socket.in_out == SOCK_OUT) {
    /* If the old socket already contained a value, then transfer it to a new one, from
     * which this value will get there. */
    bke::node_socket_move_default_value(
        *CTX_data_main(&C), this->node_tree, this->socket, new_socket);
  }
}

bNode &LinkSearchOpParams::add_node(StringRef idname)
{
  bNode *node = bke::node_add_node(&C, node_tree, idname);
  BLI_assert(node != nullptr);
  added_nodes_.append(node);
  return *node;
}

bNode &LinkSearchOpParams::add_node(const bke::bNodeType &node_type)
{
  return this->add_node(node_type.idname);
}

void LinkSearchOpParams::update_and_connect_available_socket(bNode &new_node,
                                                             StringRef socket_name)
{
  update_node_declaration_and_sockets(this->node_tree, new_node);
  if (new_node.typeinfo->updatefunc) {
    new_node.typeinfo->updatefunc(&node_tree, &new_node);
  }
  this->connect_available_socket(new_node, socket_name);
}

void search_link_ops_for_declarations(GatherLinkSearchOpParams &params,
                                      Span<SocketDeclaration *> declarations)
{
  const bke::bNodeType &node_type = params.node_type();

  const SocketDeclaration *main_socket = nullptr;
  Vector<const SocketDeclaration *> connectable_sockets;

  Set<StringRef> socket_names;
  for (const int i : declarations.index_range()) {
    const SocketDeclaration &socket = *declarations[i];
    if (!socket_names.add(socket.name)) {
      /* Don't add sockets with the same name to the search. Needed to support being called from
       * #search_link_ops_for_basic_node, which should have "okay" behavior for nodes with
       * duplicate socket names. */
      continue;
    }
    if (!socket.can_connect(params.other_socket())) {
      continue;
    }
    if (socket.is_default_link_socket || main_socket == nullptr) {
      /* Either the first connectable or explicitly tagged socket is the main socket. */
      main_socket = &socket;
    }
    connectable_sockets.append(&socket);
  }
  for (const int i : connectable_sockets.index_range()) {
    const SocketDeclaration &socket = *connectable_sockets[i];
    /* Give non-main sockets a lower weight so that they don't show up at the top of the search
     * when they are not explicitly searched for. The -1 is used to make sure that the first socket
     * has a smaller weight than zero so that it does not have the same weight as the main socket.
     * Negative weights are used to avoid making the highest weight dependent on the number of
     * sockets. */
    const int weight = (&socket == main_socket) ? 0 : -1 - i;
    params.add_item(
        IFACE_(socket.name),
        [&node_type, &socket](LinkSearchOpParams &params) {
          bNode &node = params.add_node(node_type);
          socket.make_available(node);
          params.update_and_connect_available_socket(node, socket.name);
        },
        weight);
  }
}

void search_link_ops_for_basic_node(GatherLinkSearchOpParams &params)
{
  const bke::bNodeType &node_type = params.node_type();
  if (!node_type.static_declaration) {
    return;
  }
  const NodeDeclaration &declaration = *node_type.static_declaration;
  search_link_ops_for_declarations(params, declaration.sockets(params.in_out()));
}

}  // namespace blender::nodes
