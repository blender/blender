/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LockfreeLinkNode {
  struct LockfreeLinkNode *next;
  /* NOTE: "Subclass" this structure to add custom-defined data. */
} LockfreeLinkNode;

typedef struct LockfreeLinkList {
  /* We keep a dummy node at the beginning of the list all the time.
   * This allows us to make sure head and tail pointers are always
   * valid, and saves from annoying exception cases in insert().
   */
  LockfreeLinkNode dummy_node;
  /* NOTE: This fields might point to a dummy node. */
  LockfreeLinkNode *head, *tail;
} LockfreeLinkList;

typedef void (*LockfreeeLinkNodeFreeFP)(void *link);

/* ************************************************************************** */
/* NOTE: These functions are NOT safe for use from threads. */
/* NOTE: !!! I REPEAT: DO NOT USE THEM WITHOUT EXTERNAL LOCK !!! */

/** Make list ready for lock-free access. */
void BLI_linklist_lockfree_init(LockfreeLinkList *list);

/** Completely free the whole list, it is NOT re-usable after this. */
void BLI_linklist_lockfree_free(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func);

/**
 * Remove all the elements from the list, keep it usable for further inserts.
 */
void BLI_linklist_lockfree_clear(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func);

/**
 * Begin iteration of lock-free linked list, starting with a
 * first user=defined node. Will ignore the dummy node.
 */
LockfreeLinkNode *BLI_linklist_lockfree_begin(LockfreeLinkList *list);

/* ************************************************************************** */
/* NOTE: These functions are safe for use from threads. */

void BLI_linklist_lockfree_insert(LockfreeLinkList *list, LockfreeLinkNode *node);

#ifdef __cplusplus
}
#endif
