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

/** \file
 * \ingroup bli
 *
 * Simple, fast memory allocator for allocating many small elements of different sizes
 * in fixed size memory chunks,
 * although allocations bigger than the chunk size are supported.
 * They will reduce the efficiency of this data-structure.
 * Elements are pointer aligned.
 *
 * Supports:
 *
 * - Allocation of mixed sizes.
 * - Iterating over allocations in-order.
 * - Clearing for re-use.
 *
 * Unsupported:
 *
 * - Freeing individual elements.
 *
 * \note We could inline iteration stepping,
 * but tests show this doesn't give noticeable speedup.
 */

#include <string.h>
#include <stdlib.h>

#include "BLI_utildefines.h"

#include "BLI_memiter.h" /* own include */

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h" /* keep last */

/* TODO: Valgrind. */

/* Clang defines this. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#  include "sanitizer/asan_interface.h"
#else
/* Ensure return value is used. */
#  define ASAN_POISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) (void)(0 && ((size) != 0 && (addr) != NULL))
#endif

typedef uintptr_t data_t;
typedef intptr_t offset_t;

/* Write the chunk terminator on adding each element.
 * typically we rely on the 'count' to avoid iterating past the end. */
// #define USE_TERMINATE_PARANOID

/* Currently totalloc isnt used. */
// #define USE_TOTALLOC

/* pad must be power of two */
#define PADUP(num, pad) (((num) + ((pad)-1)) & ~((pad)-1))

typedef struct BLI_memiter_elem {
  offset_t size;
  data_t data[0];
} BLI_memiter_elem;

typedef struct BLI_memiter_chunk {
  struct BLI_memiter_chunk *next;
  /**
   * internal format is:
   * ``[next_pointer, size:data, size:data, ..., negative_offset]``
   *
   * Where negative offset rewinds to the start.
   */
  data_t data[0];
} BLI_memiter_chunk;

typedef struct BLI_memiter {
  /* A pointer to 'head' is needed so we can iterate in the order allocated. */
  struct BLI_memiter_chunk *head, *tail;
  data_t *data_curr;
  data_t *data_last;
  /* Used unless a large element is requested.
   * (which should be very rare!). */
  uint chunk_size_in_bytes_min;
  uint count;
#ifdef USE_TOTALLOC
  uint totalloc;
#endif
} BLI_memiter;

BLI_INLINE uint data_offset_from_size(uint size)
{
  return (PADUP(size, (uint)sizeof(data_t))) / (uint)sizeof(data_t);
}

static void memiter_set_rewind_offset(BLI_memiter *mi)
{
  BLI_memiter_elem *elem = (BLI_memiter_elem *)mi->data_curr;

  ASAN_UNPOISON_MEMORY_REGION(elem, sizeof(BLI_memiter_elem));

  elem->size = (offset_t)(((data_t *)mi->tail) - mi->data_curr);
  BLI_assert(elem->size < 0);
}

static void memiter_init(BLI_memiter *mi)
{
  mi->head = NULL;
  mi->tail = NULL;
  mi->data_curr = NULL;
  mi->data_last = NULL;
  mi->count = 0;
#ifdef USE_TOTALLOC
  mi->totalloc = 0;
#endif
}

/* -------------------------------------------------------------------- */
/** \name Public API's
 * \{ */

/**
 * \param chunk_size_min: Should be a power of two and
 * significantly larger than the average element size used.
 *
 * While allocations of any size are supported, they won't be efficient
 * (effectively becoming a single-linked list).
 *
 * Its intended that many elements can be stored per chunk.
 */
BLI_memiter *BLI_memiter_create(uint chunk_size_min)
{
  BLI_memiter *mi = MEM_mallocN(sizeof(BLI_memiter), "BLI_memiter");
  memiter_init(mi);

  /* Small values are used for tests to check for correctness,
   * but otherwise not that useful. */
  const uint slop_space = (sizeof(BLI_memiter_chunk) + MEM_SIZE_OVERHEAD);
  if (chunk_size_min >= 1024) {
    /* As long as the input is a power of 2, this will give efficient sizes. */
    chunk_size_min -= slop_space;
  }

  mi->chunk_size_in_bytes_min = chunk_size_min;
  return mi;
}

void *BLI_memiter_alloc(BLI_memiter *mi, uint elem_size)
{
  const uint data_offset = data_offset_from_size(elem_size);
  data_t *data_curr_next = mi->data_curr + (1 + data_offset);

  if (UNLIKELY(mi->data_curr == NULL) || (data_curr_next > mi->data_last)) {

#ifndef USE_TERMINATE_PARANOID
    if (mi->data_curr != NULL) {
      memiter_set_rewind_offset(mi);
    }
#endif

    uint chunk_size_in_bytes = mi->chunk_size_in_bytes_min;
    if (UNLIKELY(chunk_size_in_bytes < elem_size + (uint)sizeof(data_t[2]))) {
      chunk_size_in_bytes = elem_size + (uint)sizeof(data_t[2]);
    }
    uint chunk_size = data_offset_from_size(chunk_size_in_bytes);
    BLI_memiter_chunk *chunk = MEM_mallocN(
        sizeof(BLI_memiter_chunk) + (chunk_size * sizeof(data_t)), "BLI_memiter_chunk");

    if (mi->head == NULL) {
      BLI_assert(mi->tail == NULL);
      mi->head = chunk;
    }
    else {
      mi->tail->next = chunk;
    }
    mi->tail = chunk;
    chunk->next = NULL;

    mi->data_curr = chunk->data;
    mi->data_last = chunk->data + (chunk_size - 1);
    data_curr_next = mi->data_curr + (1 + data_offset);

    ASAN_POISON_MEMORY_REGION(chunk->data, chunk_size * sizeof(data_t));
  }

  BLI_assert(data_curr_next <= mi->data_last);

  BLI_memiter_elem *elem = (BLI_memiter_elem *)mi->data_curr;

  ASAN_UNPOISON_MEMORY_REGION(elem, sizeof(BLI_memiter_elem) + elem_size);

  elem->size = (offset_t)elem_size;
  mi->data_curr = data_curr_next;

#ifdef USE_TERMINATE_PARANOID
  memiter_set_rewind_offset(mi);
#endif

  mi->count += 1;

#ifdef USE_TOTALLOC
  mi->totalloc += elem_size;
#endif

  return elem->data;
}

void *BLI_memiter_calloc(BLI_memiter *mi, uint elem_size)
{
  void *data = BLI_memiter_alloc(mi, elem_size);
  memset(data, 0, elem_size);
  return data;
}

void BLI_memiter_alloc_from(BLI_memiter *mi, uint elem_size, const void *data_from)
{
  void *data = BLI_memiter_alloc(mi, elem_size);
  memcpy(data, data_from, elem_size);
}

static void memiter_free_data(BLI_memiter *mi)
{
  BLI_memiter_chunk *chunk = mi->head;
  while (chunk) {
    BLI_memiter_chunk *chunk_next = chunk->next;

    /* Unpoison memory because MEM_freeN might overwrite it. */
    ASAN_UNPOISON_MEMORY_REGION(chunk, MEM_allocN_len(chunk));

    MEM_freeN(chunk);
    chunk = chunk_next;
  }
}

void BLI_memiter_destroy(BLI_memiter *mi)
{
  memiter_free_data(mi);
  MEM_freeN(mi);
}

void BLI_memiter_clear(BLI_memiter *mi)
{
  memiter_free_data(mi);
  memiter_init(mi);
}

uint BLI_memiter_count(const BLI_memiter *mi)
{
  return mi->count;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper API's
 * \{ */

/* Support direct lookup for first. */
void *BLI_memiter_elem_first(BLI_memiter *mi)
{
  if (mi->head != NULL) {
    BLI_memiter_chunk *chunk = mi->head;
    BLI_memiter_elem *elem = (BLI_memiter_elem *)chunk->data;
    return elem->data;
  }
  else {
    return NULL;
  }
}

void *BLI_memiter_elem_first_size(BLI_memiter *mi, uint *r_size)
{
  if (mi->head != NULL) {
    BLI_memiter_chunk *chunk = mi->head;
    BLI_memiter_elem *elem = (BLI_memiter_elem *)chunk->data;
    *r_size = (uint)elem->size;
    return elem->data;
  }
  else {
    return NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterator API's
 *
 * \note We could loop over elements until a NULL chunk is found,
 * however this means every allocation needs to preemptively run
 * #memiter_set_rewind_offset (see #USE_TERMINATE_PARANOID).
 * Unless we have a call to finalize allocation (which complicates usage).
 * So use a counter instead.
 *
 * \{ */

void BLI_memiter_iter_init(BLI_memiter *mi, BLI_memiter_handle *iter)
{
  iter->elem = mi->head ? (BLI_memiter_elem *)mi->head->data : NULL;
  iter->elem_left = mi->count;
}

bool BLI_memiter_iter_done(const BLI_memiter_handle *iter)
{
  return iter->elem_left != 0;
}

BLI_INLINE void memiter_chunk_step(BLI_memiter_handle *iter)
{
  BLI_assert(iter->elem->size < 0);
  BLI_memiter_chunk *chunk = (BLI_memiter_chunk *)(((data_t *)iter->elem) + iter->elem->size);
  chunk = chunk->next;
  iter->elem = chunk ? (BLI_memiter_elem *)chunk->data : NULL;
  BLI_assert(iter->elem == NULL || iter->elem->size >= 0);
}

void *BLI_memiter_iter_step_size(BLI_memiter_handle *iter, uint *r_size)
{
  if (iter->elem_left != 0) {
    iter->elem_left -= 1;
    if (UNLIKELY(iter->elem->size < 0)) {
      memiter_chunk_step(iter);
    }
    BLI_assert(iter->elem->size >= 0);
    uint size = (uint)iter->elem->size;
    *r_size = size; /* <-- only difference */
    data_t *data = iter->elem->data;
    iter->elem = (BLI_memiter_elem *)&data[data_offset_from_size(size)];
    return (void *)data;
  }
  else {
    return NULL;
  }
}

void *BLI_memiter_iter_step(BLI_memiter_handle *iter)
{
  if (iter->elem_left != 0) {
    iter->elem_left -= 1;
    if (UNLIKELY(iter->elem->size < 0)) {
      memiter_chunk_step(iter);
    }
    BLI_assert(iter->elem->size >= 0);
    uint size = (uint)iter->elem->size;
    data_t *data = iter->elem->data;
    iter->elem = (BLI_memiter_elem *)&data[data_offset_from_size(size)];
    return (void *)data;
  }
  else {
    return NULL;
  }
}

/** \} */
