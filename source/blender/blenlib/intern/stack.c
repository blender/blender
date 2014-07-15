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

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "BLI_stack.h"  /* own include */

#include "BLI_strict_flags.h"

#define USE_TOTELEM

#define CHUNK_EMPTY ((size_t)-1)
/* target chunks size: 64kb */
#define CHUNK_SIZE_DEFAULT (1 << 16)
/* ensure we get at least this many elems per chunk */
#define CHUNK_ELEM_MIN     32

/* Gets the last element in the stack */
#define CHUNK_LAST_ELEM(_stack) \
	((void)0, (((char *)(_stack)->chunk_curr->data) + \
	           ((_stack)->elem_size * (_stack)->chunk_index)))

#define IS_POW2(a) (((a) & ((a) - 1)) == 0)

struct StackChunk {
	struct StackChunk *next;
	char data[0];
};

struct BLI_Stack {
	struct StackChunk *chunk_curr;      /* currently active chunk */
	struct StackChunk *chunk_free;      /* free chunks */
	size_t             chunk_index;     /* index into 'chunk_curr' */
	size_t             chunk_elem_max;  /* number of elements per chunk */
	size_t elem_size;
#ifdef USE_TOTELEM
	size_t totelem;
#endif
};

/**
 * \return number of elements per chunk, optimized for slop-space.
 */
static size_t stack_chunk_elem_max_calc(const size_t elem_size, size_t chunk_size)
{
	/* get at least this number of elems per chunk */
	const size_t elem_size_min = elem_size * CHUNK_ELEM_MIN;

	BLI_assert((elem_size != 0) && (chunk_size != 0));

	while (UNLIKELY(chunk_size <= elem_size_min)) {
		chunk_size <<= 1;
	}

	/* account for slop-space */
	chunk_size -= (sizeof(struct StackChunk) + MEM_SIZE_OVERHEAD);

	return chunk_size / elem_size;
}

BLI_Stack *BLI_stack_new_ex(const size_t elem_size, const char *description,
                            const size_t chunk_size)
{
	BLI_Stack *stack = MEM_callocN(sizeof(*stack), description);

	stack->chunk_elem_max = stack_chunk_elem_max_calc(elem_size, chunk_size);
	stack->elem_size = elem_size;
	/* force init */
	stack->chunk_index = stack->chunk_elem_max - 1;

	/* ensure we have a correctly rounded size */
	BLI_assert((IS_POW2(stack->elem_size) == 0) ||
	           (IS_POW2((stack->chunk_elem_max * stack->elem_size) +
	                    (sizeof(struct StackChunk) + MEM_SIZE_OVERHEAD))));

	return stack;
}

/**
 * Create a new homogeneous stack with elements of 'elem_size' bytes.
 */
BLI_Stack *BLI_stack_new(const size_t elem_size, const char *description)
{
	return BLI_stack_new_ex(elem_size, description, CHUNK_SIZE_DEFAULT);
}

static void stack_free_chunks(struct StackChunk *data)
{
	while (data) {
		struct StackChunk *data_next = data->next;
		MEM_freeN(data);
		data = data_next;
	}
}

/**
 * Free the stack's data and the stack itself
 */
void BLI_stack_free(BLI_Stack *stack)
{
	stack_free_chunks(stack->chunk_curr);
	stack_free_chunks(stack->chunk_free);
	MEM_freeN(stack);
}

/**
 * Copies the source value onto the stack (note that it copies
 * elem_size bytes from 'src', the pointer itself is not stored)
 */
void *BLI_stack_push_r(BLI_Stack *stack)
{
	stack->chunk_index++;

	if (UNLIKELY(stack->chunk_index == stack->chunk_elem_max)) {
		struct StackChunk *chunk;
		if (stack->chunk_free) {
			chunk = stack->chunk_free;
			stack->chunk_free = chunk->next;
		}
		else {
			chunk = MEM_mallocN(
			        sizeof(*chunk) + (stack->elem_size * stack->chunk_elem_max),
			        __func__);
		}
		chunk->next = stack->chunk_curr;
		stack->chunk_curr = chunk;
		stack->chunk_index = 0;
	}

	BLI_assert(stack->chunk_index < stack->chunk_elem_max);

#ifdef USE_TOTELEM
	stack->totelem++;
#endif

	/* Return end of stack */
	return CHUNK_LAST_ELEM(stack);
}

void BLI_stack_push(BLI_Stack *stack, const void *src)
{
	void *dst = BLI_stack_push_r(stack);
	memcpy(dst, src, stack->elem_size);
}

/**
 * Retrieves and removes the top element from the stack.
 * The value is copies to \a dst, which must be at least \a elem_size bytes.
 *
 * Does not reduce amount of allocated memory.
 */
void BLI_stack_pop(BLI_Stack *stack, void *dst)
{
	BLI_assert(BLI_stack_is_empty(stack) == false);

	memcpy(dst, CHUNK_LAST_ELEM(stack), stack->elem_size);
#ifdef USE_TOTELEM
	stack->totelem--;
#endif
	if (--stack->chunk_index == CHUNK_EMPTY) {
		struct StackChunk *chunk_free;

		chunk_free        = stack->chunk_curr;
		stack->chunk_curr = stack->chunk_curr->next;

		chunk_free->next  = stack->chunk_free;
		stack->chunk_free = chunk_free;

		stack->chunk_index = stack->chunk_elem_max - 1;
	}
}

void BLI_stack_pop_n(BLI_Stack *stack, void *dst, unsigned int n)
{
	BLI_assert(n <= BLI_stack_count(stack));

	while (n--) {
		BLI_stack_pop(stack, dst);
		dst = (void *)((char *)dst + stack->elem_size);
	}
}

size_t BLI_stack_count(const BLI_Stack *stack)
{
#ifdef USE_TOTELEM
	return stack->totelem;
#else
	struct StackChunk *data = stack->chunk_curr;
	size_t totelem = stack->chunk_index + 1;
	size_t i;
	if (totelem != stack->chunk_elem_max) {
		data = data->next;
	}
	else {
		totelem = 0;
	}
	for (i = 0; data; data = data->next) {
		i++;
	}
	totelem += stack->chunk_elem_max * i;
	return totelem;
#endif
}

/**
 * Returns true if the stack is empty, false otherwise
 */
bool BLI_stack_is_empty(const BLI_Stack *stack)
{
#ifdef USE_TOTELEM
	BLI_assert((stack->chunk_curr == NULL) == (stack->totelem == 0));
#endif
	return (stack->chunk_curr == NULL);
}
