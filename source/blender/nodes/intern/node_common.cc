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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#include <cstddef>
#include <cstring>

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_node.h"

#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "NOD_common.h"
#include "node_common.h"
#include "node_util.h"

using blender::Map;
using blender::MultiValueMap;
using blender::Set;
using blender::Stack;

/* -------------------------------------------------------------------- */
/** \name Node Group
 * \{ */

bNodeSocket *node_group_find_input_socket(bNode *groupnode, const char *identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &groupnode->inputs) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

bNodeSocket *node_group_find_output_socket(bNode *groupnode, const char *identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &groupnode->outputs) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

/* groups display their internal tree name as label */
void node_group_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  BLI_strncpy(label, (node->id) ? node->id->name + 2 : IFACE_("Missing Data-Block"), maxlen);
}

bool node_group_poll_instance(bNode *node, bNodeTree *nodetree, const char **disabled_hint)
{
  if (node->typeinfo->poll(node->typeinfo, nodetree, disabled_hint)) {
    bNodeTree *grouptree = (bNodeTree *)node->id;
    if (grouptree) {
      return nodeGroupPoll(nodetree, grouptree, disabled_hint);
    }

    return true; /* without a linked node tree, group node is always ok */
  }

  return false;
}

bool nodeGroupPoll(bNodeTree *nodetree, bNodeTree *grouptree, const char **r_disabled_hint)
{
  bool valid = true;

  /* unspecified node group, generally allowed
   * (if anything, should be avoided on operator level)
   */
  if (grouptree == nullptr) {
    return true;
  }

  if (nodetree == grouptree) {
    *r_disabled_hint = "Nesting a node group inside of itself is not allowed";
    return false;
  }

  LISTBASE_FOREACH (bNode *, node, &grouptree->nodes) {
    if (node->typeinfo->poll_instance &&
        !node->typeinfo->poll_instance(node, nodetree, r_disabled_hint)) {
      valid = false;
      break;
    }
  }
  return valid;
}

/* used for both group nodes and interface nodes */
static bNodeSocket *group_verify_socket(bNodeTree *ntree,
                                        bNode *gnode,
                                        bNodeSocket *iosock,
                                        ListBase *verify_lb,
                                        eNodeSocketInOut in_out)
{
  bNodeSocket *sock;

  for (sock = (bNodeSocket *)verify_lb->first; sock; sock = sock->next) {
    if (STREQ(sock->identifier, iosock->identifier)) {
      break;
    }
  }
  if (sock) {
    strcpy(sock->name, iosock->name);

    const int mask = SOCK_HIDE_VALUE;
    sock->flag = (sock->flag & ~mask) | (iosock->flag & mask);

    /* Update socket type if necessary */
    if (sock->typeinfo != iosock->typeinfo) {
      nodeModifySocketType(ntree, gnode, sock, iosock->idname);
      /* Flag the tree to make sure link validity is updated after type changes. */
      ntree->update |= NTREE_UPDATE_LINKS;
    }

    if (iosock->typeinfo->interface_verify_socket) {
      iosock->typeinfo->interface_verify_socket(ntree, iosock, gnode, sock, "interface");
    }
  }
  else {
    sock = nodeAddSocket(ntree, gnode, in_out, iosock->idname, iosock->identifier, iosock->name);

    if (iosock->typeinfo->interface_init_socket) {
      iosock->typeinfo->interface_init_socket(ntree, iosock, gnode, sock, "interface");
    }
  }

  /* remove from list temporarily, to distinguish from orphaned sockets */
  BLI_remlink(verify_lb, sock);

  return sock;
}

/* used for both group nodes and interface nodes */
static void group_verify_socket_list(bNodeTree *ntree,
                                     bNode *gnode,
                                     ListBase *iosock_lb,
                                     ListBase *verify_lb,
                                     eNodeSocketInOut in_out)
{
  bNodeSocket *sock, *nextsock;

  /* step by step compare */

  bNodeSocket *iosock = (bNodeSocket *)iosock_lb->first;
  for (; iosock; iosock = iosock->next) {
    /* abusing new_sock pointer for verification here! only used inside this function */
    iosock->new_sock = group_verify_socket(ntree, gnode, iosock, verify_lb, in_out);
  }
  /* leftovers are removed */
  for (sock = (bNodeSocket *)verify_lb->first; sock; sock = nextsock) {
    nextsock = sock->next;
    nodeRemoveSocket(ntree, gnode, sock);
  }
  /* and we put back the verified sockets */
  iosock = (bNodeSocket *)iosock_lb->first;
  for (; iosock; iosock = iosock->next) {
    if (iosock->new_sock) {
      BLI_addtail(verify_lb, iosock->new_sock);
      iosock->new_sock = nullptr;
    }
  }
}

/* make sure all group node in ntree, which use ngroup, are sync'd */
void node_group_update(struct bNodeTree *ntree, struct bNode *node)
{
  /* check inputs and outputs, and remove or insert them */
  if (node->id == nullptr) {
    nodeRemoveAllSockets(ntree, node);
  }
  else if ((ID_IS_LINKED(node->id) && (node->id->tag & LIB_TAG_MISSING))) {
    /* Missing data-block, leave sockets unchanged so that when it comes back
     * the links remain valid. */
  }
  else {
    bNodeTree *ngroup = (bNodeTree *)node->id;
    group_verify_socket_list(ntree, node, &ngroup->inputs, &node->inputs, SOCK_IN);
    group_verify_socket_list(ntree, node, &ngroup->outputs, &node->outputs, SOCK_OUT);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Frame
 * \{ */

static void node_frame_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeFrame *data = (NodeFrame *)MEM_callocN(sizeof(NodeFrame), "frame node storage");
  node->storage = data;

  data->flag |= NODE_FRAME_SHRINK;

  data->label_size = 20;
}

void register_node_type_frame(void)
{
  /* frame type is used for all tree types, needs dynamic allocation */
  bNodeType *ntype = (bNodeType *)MEM_callocN(sizeof(bNodeType), "frame node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  node_type_base(ntype, NODE_FRAME, "Frame", NODE_CLASS_LAYOUT, NODE_BACKGROUND);
  node_type_init(ntype, node_frame_init);
  node_type_storage(ntype, "NodeFrame", node_free_standard_storage, node_copy_standard_storage);
  node_type_size(ntype, 150, 100, 0);

  nodeRegisterType(ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Re-Route
 * \{ */

/* simple, only a single input and output here */
static void node_reroute_update_internal_links(bNodeTree *ntree, bNode *node)
{
  bNodeLink *link;

  /* Security check! */
  if (!ntree) {
    return;
  }

  link = (bNodeLink *)MEM_callocN(sizeof(bNodeLink), "internal node link");
  link->fromnode = node;
  link->fromsock = (bNodeSocket *)node->inputs.first;
  link->tonode = node;
  link->tosock = (bNodeSocket *)node->outputs.first;
  /* internal link is always valid */
  link->flag |= NODE_LINK_VALID;
  BLI_addtail(&node->internal_links, link);
}

static void node_reroute_init(bNodeTree *ntree, bNode *node)
{
  /* NOTE: Cannot use socket templates for this, since it would reset the socket type
   * on each file read via the template verification procedure.
   */
  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Input", "Input");
  nodeAddStaticSocket(ntree, node, SOCK_OUT, SOCK_RGBA, PROP_NONE, "Output", "Output");
}

void register_node_type_reroute(void)
{
  /* frame type is used for all tree types, needs dynamic allocation */
  bNodeType *ntype = (bNodeType *)MEM_callocN(sizeof(bNodeType), "frame node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  node_type_base(ntype, NODE_REROUTE, "Reroute", NODE_CLASS_LAYOUT, 0);
  node_type_init(ntype, node_reroute_init);
  node_type_internal_links(ntype, node_reroute_update_internal_links);

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

/* Global update function for Reroute node types.
 * This depends on connected nodes, so must be done as a tree-wide update.
 */
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
   * changed in the the loop above. */
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
      nodeModifySocketType(ntree, reroute_node, input_socket, socket_type->idname);
    }
    if (output_socket->typeinfo != socket_type) {
      nodeModifySocketType(ntree, reroute_node, output_socket, socket_type->idname);
    }
  }
}

static bool node_is_connected_to_output_recursive(bNodeTree *ntree, bNode *node)
{
  bNodeLink *link;

  /* avoid redundant checks, and infinite loops in case of cyclic node links */
  if (node->done) {
    return false;
  }
  node->done = 1;

  /* main test, done before child loop so it catches output nodes themselves as well */
  if (node->typeinfo->nclass == NODE_CLASS_OUTPUT && node->flag & NODE_DO_OUTPUT) {
    return true;
  }

  /* test all connected nodes, first positive find is sufficient to return true */
  for (link = (bNodeLink *)ntree->links.first; link; link = link->next) {
    if (link->fromnode == node) {
      if (node_is_connected_to_output_recursive(ntree, link->tonode)) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_node_is_connected_to_output(bNodeTree *ntree, bNode *node)
{
  bNode *tnode;

  /* clear flags */
  for (tnode = (bNode *)ntree->nodes.first; tnode; tnode = tnode->next) {
    tnode->done = 0;
  }

  return node_is_connected_to_output_recursive(ntree, node);
}

void BKE_node_tree_unlink_id(ID *id, struct bNodeTree *ntree)
{
  bNode *node;

  for (node = (bNode *)ntree->nodes.first; node; node = node->next) {
    if (node->id == id) {
      node->id = nullptr;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node #GROUP_INPUT / #GROUP_OUTPUT
 * \{ */

static void node_group_input_init(bNodeTree *ntree, bNode *node)
{
  node_group_input_update(ntree, node);
}

bNodeSocket *node_group_input_find_socket(bNode *node, const char *identifier)
{
  bNodeSocket *sock;
  for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock->next) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

void node_group_input_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *extsock = (bNodeSocket *)node->outputs.last;
  bNodeLink *link, *linknext, *exposelink;
  /* Adding a tree socket and verifying will remove the extension socket!
   * This list caches the existing links from the extension socket
   * so they can be recreated after verification.
   */
  ListBase tmplinks;

  /* find links from the extension socket and store them */
  BLI_listbase_clear(&tmplinks);
  for (link = (bNodeLink *)ntree->links.first; link; link = linknext) {
    linknext = link->next;
    if (nodeLinkIsHidden(link)) {
      continue;
    }

    if (link->fromsock == extsock) {
      bNodeLink *tlink = (bNodeLink *)MEM_callocN(sizeof(bNodeLink), "temporary link");
      *tlink = *link;
      BLI_addtail(&tmplinks, tlink);

      nodeRemLink(ntree, link);
    }
  }

  /* find valid link to expose */
  exposelink = nullptr;
  for (link = (bNodeLink *)tmplinks.first; link; link = link->next) {
    /* XXX Multiple sockets can be connected to the extension socket at once,
     * in that case the arbitrary first link determines name and type.
     * This could be improved by choosing the "best" type among all links,
     * whatever that means.
     */
    if (link->tosock->type != SOCK_CUSTOM) {
      exposelink = link;
      break;
    }
  }

  if (exposelink) {
    bNodeSocket *gsock, *newsock;

    gsock = ntreeAddSocketInterfaceFromSocket(ntree, exposelink->tonode, exposelink->tosock);

    node_group_input_update(ntree, node);
    newsock = node_group_input_find_socket(node, gsock->identifier);

    /* redirect links from the extension socket */
    for (link = (bNodeLink *)tmplinks.first; link; link = link->next) {
      nodeAddLink(ntree, node, newsock, link->tonode, link->tosock);
    }
  }

  BLI_freelistN(&tmplinks);

  /* check inputs and outputs, and remove or insert them */
  {
    /* value_in_out inverted for interface nodes to get correct socket value_property */
    group_verify_socket_list(ntree, node, &ntree->inputs, &node->outputs, SOCK_OUT);

    /* add virtual extension socket */
    nodeAddSocket(ntree, node, SOCK_OUT, "NodeSocketVirtual", "__extend__", "");
  }
}

void register_node_type_group_input(void)
{
  /* used for all tree types, needs dynamic allocation */
  bNodeType *ntype = (bNodeType *)MEM_callocN(sizeof(bNodeType), "node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  node_type_base(ntype, NODE_GROUP_INPUT, "Group Input", NODE_CLASS_INTERFACE, 0);
  node_type_size(ntype, 140, 80, 400);
  node_type_init(ntype, node_group_input_init);
  node_type_update(ntype, node_group_input_update);

  nodeRegisterType(ntype);
}

static void node_group_output_init(bNodeTree *ntree, bNode *node)
{
  node_group_output_update(ntree, node);
}

bNodeSocket *node_group_output_find_socket(bNode *node, const char *identifier)
{
  bNodeSocket *sock;
  for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

void node_group_output_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *extsock = (bNodeSocket *)node->inputs.last;
  bNodeLink *link, *linknext, *exposelink;
  /* Adding a tree socket and verifying will remove the extension socket!
   * This list caches the existing links to the extension socket
   * so they can be recreated after verification.
   */
  ListBase tmplinks;

  /* find links to the extension socket and store them */
  BLI_listbase_clear(&tmplinks);
  for (link = (bNodeLink *)ntree->links.first; link; link = linknext) {
    linknext = link->next;
    if (nodeLinkIsHidden(link)) {
      continue;
    }

    if (link->tosock == extsock) {
      bNodeLink *tlink = (bNodeLink *)MEM_callocN(sizeof(bNodeLink), "temporary link");
      *tlink = *link;
      BLI_addtail(&tmplinks, tlink);

      nodeRemLink(ntree, link);
    }
  }

  /* find valid link to expose */
  exposelink = nullptr;
  for (link = (bNodeLink *)tmplinks.first; link; link = link->next) {
    /* XXX Multiple sockets can be connected to the extension socket at once,
     * in that case the arbitrary first link determines name and type.
     * This could be improved by choosing the "best" type among all links,
     * whatever that means.
     */
    if (link->fromsock->type != SOCK_CUSTOM) {
      exposelink = link;
      break;
    }
  }

  if (exposelink) {
    bNodeSocket *gsock, *newsock;

    /* XXX what if connecting virtual to virtual socket?? */
    gsock = ntreeAddSocketInterfaceFromSocket(ntree, exposelink->fromnode, exposelink->fromsock);

    node_group_output_update(ntree, node);
    newsock = node_group_output_find_socket(node, gsock->identifier);

    /* redirect links to the extension socket */
    for (link = (bNodeLink *)tmplinks.first; link; link = link->next) {
      nodeAddLink(ntree, link->fromnode, link->fromsock, node, newsock);
    }
  }

  BLI_freelistN(&tmplinks);

  /* check inputs and outputs, and remove or insert them */
  {
    /* value_in_out inverted for interface nodes to get correct socket value_property */
    group_verify_socket_list(ntree, node, &ntree->outputs, &node->inputs, SOCK_IN);

    /* add virtual extension socket */
    nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketVirtual", "__extend__", "");
  }
}

void register_node_type_group_output(void)
{
  /* used for all tree types, needs dynamic allocation */
  bNodeType *ntype = (bNodeType *)MEM_callocN(sizeof(bNodeType), "node type");
  ntype->free_self = (void (*)(bNodeType *))MEM_freeN;

  node_type_base(ntype, NODE_GROUP_OUTPUT, "Group Output", NODE_CLASS_INTERFACE, 0);
  node_type_size(ntype, 140, 80, 400);
  node_type_init(ntype, node_group_output_init);
  node_type_update(ntype, node_group_output_update);

  nodeRegisterType(ntype);
}

/** \} */
