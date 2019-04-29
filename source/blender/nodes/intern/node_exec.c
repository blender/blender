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

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "node_exec.h"
#include "node_util.h"

/* supported socket types in old nodes */
int node_exec_socket_use_stack(bNodeSocket *sock)
{
  /* NOTE: INT supported as FLOAT. Only for EEVEE. */
  return ELEM(sock->type, SOCK_INT, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_SHADER);
}

/* for a given socket, find the actual stack entry */
bNodeStack *node_get_socket_stack(bNodeStack *stack, bNodeSocket *sock)
{
  if (stack && sock && sock->stack_index >= 0) {
    return stack + sock->stack_index;
  }
  return NULL;
}

void node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out)
{
  bNodeSocket *sock;

  /* build pointer stack */
  if (in) {
    for (sock = node->inputs.first; sock; sock = sock->next) {
      *(in++) = node_get_socket_stack(stack, sock);
    }
  }

  if (out) {
    for (sock = node->outputs.first; sock; sock = sock->next) {
      *(out++) = node_get_socket_stack(stack, sock);
    }
  }
}

static void node_init_input_index(bNodeSocket *sock, int *index)
{
  /* Only consider existing link if from socket is valid! */
  if (sock->link && sock->link->fromsock && sock->link->fromsock->stack_index >= 0) {
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

static void node_init_output_index(bNodeSocket *sock, int *index, ListBase *internal_links)
{
  if (internal_links) {
    bNodeLink *link;
    /* copy the stack index from internally connected input to skip the node */
    for (link = internal_links->first; link; link = link->next) {
      if (link->tosock == sock) {
        sock->stack_index = link->fromsock->stack_index;
        /* set the link pointer to indicate that this socket
         * should not overwrite the stack value!
         */
        sock->link = link;
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
  else {
    if (node_exec_socket_use_stack(sock)) {
      sock->stack_index = (*index)++;
    }
    else {
      sock->stack_index = -1;
    }
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
    return NULL;
  }

  /* don't mess with remote socket stacks, these are initialized by other nodes! */
  if (sock->link) {
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
  bNodeTreeExec *exec;
  bNode *node;
  bNodeExec *nodeexec;
  bNodeInstanceKey nodekey;
  bNodeSocket *sock;
  bNodeStack *ns;
  int index;
  bNode **nodelist;
  int totnodes, n;
  /* XXX texnodes have threading issues with muting, have to disable it there ... */

  /* ensure all sock->link pointers and node levels are correct */
  /* Using global main here is likely totally wrong, not sure what to do about that one though...
   * We cannot even check ntree is in global main,
   * since most of the time it won't be (thanks to ntree design)!!! */
  ntreeUpdateTree(G.main, ntree);

  /* get a dependency-sorted list of nodes */
  ntreeGetDependencyList(ntree, &nodelist, &totnodes);

  /* XXX could let callbacks do this for specialized data */
  exec = MEM_callocN(sizeof(bNodeTreeExec), "node tree execution data");
  /* backpointer to node tree */
  exec->nodetree = ntree;

  /* set stack indices */
  index = 0;
  for (n = 0; n < totnodes; ++n) {
    node = nodelist[n];

    node->stack_index = index;

    /* init node socket stack indexes */
    for (sock = node->inputs.first; sock; sock = sock->next) {
      node_init_input_index(sock, &index);
    }

    if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
      for (sock = node->outputs.first; sock; sock = sock->next) {
        node_init_output_index(sock, &index, &node->internal_links);
      }
    }
    else {
      for (sock = node->outputs.first; sock; sock = sock->next) {
        node_init_output_index(sock, &index, NULL);
      }
    }
  }

  /* allocated exec data pointers for nodes */
  exec->totnodes = totnodes;
  exec->nodeexec = MEM_callocN(exec->totnodes * sizeof(bNodeExec), "node execution data");
  /* allocate data pointer for node stack */
  exec->stacksize = index;
  exec->stack = MEM_callocN(exec->stacksize * sizeof(bNodeStack), "bNodeStack");

  /* all non-const results are considered inputs */
  for (n = 0; n < exec->stacksize; ++n) {
    exec->stack[n].hasinput = 1;
  }

  /* prepare all nodes for execution */
  for (n = 0, nodeexec = exec->nodeexec; n < totnodes; ++n, ++nodeexec) {
    node = nodeexec->node = nodelist[n];
    nodeexec->freeexecfunc = node->typeinfo->freeexecfunc;

    /* tag inputs */
    for (sock = node->inputs.first; sock; sock = sock->next) {
      /* disable the node if an input link is invalid */
      if (sock->link && !(sock->link->flag & NODE_LINK_VALID)) {
        node->need_exec = 0;
      }

      ns = setup_stack(exec->stack, ntree, node, sock);
      if (ns) {
        ns->hasoutput = 1;
      }
    }

    /* tag all outputs */
    for (sock = node->outputs.first; sock; sock = sock->next) {
      /* ns = */ setup_stack(exec->stack, ntree, node, sock);
    }

    nodekey = BKE_node_instance_key(parent_key, ntree, node);
    nodeexec->data.preview = context->previews ?
                                 BKE_node_instance_hash_lookup(context->previews, nodekey) :
                                 NULL;
    if (node->typeinfo->initexecfunc) {
      nodeexec->data.data = node->typeinfo->initexecfunc(context, node, nodekey);
    }
  }

  if (nodelist) {
    MEM_freeN(nodelist);
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

  for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
    if (nodeexec->freeexecfunc) {
      nodeexec->freeexecfunc(nodeexec->data.data);
    }
  }

  if (exec->nodeexec) {
    MEM_freeN(exec->nodeexec);
  }

  MEM_freeN(exec);
}

/**** Material/Texture trees ****/

bNodeThreadStack *ntreeGetThreadStack(bNodeTreeExec *exec, int thread)
{
  ListBase *lb = &exec->threadstack[thread];
  bNodeThreadStack *nts;

  for (nts = lb->first; nts; nts = nts->next) {
    if (!nts->used) {
      nts->used = true;
      break;
    }
  }

  if (!nts) {
    nts = MEM_callocN(sizeof(bNodeThreadStack), "bNodeThreadStack");
    nts->stack = MEM_dupallocN(exec->stack);
    nts->used = true;
    BLI_addtail(lb, nts);
  }

  return nts;
}

void ntreeReleaseThreadStack(bNodeThreadStack *nts)
{
  nts->used = 0;
}

bool ntreeExecThreadNodes(bNodeTreeExec *exec, bNodeThreadStack *nts, void *callerdata, int thread)
{
  bNodeStack *nsin[MAX_SOCKET] = {NULL};  /* arbitrary... watch this */
  bNodeStack *nsout[MAX_SOCKET] = {NULL}; /* arbitrary... watch this */
  bNodeExec *nodeexec;
  bNode *node;
  int n;

  /* nodes are presorted, so exec is in order of list */

  for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
    node = nodeexec->node;
    if (node->need_exec) {
      node_get_stack(node, nts->stack, nsin, nsout);
      /* Handle muted nodes...
       * If the mute func is not set, assume the node should never be muted,
       * and hence execute it!
       */
      if (node->typeinfo->execfunc && !(node->flag & NODE_MUTED)) {
        node->typeinfo->execfunc(callerdata, thread, node, &nodeexec->data, nsin, nsout);
      }
    }
  }

  /* signal to that all went OK, for render */
  return true;
}
