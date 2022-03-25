/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "node_util.h"

#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bNode;
struct bNodeStack;
struct bNodeTree;

/* Node execution data */
typedef struct bNodeExec {
  /** Backpointer to node. */
  struct bNode *node;
  bNodeExecData data;

  /** Free function, stored in exec itself to avoid dangling node pointer access. */
  NodeFreeExecFunction free_exec_fn;
} bNodeExec;

/* Execution Data for each instance of node tree execution */
typedef struct bNodeTreeExec {
  struct bNodeTree *nodetree; /* backpointer to node tree */

  int totnodes;               /* total node count */
  struct bNodeExec *nodeexec; /* per-node execution data */

  int stacksize;
  struct bNodeStack *stack; /* socket data stack */
  /* only used by material and texture trees to keep one stack for each thread */
  ListBase *threadstack; /* one instance of the stack for each thread */
} bNodeTreeExec;

/* stores one stack copy for each thread (material and texture trees) */
typedef struct bNodeThreadStack {
  struct bNodeThreadStack *next, *prev;
  struct bNodeStack *stack;
  bool used;
} bNodeThreadStack;

/** For a given socket, find the actual stack entry. */
struct bNodeStack *node_get_socket_stack(struct bNodeStack *stack, struct bNodeSocket *sock);
void node_get_stack(struct bNode *node,
                    struct bNodeStack *stack,
                    struct bNodeStack **in,
                    struct bNodeStack **out);

struct bNodeTreeExec *ntree_exec_begin(struct bNodeExecContext *context,
                                       struct bNodeTree *ntree,
                                       bNodeInstanceKey parent_key);
void ntree_exec_end(struct bNodeTreeExec *exec);

#ifdef __cplusplus
}
#endif
