/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Macro's for a simple array based stack
 * \note Caller handles alloc & free.
 */

/* only validate array-bounds in debug mode */
#ifdef DEBUG
#  define STACK_DECLARE(stack) unsigned int _##stack##_index, _##stack##_num_alloc
#  define STACK_INIT(stack, stack_num) \
    ((void)stack, \
     (void)((_##stack##_index) = 0), \
     (void)((_##stack##_num_alloc) = (unsigned int)(stack_num)))
#  define _STACK_SIZETEST(stack, off) \
    (BLI_assert((_##stack##_index) + (off) <= _##stack##_num_alloc))
#  define _STACK_SWAP_TOTALLOC(stack_a, stack_b) \
    SWAP(unsigned int, _##stack_a##_num_alloc, _##stack_b##_num_alloc)
#else
#  define STACK_DECLARE(stack) unsigned int _##stack##_index
#  define STACK_INIT(stack, stack_num) \
    ((void)stack, (void)((_##stack##_index) = 0), (void)(0 ? (stack_num) : 0))
#  define _STACK_SIZETEST(stack, off) (void)(stack), (void)(off)
#  define _STACK_SWAP_TOTALLOC(stack_a, stack_b) (void)(stack_a), (void)(stack_b)
#endif
#define _STACK_BOUNDSTEST(stack, index) \
  ((void)stack, BLI_assert((unsigned int)(index) < _##stack##_index))

#define STACK_SIZE(stack) ((void)stack, (_##stack##_index))
#define STACK_CLEAR(stack) \
  { \
    (void)stack; \
    _##stack##_index = 0; \
  } \
  ((void)0)
/** add item to stack */
#define STACK_PUSH(stack, val) \
  ((void)stack, _STACK_SIZETEST(stack, 1), ((stack)[(_##stack##_index)++] = (val)))
#define STACK_PUSH_RET(stack) \
  ((void)stack, _STACK_SIZETEST(stack, 1), ((stack)[(_##stack##_index)++]))
#define STACK_PUSH_RET_PTR(stack) \
  ((void)stack, _STACK_SIZETEST(stack, 1), &((stack)[(_##stack##_index)++]))
/** take last item from stack */
#define STACK_POP(stack) ((_##stack##_index) ? ((stack)[--(_##stack##_index)]) : NULL)
#define STACK_POP_PTR(stack) ((_##stack##_index) ? &((stack)[--(_##stack##_index)]) : NULL)
#define STACK_POP_DEFAULT(stack, r) ((_##stack##_index) ? ((stack)[--(_##stack##_index)]) : (r))
/** look at last item (assumes non-empty stack) */
#define STACK_PEEK(stack) (BLI_assert(_##stack##_index), ((stack)[_##stack##_index - 1]))
#define STACK_PEEK_PTR(stack) (BLI_assert(_##stack##_index), &((stack)[_##stack##_index - 1]))
/** remove any item from the stack, take care, re-orders */
#define STACK_REMOVE(stack, i) \
  { \
    const unsigned int _i = i; \
    _STACK_BOUNDSTEST(stack, _i); \
    if (--_##stack##_index != _i) { \
      stack[_i] = stack[_##stack##_index]; \
    } \
  } \
  ((void)0)
#define STACK_DISCARD(stack, n) \
  { \
    const unsigned int _n = n; \
    BLI_assert(_##stack##_index >= _n); \
    (void)stack; \
    _##stack##_index -= _n; \
  } \
  ((void)0)
#ifdef __GNUC__
#  define STACK_SWAP(stack_a, stack_b) \
    { \
      SWAP(typeof(stack_a), stack_a, stack_b); \
      SWAP(unsigned int, _##stack_a##_index, _##stack_b##_index); \
      _STACK_SWAP_TOTALLOC(stack_a, stack_b); \
    } \
    ((void)0)
#else
#  define STACK_SWAP(stack_a, stack_b) \
    { \
      SWAP(void *, *(void **)&stack_a, *(void **)&stack_b); \
      SWAP(unsigned int, _##stack_a##_index, _##stack_b##_index); \
      _STACK_SWAP_TOTALLOC(stack_a, stack_b); \
    } \
    ((void)0)
#endif
