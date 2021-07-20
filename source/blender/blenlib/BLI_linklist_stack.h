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
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief BLI_LINKSTACK_*** wrapper macros for using a \a LinkNode
 *        to store a stack of pointers, using a single linked list
 *        allocated from a mempool.
 *
 * \note These macros follow STACK_* macros defined in 'BLI_utildefines.h'
 *       and should be kept (mostly) interchangeable.
 *
 * \note `_##var##_type` is a dummy variable only used for type-checks.
 */

/* -------------------------------------------------------------------- */
/** \name Linked Stack (mempool)
 *
 * Uses #BLI_mempool for storage.
 * \{ */

#define BLI_LINKSTACK_DECLARE(var, type) \
  LinkNode *var; \
  BLI_mempool *var##_pool_; \
  type var##_type_

#define BLI_LINKSTACK_INIT(var) \
  { \
    var = NULL; \
    var##_pool_ = BLI_mempool_create(sizeof(LinkNode), 0, 64, BLI_MEMPOOL_NOP); \
  } \
  (void)0

#define BLI_LINKSTACK_SIZE(var) BLI_mempool_len(var##_pool_)

/* check for typeof() */
#ifdef __GNUC__
#  define BLI_LINKSTACK_PUSH(var, ptr) \
    (CHECK_TYPE_INLINE(ptr, typeof(var##_type_)), \
     BLI_linklist_prepend_pool(&(var), ptr, var##_pool_))
#  define BLI_LINKSTACK_POP(var) \
    (var ? (typeof(var##_type_))BLI_linklist_pop_pool(&(var), var##_pool_) : NULL)
#  define BLI_LINKSTACK_POP_DEFAULT(var, r) \
    (var ? (typeof(var##_type_))BLI_linklist_pop_pool(&(var), var##_pool_) : r)
#else /* non gcc */
#  define BLI_LINKSTACK_PUSH(var, ptr) (BLI_linklist_prepend_pool(&(var), ptr, var##_pool_))
#  define BLI_LINKSTACK_POP(var) (var ? BLI_linklist_pop_pool(&(var), var##_pool_) : NULL)
#  define BLI_LINKSTACK_POP_DEFAULT(var, r) (var ? BLI_linklist_pop_pool(&(var), var##_pool_) : r)
#endif /* gcc check */

#define BLI_LINKSTACK_SWAP(var_a, var_b) \
  { \
    CHECK_TYPE_PAIR(var_a##_type_, var_b##_type_); \
    SWAP(LinkNode *, var_a, var_b); \
    SWAP(BLI_mempool *, var_a##_pool_, var_b##_pool_); \
  } \
  (void)0

#define BLI_LINKSTACK_FREE(var) \
  { \
    BLI_mempool_destroy(var##_pool_); \
    var##_pool_ = NULL; \
    (void)var##_pool_; \
    var = NULL; \
    (void)var; \
    (void)&(var##_type_); \
  } \
  (void)0

#include "BLI_linklist.h"
#include "BLI_mempool.h"

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linked Stack (alloca)
 *
 * Linked Stack, using stack memory (alloca).
 *
 * alloca never frees, pop'd items are stored in a free-list for reuse.
 * only use for lists small enough to fit on the stack.
 * \{ */

#ifdef __GNUC__
#  define _BLI_SMALLSTACK_CAST(var) (typeof(_##var##_type))
#else
#  define _BLI_SMALLSTACK_CAST(var)
#endif

#define _BLI_SMALLSTACK_FAKEUSER(var) (void)(&(_##var##_type))

#define BLI_SMALLSTACK_DECLARE(var, type) \
  LinkNode *_##var##_stack = NULL, *_##var##_free = NULL, *_##var##_temp = NULL; \
  type _##var##_type

#define BLI_SMALLSTACK_PUSH(var, data) \
  { \
    CHECK_TYPE_PAIR(data, _##var##_type); \
    if (_##var##_free) { \
      _##var##_temp = _##var##_free; \
      _##var##_free = _##var##_free->next; \
    } \
    else { \
      _##var##_temp = (LinkNode *)alloca(sizeof(LinkNode)); \
    } \
    _##var##_temp->next = _##var##_stack; \
    _##var##_temp->link = data; \
    _##var##_stack = _##var##_temp; \
    _BLI_SMALLSTACK_FAKEUSER(var); \
  } \
  (void)0

/* internal use, no null check */
#define _BLI_SMALLSTACK_DEL_EX(var_src, var_dst) \
  (void)(_BLI_SMALLSTACK_FAKEUSER(var_src), \
         _BLI_SMALLSTACK_FAKEUSER(var_dst), \
         (_##var_src##_temp = _##var_src##_stack->next), \
         (_##var_src##_stack->next = _##var_dst##_free), \
         (_##var_dst##_free = _##var_src##_stack), \
         (_##var_src##_stack = _##var_src##_temp))

#define _BLI_SMALLSTACK_DEL(var) _BLI_SMALLSTACK_DEL_EX(var, var)

/* check for typeof() */
#define BLI_SMALLSTACK_POP(var) \
  (_BLI_SMALLSTACK_CAST(var)( \
      (_##var##_stack) ? (_BLI_SMALLSTACK_DEL(var), (_##var##_free->link)) : NULL))

/* support to put the free-node into another stack */
#define BLI_SMALLSTACK_POP_EX(var_src, var_dst) \
  (_BLI_SMALLSTACK_CAST(var_src)( \
      (_##var_src##_stack) ? \
          (_BLI_SMALLSTACK_DEL_EX(var_src, var_dst), (_##var_dst##_free->link)) : \
          NULL))

#define BLI_SMALLSTACK_PEEK(var) \
  (_BLI_SMALLSTACK_CAST(var)((_##var##_stack) ? _##var##_stack->link : NULL))

#define BLI_SMALLSTACK_IS_EMPTY(var) ((_BLI_SMALLSTACK_CAST(var) _##var##_stack) == NULL)

/* fill in a lookup table */
#define BLI_SMALLSTACK_AS_TABLE(var, data) \
  { \
    LinkNode *_##var##_iter; \
    unsigned int i; \
    for (_##var##_iter = _##var##_stack, i = 0; _##var##_iter; \
         _##var##_iter = _##var##_iter->next, i++) { \
      (data)[i] = _BLI_SMALLSTACK_CAST(var)(_##var##_iter->link); \
    } \
  } \
  ((void)0)

/* loop over stack members last-added-first */
#define BLI_SMALLSTACK_ITER_BEGIN(var, item) \
  { \
    LinkNode *_##var##_iter; \
    for (_##var##_iter = _##var##_stack; _##var##_iter; _##var##_iter = _##var##_iter->next) { \
      item = _BLI_SMALLSTACK_CAST(var)(_##var##_iter->link);

#define BLI_SMALLSTACK_ITER_END \
  } \
  } \
  (void)0

#define BLI_SMALLSTACK_SWAP(var_a, var_b) \
  { \
    CHECK_TYPE_PAIR(_##var_a##_type, _##var_b##_type); \
    SWAP(LinkNode *, _##var_a##_stack, _##var_b##_stack); \
    SWAP(LinkNode *, _##var_a##_free, _##var_b##_free); \
  } \
  (void)0

/** \} */
