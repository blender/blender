/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_linklist_lockfree.h"
#include "BLI_strict_flags.h"

#include "atomic_ops.h"

void BLI_linklist_lockfree_init(LockfreeLinkList *list)
{
  list->dummy_node.next = NULL;
  list->head = list->tail = &list->dummy_node;
}

void BLI_linklist_lockfree_free(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func)
{
  if (free_func != NULL) {
    /* NOTE: We start from a first user-added node. */
    LockfreeLinkNode *node = list->head->next;
    while (node != NULL) {
      LockfreeLinkNode *node_next = node->next;
      free_func(node);
      node = node_next;
    }
  }
}

void BLI_linklist_lockfree_clear(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func)
{
  BLI_linklist_lockfree_free(list, free_func);
  BLI_linklist_lockfree_init(list);
}

void BLI_linklist_lockfree_insert(LockfreeLinkList *list, LockfreeLinkNode *node)
{
  /* Based on:
   *
   * John D. Valois
   * Implementing Lock-Free Queues
   *
   * http://people.csail.mit.edu/bushl2/rpi/portfolio/lockfree-grape/documents/lock-free-linked-lists.pdf
   */
  bool keep_working;
  LockfreeLinkNode *tail_node;
  node->next = NULL;
  do {
    tail_node = list->tail;
    keep_working = (atomic_cas_ptr((void **)&tail_node->next, NULL, node) != NULL);
    if (keep_working) {
      atomic_cas_ptr((void **)&list->tail, tail_node, tail_node->next);
    }
  } while (keep_working);
  atomic_cas_ptr((void **)&list->tail, tail_node, node);
}

LockfreeLinkNode *BLI_linklist_lockfree_begin(LockfreeLinkList *list)
{
  return list->head->next;
}
