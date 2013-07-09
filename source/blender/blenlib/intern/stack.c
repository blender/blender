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

/** \file blender/blenlib/intern/stack.c
 *  \ingroup bli
 */

#include <string.h>
#include <stdlib.h>  /* abort() */

#include "BLI_stack.h"  /* own include */

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

struct BLI_Stack {
	void *data;

	int totelem;
	int maxelem;

	int elem_size;
};

BLI_Stack *BLI_stack_new(int elem_size, const char *description)
{
	BLI_Stack *stack = MEM_callocN(sizeof(*stack), description);

	stack->elem_size = elem_size;

	return stack;
}

void BLI_stack_free(BLI_Stack *stack)
{
	if (stack) {
		if (stack->data)
			MEM_freeN(stack->data);
		MEM_freeN(stack);
	}
}

/* Gets the last element in the stack */
#define STACK_LAST_ELEM(stack__) \
	(((char *)(stack__)->data) + \
	 ((stack__)->elem_size * ((stack__)->totelem - 1)))

void BLI_stack_push(BLI_Stack *stack, void *src)
{
	/* Ensure stack is large enough */
	if (stack->totelem == stack->maxelem) {
		if (stack->maxelem == 0) {
			/* Initialize stack with space for a small hardcoded
			 * number of elements */
			stack->maxelem = 32;
			stack->data = MEM_mallocN((stack->elem_size *
			                           stack->maxelem), AT);
		}
		else {
			/* Double stack size */
			int maxelem = stack->maxelem + stack->maxelem;
			/* Check for overflow */
			BLI_assert(maxelem > stack->maxelem);
			stack->data = MEM_reallocN(stack->data,
			                           (stack->elem_size *
			                            maxelem));
			stack->maxelem = maxelem;
		}
	}

	BLI_assert(stack->totelem < stack->maxelem);

	/* Copy source to end of stack */
	stack->totelem++;
	memcpy(STACK_LAST_ELEM(stack), src, stack->elem_size);
}

void BLI_stack_pop(BLI_Stack *stack, void *dst)
{
	BLI_assert(stack->totelem > 0);
	if (stack->totelem > 0) {
		memcpy(dst, STACK_LAST_ELEM(stack), stack->elem_size);
		stack->totelem--;
	}
}

int BLI_stack_empty(const BLI_Stack *stack)
{
	return stack->totelem == 0;
}
