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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __BLI_STACK_H__
#define __BLI_STACK_H__

/** \file BLI_stack.h
 *  \ingroup bli
 */

#include "BLI_compiler_attrs.h"

typedef struct BLI_Stack BLI_Stack;

BLI_Stack *BLI_stack_new_ex(
        const size_t elem_size, const char *description,
        const size_t chunk_size) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_Stack *BLI_stack_new(
        const size_t elem_size, const char *description) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void BLI_stack_free(BLI_Stack *stack) ATTR_NONNULL();

void *BLI_stack_push_r(BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void  BLI_stack_push(BLI_Stack *stack, const void *src) ATTR_NONNULL();

void BLI_stack_pop_n(BLI_Stack *stack, void *dst, unsigned int n) ATTR_NONNULL();
void BLI_stack_pop(BLI_Stack *stack, void *dst) ATTR_NONNULL();

size_t BLI_stack_count(const BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool BLI_stack_is_empty(const BLI_Stack *stack) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#endif  /* __BLI_STACK_H__ */
