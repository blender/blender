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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * Support for linked lists.
 */

/** \file
 * \ingroup bli
 *
 * Routines for working with single linked lists of 'links' - pointers to other data.
 *
 * For double linked lists see 'BLI_listbase.h'.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

int BLI_linklist_count(const LinkNode *list)
{
  int len;

  for (len = 0; list; list = list->next) {
    len++;
  }

  return len;
}

int BLI_linklist_index(const LinkNode *list, void *ptr)
{
  int index;

  for (index = 0; list; list = list->next, index++) {
    if (list->link == ptr) {
      return index;
    }
  }

  return -1;
}

LinkNode *BLI_linklist_find(LinkNode *list, int index)
{
  int i;

  for (i = 0; list; list = list->next, i++) {
    if (i == index) {
      return list;
    }
  }

  return NULL;
}

void BLI_linklist_reverse(LinkNode **listp)
{
  LinkNode *rhead = NULL, *cur = *listp;

  while (cur) {
    LinkNode *next = cur->next;

    cur->next = rhead;
    rhead = cur;

    cur = next;
  }

  *listp = rhead;
}

/**
 * Move an item from its current position to a new one inside a single-linked list.
 * Note *listp may be modified.
 */
void BLI_linklist_move_item(LinkNode **listp, int curr_index, int new_index)
{
  LinkNode *lnk, *lnk_psrc = NULL, *lnk_pdst = NULL;
  int i;

  if (new_index == curr_index) {
    return;
  }

  if (new_index < curr_index) {
    for (lnk = *listp, i = 0; lnk; lnk = lnk->next, i++) {
      if (i == new_index - 1) {
        lnk_pdst = lnk;
      }
      else if (i == curr_index - 1) {
        lnk_psrc = lnk;
        break;
      }
    }

    if (!(lnk_psrc && lnk_psrc->next && (!lnk_pdst || lnk_pdst->next))) {
      /* Invalid indices, abort. */
      return;
    }

    lnk = lnk_psrc->next;
    lnk_psrc->next = lnk->next;
    if (lnk_pdst) {
      lnk->next = lnk_pdst->next;
      lnk_pdst->next = lnk;
    }
    else {
      /* destination is first element of the list... */
      lnk->next = *listp;
      *listp = lnk;
    }
  }
  else {
    for (lnk = *listp, i = 0; lnk; lnk = lnk->next, i++) {
      if (i == new_index) {
        lnk_pdst = lnk;
        break;
      }
      else if (i == curr_index - 1) {
        lnk_psrc = lnk;
      }
    }

    if (!(lnk_pdst && (!lnk_psrc || lnk_psrc->next))) {
      /* Invalid indices, abort. */
      return;
    }

    if (lnk_psrc) {
      lnk = lnk_psrc->next;
      lnk_psrc->next = lnk->next;
    }
    else {
      /* source is first element of the list... */
      lnk = *listp;
      *listp = lnk->next;
    }
    lnk->next = lnk_pdst->next;
    lnk_pdst->next = lnk;
  }
}

/**
 * A version of prepend that takes the allocated link.
 */
void BLI_linklist_prepend_nlink(LinkNode **listp, void *ptr, LinkNode *nlink)
{
  nlink->link = ptr;
  nlink->next = *listp;
  *listp = nlink;
}

void BLI_linklist_prepend(LinkNode **listp, void *ptr)
{
  LinkNode *nlink = MEM_mallocN(sizeof(*nlink), __func__);
  BLI_linklist_prepend_nlink(listp, ptr, nlink);
}

void BLI_linklist_prepend_arena(LinkNode **listp, void *ptr, MemArena *ma)
{
  LinkNode *nlink = BLI_memarena_alloc(ma, sizeof(*nlink));
  BLI_linklist_prepend_nlink(listp, ptr, nlink);
}

void BLI_linklist_prepend_pool(LinkNode **listp, void *ptr, BLI_mempool *mempool)
{
  LinkNode *nlink = BLI_mempool_alloc(mempool);
  BLI_linklist_prepend_nlink(listp, ptr, nlink);
}

/**
 * A version of append that takes the allocated link.
 */
void BLI_linklist_append_nlink(LinkNodePair *list_pair, void *ptr, LinkNode *nlink)
{
  nlink->link = ptr;
  nlink->next = NULL;

  if (list_pair->list) {
    BLI_assert((list_pair->last_node != NULL) && (list_pair->last_node->next == NULL));
    list_pair->last_node->next = nlink;
  }
  else {
    BLI_assert(list_pair->last_node == NULL);
    list_pair->list = nlink;
  }

  list_pair->last_node = nlink;
}

void BLI_linklist_append(LinkNodePair *list_pair, void *ptr)
{
  LinkNode *nlink = MEM_mallocN(sizeof(*nlink), __func__);
  BLI_linklist_append_nlink(list_pair, ptr, nlink);
}

void BLI_linklist_append_arena(LinkNodePair *list_pair, void *ptr, MemArena *ma)
{
  LinkNode *nlink = BLI_memarena_alloc(ma, sizeof(*nlink));
  BLI_linklist_append_nlink(list_pair, ptr, nlink);
}

void BLI_linklist_append_pool(LinkNodePair *list_pair, void *ptr, BLI_mempool *mempool)
{
  LinkNode *nlink = BLI_mempool_alloc(mempool);
  BLI_linklist_append_nlink(list_pair, ptr, nlink);
}

void *BLI_linklist_pop(struct LinkNode **listp)
{
  /* intentionally no NULL check */
  void *link = (*listp)->link;
  void *next = (*listp)->next;

  MEM_freeN(*listp);

  *listp = next;
  return link;
}

void *BLI_linklist_pop_pool(struct LinkNode **listp, struct BLI_mempool *mempool)
{
  /* intentionally no NULL check */
  void *link = (*listp)->link;
  void *next = (*listp)->next;

  BLI_mempool_free(mempool, (*listp));

  *listp = next;
  return link;
}

void BLI_linklist_insert_after(LinkNode **listp, void *ptr)
{
  LinkNode *nlink = MEM_mallocN(sizeof(*nlink), __func__);
  LinkNode *node = *listp;

  nlink->link = ptr;

  if (node) {
    nlink->next = node->next;
    node->next = nlink;
  }
  else {
    nlink->next = NULL;
    *listp = nlink;
  }
}

void BLI_linklist_free(LinkNode *list, LinkNodeFreeFP freefunc)
{
  while (list) {
    LinkNode *next = list->next;

    if (freefunc) {
      freefunc(list->link);
    }
    MEM_freeN(list);

    list = next;
  }
}

void BLI_linklist_free_pool(LinkNode *list, LinkNodeFreeFP freefunc, struct BLI_mempool *mempool)
{
  while (list) {
    LinkNode *next = list->next;

    if (freefunc) {
      freefunc(list->link);
    }
    BLI_mempool_free(mempool, list);

    list = next;
  }
}

void BLI_linklist_freeN(LinkNode *list)
{
  while (list) {
    LinkNode *next = list->next;

    MEM_freeN(list->link);
    MEM_freeN(list);

    list = next;
  }
}

void BLI_linklist_apply(LinkNode *list, LinkNodeApplyFP applyfunc, void *userdata)
{
  for (; list; list = list->next) {
    applyfunc(list->link, userdata);
  }
}

/* -------------------------------------------------------------------- */
/* Sort */
#define SORT_IMPL_LINKTYPE LinkNode
#define SORT_IMPL_LINKTYPE_DATA link

/* regular call */
#define SORT_IMPL_FUNC linklist_sort_fn
#include "list_sort_impl.h"
#undef SORT_IMPL_FUNC

/* re-entrant call */
#define SORT_IMPL_USE_THUNK
#define SORT_IMPL_FUNC linklist_sort_fn_r
#include "list_sort_impl.h"
#undef SORT_IMPL_FUNC
#undef SORT_IMPL_USE_THUNK

#undef SORT_IMPL_LINKTYPE
#undef SORT_IMPL_LINKTYPE_DATA

LinkNode *BLI_linklist_sort(LinkNode *list, int (*cmp)(const void *, const void *))
{
  if (list && list->next) {
    list = linklist_sort_fn(list, cmp);
  }
  return list;
}

LinkNode *BLI_linklist_sort_r(LinkNode *list,
                              int (*cmp)(void *, const void *, const void *),
                              void *thunk)
{
  if (list && list->next) {
    list = linklist_sort_fn_r(list, cmp, thunk);
  }
  return list;
}
