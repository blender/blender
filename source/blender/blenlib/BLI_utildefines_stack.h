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
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * Macro's for a simple array based stack
 * \note Caller handles alloc & free.
 */

/* only validate array-bounds in debug mode */
#ifdef DEBUG
#  define STACK_DECLARE(stack) unsigned int _##stack##_index, _##stack##_totalloc
#  define STACK_INIT(stack, tot) \
    ((void)stack, (void)((_##stack##_index) = 0), (void)((_##stack##_totalloc) = (tot)))
#  define _STACK_SIZETEST(stack, off) \
    (BLI_assert((_##stack##_index) + (off) <= _##stack##_totalloc))
#  define _STACK_SWAP_TOTALLOC(stack_a, stack_b) \
    SWAP(unsigned int, _##stack_a##_totalloc, _##stack_b##_totalloc)
#else
#  define STACK_DECLARE(stack) unsigned int _##stack##_index
#  define STACK_INIT(stack, tot) \
    ((void)stack, (void)((_##stack##_index) = 0), (void)(0 ? (tot) : 0))
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
      SWAP(void *, stack_a, stack_b); \
      SWAP(unsigned int, _##stack_a##_index, _##stack_b##_index); \
      _STACK_SWAP_TOTALLOC(stack_a, stack_b); \
    } \
    ((void)0)
#endif
