/* SPDX-FileCopyrightText: 2023 Blender Authors
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

typedef struct BLI_Stack BLI_Stack;

BLI_Stack *BLI_stack_new_ex(size_t elem_size,
                            const char *description,
                            size_t chunk_size) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Create a new homogeneous stack with elements of 'elem_size' bytes.
 */
BLI_Stack *BLI_stack_new(size_t elem_size, const char *description) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Free the stack's data and the stack itself
 */
void BLI_stack_free(BLI_Stack *stack) ATTR_NONNULL();

/**
 * Push a new item onto the stack.
 *
 * \return a pointer #BLI_Stack.elem_size
 * bytes of uninitialized memory (caller must fill in).
 */
void *BLI_stack_push_r(BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Copies the source value onto the stack
 *
 * \note This copies #BLI_Stack.elem_size bytes from \a src,
 * (the pointer itself is not stored).
 *
 * \param src: source data to be copied to the stack.
 */
void BLI_stack_push(BLI_Stack *stack, const void *src) ATTR_NONNULL();

/**
 * A version of #BLI_stack_pop which fills in an array.
 *
 * \param dst: The destination array,
 * must be at least (#BLI_Stack.elem_size * \a n) bytes long.
 * \param n: The number of items to pop.
 *
 * \note The first item in the array will be last item added to the stack.
 */
void BLI_stack_pop_n(BLI_Stack *stack, void *dst, unsigned int n) ATTR_NONNULL();
/**
 * A version of #BLI_stack_pop_n which fills in an array (in the reverse order).
 *
 * \note The first item in the array will be first item added to the stack.
 */
void BLI_stack_pop_n_reverse(BLI_Stack *stack, void *dst, unsigned int n) ATTR_NONNULL();
/**
 * Retrieves and removes the top element from the stack.
 * The value is copies to \a dst, which must be at least \a elem_size bytes.
 *
 * Does not reduce amount of allocated memory.
 */
void BLI_stack_pop(BLI_Stack *stack, void *dst) ATTR_NONNULL();

void *BLI_stack_peek(BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Removes the top element from the stack.
 */
void BLI_stack_discard(BLI_Stack *stack) ATTR_NONNULL();
/**
 * Discards all elements without freeing.
 */
void BLI_stack_clear(BLI_Stack *stack) ATTR_NONNULL();

size_t BLI_stack_count(const BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Returns true if the stack is empty, false otherwise
 */
bool BLI_stack_is_empty(const BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
