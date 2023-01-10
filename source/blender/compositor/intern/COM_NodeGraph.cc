/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. */

#include <cstring>

#include "DNA_node_types.h"

#include "BKE_node.h"
#include "BKE_node_runtime.hh"

#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_SocketProxyNode.h"

#include "COM_NodeGraph.h" /* own include */

namespace blender::compositor {

/*******************
 **** NodeGraph ****
 *******************/

NodeGraph::~NodeGraph()
{
  while (nodes_.size()) {
    delete nodes_.pop_last();
  }
}

void NodeGraph::from_bNodeTree(const CompositorContext &context, bNodeTree *tree)
{
  add_bNodeTree(context, 0, tree, NODE_INSTANCE_KEY_BASE);
}

bNodeSocket *NodeGraph::find_b_node_input(bNode *b_node, const char *identifier)
{
  for (bNodeSocket *b_sock = (bNodeSocket *)b_node->inputs.first; b_sock; b_sock = b_sock->next) {
    if (STREQ(b_sock->identifier, identifier)) {
      return b_sock;
    }
  }
  return nullptr;
}

bNodeSocket *NodeGraph::find_b_node_output(bNode *b_node, const char *identifier)
{
  for (bNodeSocket *b_sock = (bNodeSocket *)b_node->outputs.first; b_sock; b_sock = b_sock->next) {
    if (STREQ(b_sock->identifier, identifier)) {
      return b_sock;
    }
  }
  return nullptr;
}

void NodeGraph::add_node(Node *node,
                         bNodeTree *b_ntree,
                         bNodeInstanceKey key,
                         bool is_active_group)
{
  node->set_bnodetree(b_ntree);
  node->set_instance_key(key);
  node->set_is_in_active_group(is_active_group);

  nodes_.append(node);

  DebugInfo::node_added(node);
}

void NodeGraph::add_link(NodeOutput *from_socket, NodeInput *to_socket)
{
  links_.append(Link(from_socket, to_socket));

  /* register with the input */
  to_socket->set_link(from_socket);
}

void NodeGraph::add_bNodeTree(const CompositorContext &context,
                              int nodes_start,
                              bNodeTree *tree,
                              bNodeInstanceKey parent_key)
{
  const bNodeTree *basetree = context.get_bnodetree();

  /* Update viewers in the active edit-tree as well the base tree (for backdrop). */
  bool is_active_group = (parent_key.value == basetree->active_viewer_key.value);

  /* add all nodes of the tree to the node list */
  for (bNode *node = (bNode *)tree->nodes.first; node; node = node->next) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, tree, node);
    add_bNode(context, tree, node, key, is_active_group);
  }

  NodeRange node_range(nodes_.begin() + nodes_start, nodes_.end());
  /* Add all node-links of the tree to the link list. */
  for (bNodeLink *nodelink = (bNodeLink *)tree->links.first; nodelink; nodelink = nodelink->next) {
    add_bNodeLink(node_range, nodelink);
  }
}

void NodeGraph::add_bNode(const CompositorContext &context,
                          bNodeTree *b_ntree,
                          bNode *b_node,
                          bNodeInstanceKey key,
                          bool is_active_group)
{
  /* replace muted nodes by proxies for internal links */
  if (b_node->flag & NODE_MUTED) {
    add_proxies_mute(b_ntree, b_node, key, is_active_group);
    return;
  }

  /* replace slow nodes with proxies for fast execution */
  if (context.is_fast_calculation() && !COM_bnode_is_fast_node(*b_node)) {
    add_proxies_skip(b_ntree, b_node, key, is_active_group);
    return;
  }

  /* special node types */
  if (ELEM(b_node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
    add_proxies_group(context, b_node, key);
  }
  else if (b_node->type == NODE_REROUTE) {
    add_proxies_reroute(b_ntree, b_node, key, is_active_group);
  }
  else {
    /* regular nodes, handled in Converter */
    Node *node = COM_convert_bnode(b_node);
    if (node) {
      add_node(node, b_ntree, key, is_active_group);
    }
  }
}

NodeOutput *NodeGraph::find_output(const NodeRange &node_range, bNodeSocket *b_socket)
{
  for (Vector<Node *>::iterator it = node_range.first; it != node_range.second; ++it) {
    Node *node = *it;
    for (NodeOutput *output : node->get_output_sockets()) {
      if (output->get_bnode_socket() == b_socket) {
        return output;
      }
    }
  }
  return nullptr;
}

void NodeGraph::add_bNodeLink(const NodeRange &node_range, bNodeLink *b_nodelink)
{
  /** \note Ignore invalid links. */
  if (!(b_nodelink->flag & NODE_LINK_VALID)) {
    return;
  }
  if ((b_nodelink->fromsock->flag & SOCK_UNAVAIL) || (b_nodelink->tosock->flag & SOCK_UNAVAIL) ||
      (b_nodelink->flag & NODE_LINK_MUTED)) {
    return;
  }

  /* NOTE: a DNA input socket can have multiple NodeInput in the compositor tree! (proxies)
   * The output then gets linked to each one of them.
   */

  NodeOutput *output = find_output(node_range, b_nodelink->fromsock);
  if (!output) {
    return;
  }

  for (Vector<Node *>::iterator it = node_range.first; it != node_range.second; ++it) {
    Node *node = *it;
    for (NodeInput *input : node->get_input_sockets()) {
      if (input->get_bnode_socket() == b_nodelink->tosock && !input->is_linked()) {
        add_link(output, input);
      }
    }
  }
}

/* **** Special proxy node type conversions **** */

void NodeGraph::add_proxies_mute(bNodeTree *b_ntree,
                                 bNode *b_node,
                                 bNodeInstanceKey key,
                                 bool is_active_group)
{
  for (const bNodeLink &b_link : b_node->internal_links()) {
    SocketProxyNode *proxy = new SocketProxyNode(b_node, b_link.fromsock, b_link.tosock, false);
    add_node(proxy, b_ntree, key, is_active_group);
  }
}

void NodeGraph::add_proxies_skip(bNodeTree *b_ntree,
                                 bNode *b_node,
                                 bNodeInstanceKey key,
                                 bool is_active_group)
{
  for (bNodeSocket *output = (bNodeSocket *)b_node->outputs.first; output; output = output->next) {
    bNodeSocket *input;

    /* look for first input with matching datatype for each output */
    for (input = (bNodeSocket *)b_node->inputs.first; input; input = input->next) {
      if (input->type == output->type) {
        break;
      }
    }

    if (input) {
      SocketProxyNode *proxy = new SocketProxyNode(b_node, input, output, true);
      add_node(proxy, b_ntree, key, is_active_group);
    }
  }
}

void NodeGraph::add_proxies_group_inputs(bNode *b_node, bNode *b_node_io)
{
  bNodeTree *b_group_tree = (bNodeTree *)b_node->id;
  BLI_assert(b_group_tree); /* should have been checked in advance */

  /* not important for proxies */
  bNodeInstanceKey key = NODE_INSTANCE_KEY_BASE;
  bool is_active_group = false;

  for (bNodeSocket *b_sock_io = (bNodeSocket *)b_node_io->outputs.first; b_sock_io;
       b_sock_io = b_sock_io->next) {
    bNodeSocket *b_sock_group = find_b_node_input(b_node, b_sock_io->identifier);
    if (b_sock_group) {
      SocketProxyNode *proxy = new SocketProxyNode(b_node_io, b_sock_group, b_sock_io, true);
      add_node(proxy, b_group_tree, key, is_active_group);
    }
  }
}

void NodeGraph::add_proxies_group_outputs(const CompositorContext &context,
                                          bNode *b_node,
                                          bNode *b_node_io)
{
  bNodeTree *b_group_tree = (bNodeTree *)b_node->id;
  BLI_assert(b_group_tree); /* should have been checked in advance */

  /* not important for proxies */
  bNodeInstanceKey key = NODE_INSTANCE_KEY_BASE;
  bool is_active_group = false;

  for (bNodeSocket *b_sock_io = (bNodeSocket *)b_node_io->inputs.first; b_sock_io;
       b_sock_io = b_sock_io->next) {
    bNodeSocket *b_sock_group = find_b_node_output(b_node, b_sock_io->identifier);
    if (b_sock_group) {
      if (context.is_groupnode_buffer_enabled() &&
          context.get_execution_model() == eExecutionModel::Tiled) {
        SocketBufferNode *buffer = new SocketBufferNode(b_node_io, b_sock_io, b_sock_group);
        add_node(buffer, b_group_tree, key, is_active_group);
      }
      else {
        SocketProxyNode *proxy = new SocketProxyNode(b_node_io, b_sock_io, b_sock_group, true);
        add_node(proxy, b_group_tree, key, is_active_group);
      }
    }
  }
}

void NodeGraph::add_proxies_group(const CompositorContext &context,
                                  bNode *b_node,
                                  bNodeInstanceKey key)
{
  bNodeTree *b_group_tree = (bNodeTree *)b_node->id;

  /* missing node group datablock can happen with library linking */
  if (!b_group_tree) {
    /* This error case its handled in convert_to_operations()
     * so we don't get un-converted sockets. */
    return;
  }

  /* use node list size before adding proxies, so they can be connected in add_bNodeTree */
  int nodes_start = nodes_.size();

  /* create proxy nodes for group input/output nodes */
  for (bNode *b_node_io = (bNode *)b_group_tree->nodes.first; b_node_io;
       b_node_io = b_node_io->next) {
    if (b_node_io->type == NODE_GROUP_INPUT) {
      add_proxies_group_inputs(b_node, b_node_io);
    }

    if (b_node_io->type == NODE_GROUP_OUTPUT && (b_node_io->flag & NODE_DO_OUTPUT)) {
      add_proxies_group_outputs(context, b_node, b_node_io);
    }
  }

  add_bNodeTree(context, nodes_start, b_group_tree, key);
}

void NodeGraph::add_proxies_reroute(bNodeTree *b_ntree,
                                    bNode *b_node,
                                    bNodeInstanceKey key,
                                    bool is_active_group)
{
  SocketProxyNode *proxy = new SocketProxyNode(
      b_node, (bNodeSocket *)b_node->inputs.first, (bNodeSocket *)b_node->outputs.first, false);
  add_node(proxy, b_ntree, key, is_active_group);
}

}  // namespace blender::compositor
