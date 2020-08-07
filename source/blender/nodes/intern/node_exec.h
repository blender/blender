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
  NodeFreeExecFunction freeexecfunc;
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

int node_exec_socket_use_stack(struct bNodeSocket *sock);

struct bNodeStack *node_get_socket_stack(struct bNodeStack *stack, struct bNodeSocket *sock);
void node_get_stack(struct bNode *node,
                    struct bNodeStack *stack,
                    struct bNodeStack **in,
                    struct bNodeStack **out);

struct bNodeTreeExec *ntree_exec_begin(struct bNodeExecContext *context,
                                       struct bNodeTree *ntree,
                                       bNodeInstanceKey parent_key);
void ntree_exec_end(struct bNodeTreeExec *exec);

struct bNodeThreadStack *ntreeGetThreadStack(struct bNodeTreeExec *exec, int thread);
void ntreeReleaseThreadStack(struct bNodeThreadStack *nts);
bool ntreeExecThreadNodes(struct bNodeTreeExec *exec,
                          struct bNodeThreadStack *nts,
                          void *callerdata,
                          int thread);

struct bNodeTreeExec *ntreeShaderBeginExecTree_internal(struct bNodeExecContext *context,
                                                        struct bNodeTree *ntree,
                                                        bNodeInstanceKey parent_key);
void ntreeShaderEndExecTree_internal(struct bNodeTreeExec *exec);

struct bNodeTreeExec *ntreeTexBeginExecTree_internal(struct bNodeExecContext *context,
                                                     struct bNodeTree *ntree,
                                                     bNodeInstanceKey parent_key);
void ntreeTexEndExecTree_internal(struct bNodeTreeExec *exec);

#ifdef __cplusplus
}
#endif
