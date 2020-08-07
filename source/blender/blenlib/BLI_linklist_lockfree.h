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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

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

/* Make list ready for lock-free access. */
void BLI_linklist_lockfree_init(LockfreeLinkList *list);

/* Completely free the whole list, it is NOT re-usable after this. */
void BLI_linklist_lockfree_free(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func);

/* Remove all the elements from the list, keep it usable for further
 * inserts.
 */
void BLI_linklist_lockfree_clear(LockfreeLinkList *list, LockfreeeLinkNodeFreeFP free_func);

/* Begin iteration of lock-free linked list, starting with a
 * first user=defined node. Will ignore the dummy node.
 */
LockfreeLinkNode *BLI_linklist_lockfree_begin(LockfreeLinkList *list);

/* ************************************************************************** */
/* NOTE: These functions are safe for use from threads. */

void BLI_linklist_lockfree_insert(LockfreeLinkList *list, LockfreeLinkNode *node);

#ifdef __cplusplus
}
#endif
