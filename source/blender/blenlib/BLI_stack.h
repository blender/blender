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

typedef struct BLI_Stack BLI_Stack;

/* Create a new homogeneous stack with elements of 'elem_size' bytes */
BLI_Stack *BLI_stack_new(int elem_size, const char *description);

/* Free the stack's data and the stack itself */
void BLI_stack_free(BLI_Stack *stack);

/* Copies the source value onto the stack (note that it copies
 * elem_size bytes from 'src', the pointer itself is not stored) */
void BLI_stack_push(BLI_Stack *stack, void *src);

/* Retrieves and removes the top element from the stack. The value is
 * copies to 'dst', which must be at least elem_size bytes.
 *
 * Does not reduce amount of allocated memory.
 *
 * If stack is empty, 'dst' will not be modified. */
void BLI_stack_pop(BLI_Stack *stack, void *dst);

/* Returns TRUE if the stack is empty, FALSE otherwise */
int BLI_stack_empty(const BLI_Stack *stack);

#endif
