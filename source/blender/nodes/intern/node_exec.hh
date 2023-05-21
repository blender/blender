/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.hh"

#include "node_util.hh"

#include "RNA_types.h"

struct bNode;
struct bNodeStack;
struct bNodeTree;

/* Node execution data */
struct bNodeExec {
  /** Back-pointer to node. */
  bNode *node;
  bNodeExecData data;

  /** Free function, stored in exec itself to avoid dangling node pointer access. */
  NodeFreeExecFunction free_exec_fn;
};

/* Execution Data for each instance of node tree execution */
struct bNodeTreeExec {
  bNodeTree *nodetree; /* Back-pointer to node tree. */

  int totnodes;        /* total node count */
  bNodeExec *nodeexec; /* per-node execution data */

  int stacksize;
  bNodeStack *stack; /* socket data stack */
  /* only used by material and texture trees to keep one stack for each thread */
  ListBase *threadstack; /* one instance of the stack for each thread */
};

/* stores one stack copy for each thread (material and texture trees) */
struct bNodeThreadStack {
  bNodeThreadStack *next, *prev;
  bNodeStack *stack;
  bool used;
};

/** For a given socket, find the actual stack entry. */
bNodeStack *node_get_socket_stack(struct bNodeStack *stack, struct bNodeSocket *sock);
void node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out);

bNodeTreeExec *ntree_exec_begin(bNodeExecContext *context,
                                bNodeTree *ntree,
                                bNodeInstanceKey parent_key);
void ntree_exec_end(bNodeTreeExec *exec);
