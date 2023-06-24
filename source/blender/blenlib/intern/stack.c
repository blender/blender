/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <stdlib.h> /* abort() */
#include <string.h>

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "BLI_stack.h" /* own include */

#include "BLI_strict_flags.h"

#define USE_TOTELEM

#define CHUNK_EMPTY ((size_t)-1)
/* target chunks size: 64kb */
#define CHUNK_SIZE_DEFAULT (1 << 16)
/* ensure we get at least this many elems per chunk */
#define CHUNK_ELEM_MIN 32

struct StackChunk {
  struct StackChunk *next;
  char data[0];
};

struct BLI_Stack {
  struct StackChunk *chunk_curr; /* currently active chunk */
  struct StackChunk *chunk_free; /* free chunks */
  size_t chunk_index;            /* index into 'chunk_curr' */
  size_t chunk_elem_max;         /* number of elements per chunk */
  size_t elem_size;
#ifdef USE_TOTELEM
  size_t elem_num;
#endif
};

static void *stack_get_last_elem(BLI_Stack *stack)
{
  return ((char *)(stack)->chunk_curr->data) + ((stack)->elem_size * (stack)->chunk_index);
}

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

BLI_Stack *BLI_stack_new_ex(const size_t elem_size,
                            const char *description,
                            const size_t chunk_size)
{
  BLI_Stack *stack = MEM_callocN(sizeof(*stack), description);

  stack->chunk_elem_max = stack_chunk_elem_max_calc(elem_size, chunk_size);
  stack->elem_size = elem_size;
  /* force init */
  stack->chunk_index = stack->chunk_elem_max - 1;

  return stack;
}

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

void BLI_stack_free(BLI_Stack *stack)
{
  stack_free_chunks(stack->chunk_curr);
  stack_free_chunks(stack->chunk_free);
  MEM_freeN(stack);
}

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
      chunk = MEM_mallocN(sizeof(*chunk) + (stack->elem_size * stack->chunk_elem_max), __func__);
    }
    chunk->next = stack->chunk_curr;
    stack->chunk_curr = chunk;
    stack->chunk_index = 0;
  }

  BLI_assert(stack->chunk_index < stack->chunk_elem_max);

#ifdef USE_TOTELEM
  stack->elem_num++;
#endif

  /* Return end of stack */
  return stack_get_last_elem(stack);
}

void BLI_stack_push(BLI_Stack *stack, const void *src)
{
  void *dst = BLI_stack_push_r(stack);
  memcpy(dst, src, stack->elem_size);
}

void BLI_stack_pop(BLI_Stack *stack, void *dst)
{
  BLI_assert(BLI_stack_is_empty(stack) == false);

  memcpy(dst, stack_get_last_elem(stack), stack->elem_size);

  BLI_stack_discard(stack);
}

void BLI_stack_pop_n(BLI_Stack *stack, void *dst, uint n)
{
  BLI_assert(n <= BLI_stack_count(stack));

  while (n--) {
    BLI_stack_pop(stack, dst);
    dst = (void *)((char *)dst + stack->elem_size);
  }
}

void BLI_stack_pop_n_reverse(BLI_Stack *stack, void *dst, uint n)
{
  BLI_assert(n <= BLI_stack_count(stack));

  dst = (void *)((char *)dst + (stack->elem_size * n));

  while (n--) {
    dst = (void *)((char *)dst - stack->elem_size);
    BLI_stack_pop(stack, dst);
  }
}

void *BLI_stack_peek(BLI_Stack *stack)
{
  BLI_assert(BLI_stack_is_empty(stack) == false);

  return stack_get_last_elem(stack);
}

void BLI_stack_discard(BLI_Stack *stack)
{
  BLI_assert(BLI_stack_is_empty(stack) == false);

#ifdef USE_TOTELEM
  stack->elem_num--;
#endif
  if (UNLIKELY(--stack->chunk_index == CHUNK_EMPTY)) {
    struct StackChunk *chunk_free;

    chunk_free = stack->chunk_curr;
    stack->chunk_curr = stack->chunk_curr->next;

    chunk_free->next = stack->chunk_free;
    stack->chunk_free = chunk_free;

    stack->chunk_index = stack->chunk_elem_max - 1;
  }
}

void BLI_stack_clear(BLI_Stack *stack)
{
#ifdef USE_TOTELEM
  if (UNLIKELY(stack->elem_num == 0)) {
    return;
  }
  stack->elem_num = 0;
#else
  if (UNLIKELY(stack->chunk_curr == NULL)) {
    return;
  }
#endif

  stack->chunk_index = stack->chunk_elem_max - 1;

  if (stack->chunk_free) {
    if (stack->chunk_curr) {
      /* move all used chunks into tail of free list */
      struct StackChunk *chunk_free_last = stack->chunk_free;
      while (chunk_free_last->next) {
        chunk_free_last = chunk_free_last->next;
      }
      chunk_free_last->next = stack->chunk_curr;
      stack->chunk_curr = NULL;
    }
  }
  else {
    stack->chunk_free = stack->chunk_curr;
    stack->chunk_curr = NULL;
  }
}

size_t BLI_stack_count(const BLI_Stack *stack)
{
#ifdef USE_TOTELEM
  return stack->elem_num;
#else
  struct StackChunk *data = stack->chunk_curr;
  size_t elem_num = stack->chunk_index + 1;
  size_t i;
  if (elem_num != stack->chunk_elem_max) {
    data = data->next;
  }
  else {
    elem_num = 0;
  }
  for (i = 0; data; data = data->next) {
    i++;
  }
  elem_num += stack->chunk_elem_max * i;
  return elem_num;
#endif
}

bool BLI_stack_is_empty(const BLI_Stack *stack)
{
#ifdef USE_TOTELEM
  BLI_assert((stack->chunk_curr == NULL) == (stack->elem_num == 0));
#endif
  return (stack->chunk_curr == NULL);
}
