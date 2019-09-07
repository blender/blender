/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2013, Blender Foundation.
 */

#include <cstring>

extern "C" {
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "BKE_node.h"
}

#include "COM_CompositorContext.h"
#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_Node.h"
#include "COM_SocketProxyNode.h"

#include "COM_NodeGraph.h" /* own include */

/*******************
 **** NodeGraph ****
 *******************/

NodeGraph::NodeGraph()
{
}

NodeGraph::~NodeGraph()
{
  for (int index = 0; index < this->m_nodes.size(); index++) {
    Node *node = this->m_nodes[index];
    delete node;
  }
}

void NodeGraph::from_bNodeTree(const CompositorContext &context, bNodeTree *tree)
{
  add_bNodeTree(context, 0, tree, NODE_INSTANCE_KEY_BASE);
}

bNodeSocket *NodeGraph::find_b_node_input(bNode *b_group_node, const char *identifier)
{
  for (bNodeSocket *b_sock = (bNodeSocket *)b_group_node->inputs.first; b_sock;
       b_sock = b_sock->next) {
    if (STREQ(b_sock->identifier, identifier)) {
      return b_sock;
    }
  }
  return NULL;
}

bNodeSocket *NodeGraph::find_b_node_output(bNode *b_group_node, const char *identifier)
{
  for (bNodeSocket *b_sock = (bNodeSocket *)b_group_node->outputs.first; b_sock;
       b_sock = b_sock->next) {
    if (STREQ(b_sock->identifier, identifier)) {
      return b_sock;
    }
  }
  return NULL;
}

void NodeGraph::add_node(Node *node,
                         bNodeTree *b_ntree,
                         bNodeInstanceKey key,
                         bool is_active_group)
{
  node->setbNodeTree(b_ntree);
  node->setInstanceKey(key);
  node->setIsInActiveGroup(is_active_group);

  m_nodes.push_back(node);

  DebugInfo::node_added(node);
}

void NodeGraph::add_link(NodeOutput *fromSocket, NodeInput *toSocket)
{
  m_links.push_back(Link(fromSocket, toSocket));

  /* register with the input */
  toSocket->setLink(fromSocket);
}

void NodeGraph::add_bNodeTree(const CompositorContext &context,
                              int nodes_start,
                              bNodeTree *tree,
                              bNodeInstanceKey parent_key)
{
  const bNodeTree *basetree = context.getbNodeTree();

  /* update viewers in the active edittree as well the base tree (for backdrop) */
  bool is_active_group = (parent_key.value == basetree->active_viewer_key.value);

  /* add all nodes of the tree to the node list */
  for (bNode *node = (bNode *)tree->nodes.first; node; node = node->next) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, tree, node);
    add_bNode(context, tree, node, key, is_active_group);
  }

  NodeRange node_range(m_nodes.begin() + nodes_start, m_nodes.end());
  /* add all nodelinks of the tree to the link list */
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
  if (context.isFastCalculation() && !Converter::is_fast_node(b_node)) {
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
    Node *node = Converter::convert(b_node);
    if (node) {
      add_node(node, b_ntree, key, is_active_group);
    }
  }
}

NodeGraph::NodeInputs NodeGraph::find_inputs(const NodeRange &node_range, bNodeSocket *b_socket)
{
  NodeInputs result;
  for (NodeGraph::NodeIterator it = node_range.first; it != node_range.second; ++it) {
    Node *node = *it;
    for (int index = 0; index < node->getNumberOfInputSockets(); index++) {
      NodeInput *input = node->getInputSocket(index);
      if (input->getbNodeSocket() == b_socket) {
        result.push_back(input);
      }
    }
  }
  return result;
}

NodeOutput *NodeGraph::find_output(const NodeRange &node_range, bNodeSocket *b_socket)
{
  for (NodeGraph::NodeIterator it = node_range.first; it != node_range.second; ++it) {
    Node *node = *it;
    for (int index = 0; index < node->getNumberOfOutputSockets(); index++) {
      NodeOutput *output = node->getOutputSocket(index);
      if (output->getbNodeSocket() == b_socket) {
        return output;
      }
    }
  }
  return NULL;
}

void NodeGraph::add_bNodeLink(const NodeRange &node_range, bNodeLink *b_nodelink)
{
  /// \note: ignore invalid links
  if (!(b_nodelink->flag & NODE_LINK_VALID)) {
    return;
  }
  if ((b_nodelink->fromsock->flag & SOCK_UNAVAIL) || (b_nodelink->tosock->flag & SOCK_UNAVAIL)) {
    return;
  }

  /* Note: a DNA input socket can have multiple NodeInput in the compositor tree! (proxies)
   * The output then gets linked to each one of them.
   */

  NodeOutput *output = find_output(node_range, b_nodelink->fromsock);
  if (!output) {
    return;
  }

  NodeInputs inputs = find_inputs(node_range, b_nodelink->tosock);
  for (NodeInputs::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
    NodeInput *input = *it;
    if (input->isLinked()) {
      continue;
    }
    add_link(output, input);
  }
}

/* **** Special proxy node type conversions **** */

void NodeGraph::add_proxies_mute(bNodeTree *b_ntree,
                                 bNode *b_node,
                                 bNodeInstanceKey key,
                                 bool is_active_group)
{
  for (bNodeLink *b_link = (bNodeLink *)b_node->internal_links.first; b_link;
       b_link = b_link->next) {
    SocketProxyNode *proxy = new SocketProxyNode(b_node, b_link->fromsock, b_link->tosock, false);
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

void NodeGraph::add_proxies_group_outputs(bNode *b_node, bNode *b_node_io, bool use_buffer)
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
      if (use_buffer) {
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
    /* This error case its handled in convertToOperations()
     * so we don't get un-converted sockets. */
    return;
  }

  /* use node list size before adding proxies, so they can be connected in add_bNodeTree */
  int nodes_start = m_nodes.size();

  /* create proxy nodes for group input/output nodes */
  for (bNode *b_node_io = (bNode *)b_group_tree->nodes.first; b_node_io;
       b_node_io = b_node_io->next) {
    if (b_node_io->type == NODE_GROUP_INPUT) {
      add_proxies_group_inputs(b_node, b_node_io);
    }

    if (b_node_io->type == NODE_GROUP_OUTPUT && (b_node_io->flag & NODE_DO_OUTPUT)) {
      add_proxies_group_outputs(b_node, b_node_io, context.isGroupnodeBufferEnabled());
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
