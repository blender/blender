/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * \brief A generic structure queue
 * (a queue for fixed length generally small) structures.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_gsqueue.h"
#include "BLI_strict_flags.h"
#include "BLI_utildefines.h"

/* target chunk size: 64kb */
#define CHUNK_SIZE_DEFAULT (1 << 16)
/* ensure we get at least this many elems per chunk */
#define CHUNK_ELEM_MIN 32

struct QueueChunk {
  struct QueueChunk *next;
  char data[0];
};

struct _GSQueue {
  struct QueueChunk *chunk_first; /* first active chunk to pop from */
  struct QueueChunk *chunk_last;  /* last active chunk to push onto */
  struct QueueChunk *chunk_free;  /* free chunks to reuse */
  size_t chunk_first_index;       /* index into 'chunk_first' */
  size_t chunk_last_index;        /* index into 'chunk_last' */
  size_t chunk_elem_max;          /* number of elements per chunk */
  size_t elem_size;               /* memory size of elements */
  size_t elem_num;                /* total number of elements */
};

static void *queue_get_first_elem(GSQueue *queue)
{
  return ((char *)(queue)->chunk_first->data) + ((queue)->elem_size * (queue)->chunk_first_index);
}

static void *queue_get_last_elem(GSQueue *queue)
{
  return ((char *)(queue)->chunk_last->data) + ((queue)->elem_size * (queue)->chunk_last_index);
}

/**
 * \return number of elements per chunk, optimized for slop-space.
 */
static size_t queue_chunk_elem_max_calc(const size_t elem_size, size_t chunk_size)
{
  /* get at least this number of elems per chunk */
  const size_t elem_size_min = elem_size * CHUNK_ELEM_MIN;

  BLI_assert((elem_size != 0) && (chunk_size != 0));

  while (UNLIKELY(chunk_size <= elem_size_min)) {
    chunk_size <<= 1;
  }

  /* account for slop-space */
  chunk_size -= (sizeof(struct QueueChunk) + MEM_SIZE_OVERHEAD);

  return chunk_size / elem_size;
}

GSQueue *BLI_gsqueue_new(const size_t elem_size)
{
  GSQueue *queue = MEM_callocN(sizeof(*queue), "BLI_gsqueue_new");

  queue->chunk_elem_max = queue_chunk_elem_max_calc(elem_size, CHUNK_SIZE_DEFAULT);
  queue->elem_size = elem_size;
  /* force init */
  queue->chunk_last_index = queue->chunk_elem_max - 1;

  return queue;
}

static void queue_free_chunk(struct QueueChunk *data)
{
  while (data) {
    struct QueueChunk *data_next = data->next;
    MEM_freeN(data);
    data = data_next;
  }
}

void BLI_gsqueue_free(GSQueue *queue)
{
  queue_free_chunk(queue->chunk_first);
  queue_free_chunk(queue->chunk_free);
  MEM_freeN(queue);
}

void BLI_gsqueue_push(GSQueue *queue, const void *item)
{
  queue->chunk_last_index++;
  queue->elem_num++;

  if (UNLIKELY(queue->chunk_last_index == queue->chunk_elem_max)) {
    struct QueueChunk *chunk;
    if (queue->chunk_free) {
      chunk = queue->chunk_free;
      queue->chunk_free = chunk->next;
    }
    else {
      chunk = MEM_mallocN(sizeof(*chunk) + (queue->elem_size * queue->chunk_elem_max), __func__);
    }

    chunk->next = NULL;

    if (queue->chunk_last == NULL) {
      queue->chunk_first = chunk;
    }
    else {
      queue->chunk_last->next = chunk;
    }

    queue->chunk_last = chunk;
    queue->chunk_last_index = 0;
  }

  BLI_assert(queue->chunk_last_index < queue->chunk_elem_max);

  /* Return last of queue */
  memcpy(queue_get_last_elem(queue), item, queue->elem_size);
}

void BLI_gsqueue_pop(GSQueue *queue, void *r_item)
{
  BLI_assert(BLI_gsqueue_is_empty(queue) == false);

  memcpy(r_item, queue_get_first_elem(queue), queue->elem_size);
  queue->chunk_first_index++;
  queue->elem_num--;

  if (UNLIKELY(queue->chunk_first_index == queue->chunk_elem_max || queue->elem_num == 0)) {
    struct QueueChunk *chunk_free = queue->chunk_first;

    queue->chunk_first = queue->chunk_first->next;
    queue->chunk_first_index = 0;
    if (queue->chunk_first == NULL) {
      queue->chunk_last = NULL;
      queue->chunk_last_index = queue->chunk_elem_max - 1;
    }

    chunk_free->next = queue->chunk_free;
    queue->chunk_free = chunk_free;
  }
}

size_t BLI_gsqueue_len(const GSQueue *queue)
{
  return queue->elem_num;
}

bool BLI_gsqueue_is_empty(const GSQueue *queue)
{
  return (queue->chunk_first == NULL);
}
