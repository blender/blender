/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_set.hh"

#include "BKE_node.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes {

void GatherLinkSearchOpParams::add_item(std::string socket_name,
                                        SocketLinkOperation::LinkSocketFn fn,
                                        const int weight)
{

  std::string name = std::string(IFACE_(node_type_.ui_name)) + " " + UI_MENU_ARROW_SEP +
                     socket_name;

  items_.append({std::move(name), std::move(fn), weight});
}

const bNodeSocket &GatherLinkSearchOpParams::other_socket() const
{
  return other_socket_;
}

const bNodeTree &GatherLinkSearchOpParams::node_tree() const
{
  return node_tree_;
}

const bNodeType &GatherLinkSearchOpParams::node_type() const
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
  nodeAddLink(&node_tree, &new_node, new_node_socket, &node, &socket);
}

bNode &LinkSearchOpParams::add_node(StringRef idname)
{
  std::string idname_str = idname;
  bNode *node = nodeAddNode(&C, &node_tree, idname_str.c_str());
  BLI_assert(node != nullptr);
  added_nodes_.append(node);
  return *node;
}

bNode &LinkSearchOpParams::add_node(const bNodeType &node_type)
{
  return this->add_node(node_type.idname);
}

void LinkSearchOpParams::update_and_connect_available_socket(bNode &new_node,
                                                             StringRef socket_name)
{
  if (new_node.typeinfo->updatefunc) {
    new_node.typeinfo->updatefunc(&node_tree, &new_node);
  }
  this->connect_available_socket(new_node, socket_name);
}

void search_link_ops_for_declarations(GatherLinkSearchOpParams &params,
                                      Span<SocketDeclarationPtr> declarations)
{
  const bNodeType &node_type = params.node_type();

  const SocketDeclaration *main_socket = nullptr;
  Vector<const SocketDeclaration *> connectable_sockets;

  Set<StringRef> socket_names;
  for (const int i : declarations.index_range()) {
    const SocketDeclaration &socket = *declarations[i];
    if (!socket_names.add(socket.name())) {
      /* Don't add sockets with the same name to the search. Needed to support being called from
       * #search_link_ops_for_basic_node, which should have "okay" behavior for nodes with
       * duplicate socket names. */
      continue;
    }
    if (!socket.can_connect(params.other_socket())) {
      continue;
    }
    if (socket.is_default_link_socket() || main_socket == nullptr) {
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
        IFACE_(socket.name().c_str()),
        [&node_type, &socket](LinkSearchOpParams &params) {
          bNode &node = params.add_node(node_type);
          socket.make_available(node);
          params.update_and_connect_available_socket(node, socket.name());
        },
        weight);
  }
}

static void search_link_ops_for_socket_templates(GatherLinkSearchOpParams &params,
                                                 const bNodeSocketTemplate *templates,
                                                 const eNodeSocketInOut in_out)
{
  const bNodeType &node_type = params.node_type();
  const bNodeTreeType &node_tree_type = *params.node_tree().typeinfo;

  Set<StringRef> socket_names;
  for (const bNodeSocketTemplate *socket_template = templates; socket_template->type != -1;
       socket_template++) {
    eNodeSocketDatatype from = (eNodeSocketDatatype)socket_template->type;
    eNodeSocketDatatype to = (eNodeSocketDatatype)params.other_socket().type;
    if (in_out == SOCK_IN) {
      std::swap(from, to);
    }
    if (node_tree_type.validate_link && !node_tree_type.validate_link(from, to)) {
      continue;
    }
    if (!socket_names.add(socket_template->name)) {
      /* See comment in #search_link_ops_for_declarations. */
      continue;
    }

    params.add_item(
        socket_template->name, [socket_template, node_type, in_out](LinkSearchOpParams &params) {
          bNode &node = params.add_node(node_type);
          bNodeSocket *new_node_socket = bke::node_find_enabled_socket(
              node, in_out, socket_template->name);
          if (new_node_socket != nullptr) {
            /* Rely on the way #nodeAddLink switches in/out if necessary. */
            nodeAddLink(&params.node_tree, &params.node, &params.socket, &node, new_node_socket);
          }
        });
  }
}

void search_link_ops_for_basic_node(GatherLinkSearchOpParams &params)
{
  const bNodeType &node_type = params.node_type();

  if (node_type.declare) {
    if (node_type.declaration_is_dynamic) {
      /* Dynamic declarations (whatever they end up being) aren't supported
       * by this function, but still avoid a crash in release builds. */
      BLI_assert_unreachable();
      return;
    }

    const NodeDeclaration &declaration = *node_type.fixed_declaration;

    search_link_ops_for_declarations(params, declaration.sockets(params.in_out()));
  }
  else if (node_type.inputs && params.in_out() == SOCK_IN) {
    search_link_ops_for_socket_templates(params, node_type.inputs, SOCK_IN);
  }
  else if (node_type.outputs && params.in_out() == SOCK_OUT) {
    search_link_ops_for_socket_templates(params, node_type.outputs, SOCK_OUT);
  }
}

}  // namespace blender::nodes
