/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup nodes
 */

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"

#include "MEM_guardedalloc.h"

#include "node_exec.h"
#include "node_util.h"

static int node_exec_socket_use_stack(bNodeSocket *sock)
{
  /* NOTE: INT supported as FLOAT. Only for EEVEE. */
  return ELEM(sock->type, SOCK_INT, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_SHADER);
}

bNodeStack *node_get_socket_stack(bNodeStack *stack, bNodeSocket *sock)
{
  if (stack && sock && sock->stack_index >= 0) {
    return stack + sock->stack_index;
  }
  return nullptr;
}

void node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out)
{
  bNodeSocket *sock;

  /* build pointer stack */
  if (in) {
    for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
      *(in++) = node_get_socket_stack(stack, sock);
    }
  }

  if (out) {
    for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock->next) {
      *(out++) = node_get_socket_stack(stack, sock);
    }
  }
}

static void node_init_input_index(bNodeSocket *sock, int *index)
{
  /* Only consider existing link if from socket is valid! */
  if (sock->link && !(sock->link->flag & NODE_LINK_MUTED) && sock->link->fromsock &&
      sock->link->fromsock->stack_index >= 0) {
    sock->stack_index = sock->link->fromsock->stack_index;
  }
  else {
    if (node_exec_socket_use_stack(sock)) {
      sock->stack_index = (*index)++;
    }
    else {
      sock->stack_index = -1;
    }
  }
}

static void node_init_output_index_muted(bNodeSocket *sock,
                                         int *index,
                                         const blender::MutableSpan<bNodeLink> internal_links)
{
  const bNodeLink *link;
  /* copy the stack index from internally connected input to skip the node */
  for (bNodeLink &iter_link : internal_links) {
    if (iter_link.tosock == sock) {
      sock->stack_index = iter_link.fromsock->stack_index;
      /* set the link pointer to indicate that this socket
       * should not overwrite the stack value!
       */
      sock->link = &iter_link;
      link = &iter_link;
      break;
    }
  }
  /* if not internally connected, assign a new stack index anyway to avoid bad stack access */
  if (!link) {
    if (node_exec_socket_use_stack(sock)) {
      sock->stack_index = (*index)++;
    }
    else {
      sock->stack_index = -1;
    }
  }
}

static void node_init_output_index(bNodeSocket *sock, int *index)
{
  if (node_exec_socket_use_stack(sock)) {
    sock->stack_index = (*index)++;
  }
  else {
    sock->stack_index = -1;
  }
}

/* basic preparation of socket stacks */
static struct bNodeStack *setup_stack(bNodeStack *stack,
                                      bNodeTree *ntree,
                                      bNode *node,
                                      bNodeSocket *sock)
{
  bNodeStack *ns = node_get_socket_stack(stack, sock);
  if (!ns) {
    return nullptr;
  }

  /* don't mess with remote socket stacks, these are initialized by other nodes! */
  if (sock->link && !(sock->link->flag & NODE_LINK_MUTED)) {
    return ns;
  }

  ns->sockettype = sock->type;

  switch (sock->type) {
    case SOCK_FLOAT:
      ns->vec[0] = node_socket_get_float(ntree, node, sock);
      break;
    case SOCK_VECTOR:
      node_socket_get_vector(ntree, node, sock, ns->vec);
      break;
    case SOCK_RGBA:
      node_socket_get_color(ntree, node, sock, ns->vec);
      break;
  }

  return ns;
}

bNodeTreeExec *ntree_exec_begin(bNodeExecContext *context,
                                bNodeTree *ntree,
                                bNodeInstanceKey parent_key)
{
  using namespace blender;
  bNodeTreeExec *exec;
  bNode *node;
  bNodeExec *nodeexec;
  bNodeInstanceKey nodekey;
  bNodeSocket *sock;
  bNodeStack *ns;
  int index;
  /* XXX: texture-nodes have threading issues with muting, have to disable it there. */

  /* ensure all sock->link pointers and node levels are correct */
  /* Using global main here is likely totally wrong, not sure what to do about that one though...
   * We cannot even check ntree is in global main,
   * since most of the time it won't be (thanks to ntree design)!!! */
  BKE_ntree_update_main_tree(G.main, ntree, nullptr);

  ntree->ensure_topology_cache();
  const Span<bNode *> nodelist = ntree->toposort_left_to_right();

  /* XXX could let callbacks do this for specialized data */
  exec = MEM_cnew<bNodeTreeExec>("node tree execution data");
  /* Back-pointer to node tree. */
  exec->nodetree = ntree;

  /* set stack indices */
  index = 0;
  for (const int n : nodelist.index_range()) {
    node = nodelist[n];

    /* init node socket stack indexes */
    for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
      node_init_input_index(sock, &index);
    }

    if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
      for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock->next) {
        node_init_output_index_muted(sock, &index, node->runtime->internal_links);
      }
    }
    else {
      for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock->next) {
        node_init_output_index(sock, &index);
      }
    }
  }

  /* allocated exec data pointers for nodes */
  exec->totnodes = nodelist.size();
  exec->nodeexec = (bNodeExec *)MEM_callocN(exec->totnodes * sizeof(bNodeExec),
                                            "node execution data");
  /* allocate data pointer for node stack */
  exec->stacksize = index;
  exec->stack = (bNodeStack *)MEM_callocN(exec->stacksize * sizeof(bNodeStack), "bNodeStack");

  /* all non-const results are considered inputs */
  int n;
  for (n = 0; n < exec->stacksize; n++) {
    exec->stack[n].hasinput = 1;
  }

  /* prepare all nodes for execution */
  for (n = 0, nodeexec = exec->nodeexec; n < nodelist.size(); n++, nodeexec++) {
    node = nodeexec->node = nodelist[n];
    nodeexec->free_exec_fn = node->typeinfo->free_exec_fn;

    /* tag inputs */
    for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
      /* disable the node if an input link is invalid */
      if (sock->link && !(sock->link->flag & NODE_LINK_VALID)) {
        node->runtime->need_exec = 0;
      }

      ns = setup_stack(exec->stack, ntree, node, sock);
      if (ns) {
        ns->hasoutput = 1;
      }
    }

    /* tag all outputs */
    for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock->next) {
      /* ns = */ setup_stack(exec->stack, ntree, node, sock);
    }

    nodekey = BKE_node_instance_key(parent_key, ntree, node);
    nodeexec->data.preview = context->previews ? (bNodePreview *)BKE_node_instance_hash_lookup(
                                                     context->previews, nodekey) :
                                                 nullptr;
    if (node->typeinfo->init_exec_fn) {
      nodeexec->data.data = node->typeinfo->init_exec_fn(context, node, nodekey);
    }
  }

  return exec;
}

void ntree_exec_end(bNodeTreeExec *exec)
{
  bNodeExec *nodeexec;
  int n;

  if (exec->stack) {
    MEM_freeN(exec->stack);
  }

  for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; n++, nodeexec++) {
    if (nodeexec->free_exec_fn) {
      nodeexec->free_exec_fn(nodeexec->data.data);
    }
  }

  if (exec->nodeexec) {
    MEM_freeN(exec->nodeexec);
  }

  MEM_freeN(exec);
}
