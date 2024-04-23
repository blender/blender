/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;
struct MemArena;

typedef void (*LinkNodeFreeFP)(void *link);
typedef void (*LinkNodeApplyFP)(void *link, void *userdata);

typedef struct LinkNode {
  struct LinkNode *next;
  void *link;
} LinkNode;

/**
 * Use for append (single linked list, storing the last element).
 *
 * \note list manipulation functions don't operate on this struct.
 * This is only to be used while appending.
 */
typedef struct LinkNodePair {
  LinkNode *list, *last_node;
} LinkNodePair;

int BLI_linklist_count(const LinkNode *list) ATTR_WARN_UNUSED_RESULT;
int BLI_linklist_index(const LinkNode *list, const void *ptr) ATTR_WARN_UNUSED_RESULT;

LinkNode *BLI_linklist_find(LinkNode *list, int index) ATTR_WARN_UNUSED_RESULT;
LinkNode *BLI_linklist_find_last(LinkNode *list) ATTR_WARN_UNUSED_RESULT;

void BLI_linklist_reverse(LinkNode **listp) ATTR_NONNULL(1);

/**
 * Move an item from its current position to a new one inside a single-linked list.
 * \note `*listp` may be modified.
 */
void BLI_linklist_move_item(LinkNode **listp, int curr_index, int new_index) ATTR_NONNULL(1);

/**
 * A version of #BLI_linklist_prepend that takes the allocated link.
 */
void BLI_linklist_prepend_nlink(LinkNode **listp, void *ptr, LinkNode *nlink) ATTR_NONNULL(1, 3);
void BLI_linklist_prepend(LinkNode **listp, void *ptr) ATTR_NONNULL(1);
void BLI_linklist_prepend_arena(LinkNode **listp, void *ptr, struct MemArena *ma)
    ATTR_NONNULL(1, 3);
void BLI_linklist_prepend_pool(LinkNode **listp, void *ptr, struct BLI_mempool *mempool)
    ATTR_NONNULL(1, 3);

/* Use #LinkNodePair to avoid full search. */

/**
 * A version of append that takes the allocated link.
 */
void BLI_linklist_append_nlink(LinkNodePair *list_pair, void *ptr, LinkNode *nlink)
    ATTR_NONNULL(1, 3);
void BLI_linklist_append(LinkNodePair *list_pair, void *ptr) ATTR_NONNULL(1);
void BLI_linklist_append_arena(LinkNodePair *list_pair, void *ptr, struct MemArena *ma)
    ATTR_NONNULL(1, 3);
void BLI_linklist_append_pool(LinkNodePair *list_pair, void *ptr, struct BLI_mempool *mempool)
    ATTR_NONNULL(1, 3);

void *BLI_linklist_pop(LinkNode **listp) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_linklist_pop_pool(LinkNode **listp, struct BLI_mempool *mempool) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
void BLI_linklist_insert_after(LinkNode **listp, void *ptr) ATTR_NONNULL(1);

void BLI_linklist_free(LinkNode *list, LinkNodeFreeFP freefunc);
void BLI_linklist_freeN(LinkNode *list);
void BLI_linklist_free_pool(LinkNode *list, LinkNodeFreeFP freefunc, struct BLI_mempool *mempool);
void BLI_linklist_apply(LinkNode *list, LinkNodeApplyFP applyfunc, void *userdata);
LinkNode *BLI_linklist_sort(LinkNode *list,
                            int (*cmp)(const void *, const void *)) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(2);
LinkNode *BLI_linklist_sort_r(LinkNode *list,
                              int (*cmp)(void *, const void *, const void *),
                              void *thunk) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);

#define BLI_linklist_prepend_alloca(listp, ptr) \
  BLI_linklist_prepend_nlink(listp, ptr, (LinkNode *)alloca(sizeof(LinkNode)))
#define BLI_linklist_append_alloca(list_pair, ptr) \
  BLI_linklist_append_nlink(list_pair, ptr, (LinkNode *)alloca(sizeof(LinkNode)))

#ifdef __cplusplus
}
#endif
