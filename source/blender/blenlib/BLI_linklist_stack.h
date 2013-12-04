/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_LINKLIST_STACK_H__
#define __BLI_LINKLIST_STACK_H__

/** \file BLI_linklist_stack.h
 * \ingroup bli
 * \brief BLI_LINKSTACK_*** wrapper macros for using a \a LinkNode
 *        to store a stack of pointers, using a single linked list
 *        allocated from a mempool.
 *
 * \note These macros follow STACK_* macros defined in 'BLI_utildefines.h'
 *       and should be kept (mostly) interchangeable.
 *
 * \note _##var##_type is a dummy var only used for typechecks.
 */

/* -------------------------------------------------------------------- */
/* Linked Stack using BLI_mempool
 *
 * Uses mempool for storage.
 */

/** \name Linked Stack (mempool)
 * \{ */

#define BLI_LINKSTACK_DECLARE(var, type) \
	LinkNode *var; \
	BLI_mempool *_##var##_pool; \
	type _##var##_type

#define BLI_LINKSTACK_INIT(var)  { \
	var = NULL; \
	_##var##_pool = BLI_mempool_create(sizeof(LinkNode), 1, 64, 0); \
} (void)0

#define BLI_LINKSTACK_SIZE(var) \
	BLI_mempool_count(_##var##_pool)

/* check for typeof() */
#ifdef __GNUC__
#define BLI_LINKSTACK_PUSH(var, ptr)  ( \
	CHECK_TYPE_INLINE(ptr, typeof(_##var##_type)), \
	BLI_linklist_prepend_pool(&(var), ptr, _##var##_pool))
#define BLI_LINKSTACK_POP(var) \
	(var ? (typeof(_##var##_type))BLI_linklist_pop_pool(&(var), _##var##_pool) : NULL)
#define BLI_LINKSTACK_POP_ELSE(var, r) \
	(var ? (typeof(_##var##_type))BLI_linklist_pop_pool(&(var), _##var##_pool) : r)
#else  /* non gcc */
#define BLI_LINKSTACK_PUSH(var, ptr)  ( \
	BLI_linklist_prepend_pool(&(var), ptr, _##var##_pool))
#define BLI_LINKSTACK_POP(var) \
	(var ? BLI_linklist_pop_pool(&(var), _##var##_pool) : NULL)
#define BLI_LINKSTACK_POP_ELSE(var, r) \
	(var ? BLI_linklist_pop_pool(&(var), _##var##_pool) : r)
#endif  /* gcc check */

#define BLI_LINKSTACK_SWAP(var_a, var_b)  { \
	CHECK_TYPE_PAIR(_##var_a##_type, _##var_b##_type); \
	SWAP(LinkNode *, var_a, var_b); \
	SWAP(BLI_mempool *, _##var_a##_pool, _##var_b##_pool); \
} (void)0

#define BLI_LINKSTACK_FREE(var)  { \
	BLI_mempool_destroy(_##var##_pool); \
	_##var##_pool = NULL; (void)_##var##_pool; \
	var = NULL; (void)var; \
	(void)&(_##var##_type); \
} (void)0

#include "BLI_linklist.h"
#include "BLI_mempool.h"

/** \} */


/* -------------------------------------------------------------------- */
/* Linked Stack, using stack memory (alloca)
 *
 * alloca never frees, pop'd items are stored in a free-list for reuse.
 * only use for lists small enough to fit on the stack.
 */


/** \name Linked Stack (alloca)
 * \{ */

#ifdef __GNUC__
#  define _BLI_SMALLSTACK_CAST(var) (typeof(_##var##_type))
#else
#  define _BLI_SMALLSTACK_CAST(var)
#endif

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
		_##var##_temp = alloca(sizeof(LinkNode)); \
	} \
	_##var##_temp->next = _##var##_stack; \
	_##var##_temp->link = data; \
	_##var##_stack = _##var##_temp; \
} (void)0

/* internal use, no null check */
#define _BLI_SMALLSTACK_DEL(var) \
	(void)((_##var##_temp = _##var##_stack->next), \
	       (_##var##_stack->next = _##var##_free), \
	       (_##var##_free = _##var##_stack), \
	       (_##var##_stack = _##var##_temp)) \

/* check for typeof() */
#define BLI_SMALLSTACK_POP(var) \
	(_BLI_SMALLSTACK_CAST(var) ((_##var##_stack) ? \
	(_BLI_SMALLSTACK_DEL(var), (_##var##_free->link)) : NULL))

/* loop over stack members last-added-first */
#define BLI_SMALLSTACK_ITER_BEGIN(var, item) \
	{ \
		LinkNode *_##var##_iter; \
		for (_##var##_iter = _##var##_stack; _##var##_iter; _##var##_iter = _##var##_iter->next) { \
			item = _BLI_SMALLSTACK_CAST(var) (_##var##_iter->link); \

#define BLI_SMALLSTACK_ITER_END \
		} \
	} (void)0

#define BLI_SMALLSTACK_FREE(var)  { \
	(void)&(_##var##_type); \
} (void)0

/** \} */

#endif  /* __BLI_LINKLIST_STACK_H__ */

