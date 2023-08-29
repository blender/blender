/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 *
 * Guarded memory allocation, and boundary-write detection.
 */

#include "MEM_guardedalloc.h"

/* to ensure strict conversions */
#include "../../source/blender/blenlib/BLI_strict_flags.h"

#include <assert.h>

#include "mallocn_intern.h"

#ifdef WITH_JEMALLOC_CONF
/* If JEMALLOC is used, it reads this global variable and enables background
 * threads to purge dirty pages. Otherwise we release memory too slowly or not
 * at all if the thread that did the allocation stays inactive. */
const char *malloc_conf = "background_thread:true,dirty_decay_ms:4000";
#endif

/* NOTE: Keep in sync with MEM_use_lockfree_allocator(). */
size_t (*MEM_allocN_len)(const void *vmemh) = MEM_lockfree_allocN_len;
void (*MEM_freeN)(void *vmemh) = MEM_lockfree_freeN;
void *(*MEM_dupallocN)(const void *vmemh) = MEM_lockfree_dupallocN;
void *(*MEM_reallocN_id)(void *vmemh, size_t len, const char *str) = MEM_lockfree_reallocN_id;
void *(*MEM_recallocN_id)(void *vmemh, size_t len, const char *str) = MEM_lockfree_recallocN_id;
void *(*MEM_callocN)(size_t len, const char *str) = MEM_lockfree_callocN;
void *(*MEM_calloc_arrayN)(size_t len, size_t size, const char *str) = MEM_lockfree_calloc_arrayN;
void *(*MEM_mallocN)(size_t len, const char *str) = MEM_lockfree_mallocN;
void *(*MEM_malloc_arrayN)(size_t len, size_t size, const char *str) = MEM_lockfree_malloc_arrayN;
void *(*MEM_mallocN_aligned)(size_t len,
                             size_t alignment,
                             const char *str) = MEM_lockfree_mallocN_aligned;
void (*MEM_printmemlist_pydict)(void) = MEM_lockfree_printmemlist_pydict;
void (*MEM_printmemlist)(void) = MEM_lockfree_printmemlist;
void (*MEM_callbackmemlist)(void (*func)(void *)) = MEM_lockfree_callbackmemlist;
void (*MEM_printmemlist_stats)(void) = MEM_lockfree_printmemlist_stats;
void (*MEM_set_error_callback)(void (*func)(const char *)) = MEM_lockfree_set_error_callback;
bool (*MEM_consistency_check)(void) = MEM_lockfree_consistency_check;
void (*MEM_set_memory_debug)(void) = MEM_lockfree_set_memory_debug;
size_t (*MEM_get_memory_in_use)(void) = MEM_lockfree_get_memory_in_use;
uint (*MEM_get_memory_blocks_in_use)(void) = MEM_lockfree_get_memory_blocks_in_use;
void (*MEM_reset_peak_memory)(void) = MEM_lockfree_reset_peak_memory;
size_t (*MEM_get_peak_memory)(void) = MEM_lockfree_get_peak_memory;

void (*mem_clearmemlist)(void) = mem_lockfree_clearmemlist;

#ifndef NDEBUG
const char *(*MEM_name_ptr)(void *vmemh) = MEM_lockfree_name_ptr;
void (*MEM_name_ptr_set)(void *vmemh, const char *str) = MEM_lockfree_name_ptr_set;
#endif

void *aligned_malloc(size_t size, size_t alignment)
{
  /* #posix_memalign requires alignment to be a multiple of `sizeof(void *)`. */
  assert(alignment >= ALIGNED_MALLOC_MINIMUM_ALIGNMENT);

#ifdef _WIN32
  return _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  void *result;

  if (posix_memalign(&result, alignment, size)) {
    /* non-zero means allocation error
     * either no allocation or bad alignment value
     */
    return NULL;
  }
  return result;
#else /* This is for Linux. */
  return memalign(alignment, size);
#endif
}

void aligned_free(void *ptr)
{
#ifdef _WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

/* Perform assert checks on allocator type change.
 *
 * Helps catching issues (in debug build) caused by an unintended allocator type change when there
 * are allocation happened. */
static void assert_for_allocator_change(void)
{
  /* NOTE: Assume that there is no "sticky" internal state which would make switching allocator
   * type after all allocations are freed unsafe. In fact, it should be safe to change allocator
   * type after all blocks has been freed: some regression tests do rely on this property of
   * allocators. */
  assert(MEM_get_memory_blocks_in_use() == 0);
}

void MEM_use_lockfree_allocator(void)
{
  /* NOTE: Keep in sync with static initialization of the variables. */

  /* TODO(sergey): Find a way to de-duplicate the logic. Maybe by requiring an explicit call
   * to guarded allocator initialization at an application startup. */

  assert_for_allocator_change();

  MEM_allocN_len = MEM_lockfree_allocN_len;
  MEM_freeN = MEM_lockfree_freeN;
  MEM_dupallocN = MEM_lockfree_dupallocN;
  MEM_reallocN_id = MEM_lockfree_reallocN_id;
  MEM_recallocN_id = MEM_lockfree_recallocN_id;
  MEM_callocN = MEM_lockfree_callocN;
  MEM_calloc_arrayN = MEM_lockfree_calloc_arrayN;
  MEM_mallocN = MEM_lockfree_mallocN;
  MEM_malloc_arrayN = MEM_lockfree_malloc_arrayN;
  MEM_mallocN_aligned = MEM_lockfree_mallocN_aligned;
  MEM_printmemlist_pydict = MEM_lockfree_printmemlist_pydict;
  MEM_printmemlist = MEM_lockfree_printmemlist;
  MEM_callbackmemlist = MEM_lockfree_callbackmemlist;
  MEM_printmemlist_stats = MEM_lockfree_printmemlist_stats;
  MEM_set_error_callback = MEM_lockfree_set_error_callback;
  MEM_consistency_check = MEM_lockfree_consistency_check;
  MEM_set_memory_debug = MEM_lockfree_set_memory_debug;
  MEM_get_memory_in_use = MEM_lockfree_get_memory_in_use;
  MEM_get_memory_blocks_in_use = MEM_lockfree_get_memory_blocks_in_use;
  MEM_reset_peak_memory = MEM_lockfree_reset_peak_memory;
  MEM_get_peak_memory = MEM_lockfree_get_peak_memory;

  mem_clearmemlist = mem_lockfree_clearmemlist;

#ifndef NDEBUG
  MEM_name_ptr = MEM_lockfree_name_ptr;
  MEM_name_ptr_set = MEM_lockfree_name_ptr_set;
#endif
}

void MEM_use_guarded_allocator(void)
{
  assert_for_allocator_change();

  MEM_allocN_len = MEM_guarded_allocN_len;
  MEM_freeN = MEM_guarded_freeN;
  MEM_dupallocN = MEM_guarded_dupallocN;
  MEM_reallocN_id = MEM_guarded_reallocN_id;
  MEM_recallocN_id = MEM_guarded_recallocN_id;
  MEM_callocN = MEM_guarded_callocN;
  MEM_calloc_arrayN = MEM_guarded_calloc_arrayN;
  MEM_mallocN = MEM_guarded_mallocN;
  MEM_malloc_arrayN = MEM_guarded_malloc_arrayN;
  MEM_mallocN_aligned = MEM_guarded_mallocN_aligned;
  MEM_printmemlist_pydict = MEM_guarded_printmemlist_pydict;
  MEM_printmemlist = MEM_guarded_printmemlist;
  MEM_callbackmemlist = MEM_guarded_callbackmemlist;
  MEM_printmemlist_stats = MEM_guarded_printmemlist_stats;
  MEM_set_error_callback = MEM_guarded_set_error_callback;
  MEM_consistency_check = MEM_guarded_consistency_check;
  MEM_set_memory_debug = MEM_guarded_set_memory_debug;
  MEM_get_memory_in_use = MEM_guarded_get_memory_in_use;
  MEM_get_memory_blocks_in_use = MEM_guarded_get_memory_blocks_in_use;
  MEM_reset_peak_memory = MEM_guarded_reset_peak_memory;
  MEM_get_peak_memory = MEM_guarded_get_peak_memory;

  mem_clearmemlist = mem_guarded_clearmemlist;

#ifndef NDEBUG
  MEM_name_ptr = MEM_guarded_name_ptr;
  MEM_name_ptr_set = MEM_guarded_name_ptr_set;
#endif
}
