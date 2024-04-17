/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Primitive generic buffer library.
 *
 * - Automatically grow as needed.
 *   (currently never shrinks).
 * - Can be passed between functions.
 * - Supports using stack memory by default,
 *   falling back to heap as needed.
 *
 * Usage examples:
 * \code{.c}
 * BLI_buffer_declare_static(int, my_int_array, BLI_BUFFER_NOP, 32);
 *
 * BLI_buffer_append(my_int_array, int, 42);
 * BLI_assert(my_int_array.count == 1);
 * BLI_assert(BLI_buffer_at(my_int_array, int, 0) == 42);
 *
 * BLI_buffer_free(&my_int_array);
 * \endcode
 *
 * \note this more or less fills same purpose as #BLI_array,
 * but supports resizing the array outside of the function
 * it was declared in.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* Keep last. */

static void *buffer_alloc(BLI_Buffer *buffer, const size_t len)
{
  return MEM_mallocN(buffer->elem_size * len, "BLI_Buffer.data");
}

static void *buffer_realloc(BLI_Buffer *buffer, const size_t len)
{
  return MEM_reallocN_id(buffer->data, buffer->elem_size * len, "BLI_Buffer.data");
}

void BLI_buffer_resize(BLI_Buffer *buffer, const size_t new_count)
{
  if (UNLIKELY(new_count > buffer->alloc_count)) {
    if (buffer->flag & BLI_BUFFER_USE_STATIC) {
      const void *orig = buffer->data;

      buffer->data = buffer_alloc(buffer, new_count);
      memcpy(buffer->data, orig, buffer->elem_size * buffer->count);
      buffer->alloc_count = new_count;
      buffer->flag &= ~BLI_BUFFER_USE_STATIC;
    }
    else {
      if (buffer->alloc_count && (new_count < buffer->alloc_count * 2)) {
        buffer->alloc_count *= 2;
      }
      else {
        buffer->alloc_count = new_count;
      }

      buffer->data = buffer_realloc(buffer, buffer->alloc_count);
    }
  }

  buffer->count = new_count;
}

void BLI_buffer_reinit(BLI_Buffer *buffer, const size_t new_count)
{
  if (UNLIKELY(new_count > buffer->alloc_count)) {
    if ((buffer->flag & BLI_BUFFER_USE_STATIC) == 0) {
      if (buffer->data) {
        MEM_freeN(buffer->data);
      }
    }

    if (buffer->alloc_count && (new_count < buffer->alloc_count * 2)) {
      buffer->alloc_count *= 2;
    }
    else {
      buffer->alloc_count = new_count;
    }

    buffer->flag &= ~BLI_BUFFER_USE_STATIC;
    buffer->data = buffer_alloc(buffer, buffer->alloc_count);
  }

  buffer->count = new_count;
}

void _bli_buffer_append_array(BLI_Buffer *buffer, void *new_data, size_t count)
{
  size_t size = buffer->count;
  BLI_buffer_resize(buffer, size + count);

  uint8_t *bytes = (uint8_t *)buffer->data;
  memcpy(bytes + size * buffer->elem_size, new_data, count * buffer->elem_size);
}

void _bli_buffer_free(BLI_Buffer *buffer)
{
  if ((buffer->flag & BLI_BUFFER_USE_STATIC) == 0) {
    if (buffer->data) {
      MEM_freeN(buffer->data);
    }
  }
  memset(buffer, 0, sizeof(*buffer));
}
