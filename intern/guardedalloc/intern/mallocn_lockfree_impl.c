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
 * \ingroup MEM
 *
 * Memory allocation which keeps track on allocated memory counters
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h> /* memcpy */
#include <sys/types.h>

#include "MEM_guardedalloc.h"

/* to ensure strict conversions */
#include "../../source/blender/blenlib/BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mallocn_intern.h"

typedef struct MemHead {
  /* Length of allocated memory block. */
  size_t len;
} MemHead;

typedef struct MemHeadAligned {
  short alignment;
  size_t len;
} MemHeadAligned;

static unsigned int totblock = 0;
static size_t mem_in_use = 0, peak_mem = 0;
static bool malloc_debug_memset = false;

static void (*error_callback)(const char *) = NULL;

enum {
  MEMHEAD_ALIGN_FLAG = 1,
};

#define MEMHEAD_FROM_PTR(ptr) (((MemHead *)ptr) - 1)
#define PTR_FROM_MEMHEAD(memhead) (memhead + 1)
#define MEMHEAD_ALIGNED_FROM_PTR(ptr) (((MemHeadAligned *)ptr) - 1)
#define MEMHEAD_IS_ALIGNED(memhead) ((memhead)->len & (size_t)MEMHEAD_ALIGN_FLAG)

/* Uncomment this to have proper peak counter. */
#define USE_ATOMIC_MAX

MEM_INLINE void update_maximum(size_t *maximum_value, size_t value)
{
#ifdef USE_ATOMIC_MAX
  atomic_fetch_and_update_max_z(maximum_value, value);
#else
  *maximum_value = value > *maximum_value ? value : *maximum_value;
#endif
}

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
static void
print_error(const char *str, ...)
{
  char buf[512];
  va_list ap;

  va_start(ap, str);
  vsnprintf(buf, sizeof(buf), str, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  if (error_callback) {
    error_callback(buf);
  }
}

size_t MEM_lockfree_allocN_len(const void *vmemh)
{
  if (vmemh) {
    return MEMHEAD_FROM_PTR(vmemh)->len & ~((size_t)(MEMHEAD_ALIGN_FLAG));
  }
  else {
    return 0;
  }
}

void MEM_lockfree_freeN(void *vmemh)
{
  MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
  size_t len = MEM_lockfree_allocN_len(vmemh);

  if (vmemh == NULL) {
    print_error("Attempt to free NULL pointer\n");
#ifdef WITH_ASSERT_ABORT
    abort();
#endif
    return;
  }

  atomic_sub_and_fetch_u(&totblock, 1);
  atomic_sub_and_fetch_z(&mem_in_use, len);

  if (UNLIKELY(malloc_debug_memset && len)) {
    memset(memh + 1, 255, len);
  }
  if (UNLIKELY(MEMHEAD_IS_ALIGNED(memh))) {
    MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
    aligned_free(MEMHEAD_REAL_PTR(memh_aligned));
  }
  else {
    free(memh);
  }
}

void *MEM_lockfree_dupallocN(const void *vmemh)
{
  void *newp = NULL;
  if (vmemh) {
    MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    const size_t prev_size = MEM_lockfree_allocN_len(vmemh);
    if (UNLIKELY(MEMHEAD_IS_ALIGNED(memh))) {
      MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(
          prev_size, (size_t)memh_aligned->alignment, "dupli_malloc");
    }
    else {
      newp = MEM_lockfree_mallocN(prev_size, "dupli_malloc");
    }
    memcpy(newp, vmemh, prev_size);
  }
  return newp;
}

void *MEM_lockfree_reallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = NULL;

  if (vmemh) {
    MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    size_t old_len = MEM_lockfree_allocN_len(vmemh);

    if (LIKELY(!MEMHEAD_IS_ALIGNED(memh))) {
      newp = MEM_lockfree_mallocN(len, "realloc");
    }
    else {
      MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(len, (size_t)memh_aligned->alignment, "realloc");
    }

    if (newp) {
      if (len < old_len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        /* grow (or remain same size) */
        memcpy(newp, vmemh, old_len);
      }
    }

    MEM_lockfree_freeN(vmemh);
  }
  else {
    newp = MEM_lockfree_mallocN(len, str);
  }

  return newp;
}

void *MEM_lockfree_recallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = NULL;

  if (vmemh) {
    MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    size_t old_len = MEM_lockfree_allocN_len(vmemh);

    if (LIKELY(!MEMHEAD_IS_ALIGNED(memh))) {
      newp = MEM_lockfree_mallocN(len, "recalloc");
    }
    else {
      MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(len, (size_t)memh_aligned->alignment, "recalloc");
    }

    if (newp) {
      if (len < old_len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        memcpy(newp, vmemh, old_len);

        if (len > old_len) {
          /* grow */
          /* zero new bytes */
          memset(((char *)newp) + old_len, 0, len - old_len);
        }
      }
    }

    MEM_lockfree_freeN(vmemh);
  }
  else {
    newp = MEM_lockfree_callocN(len, str);
  }

  return newp;
}

void *MEM_lockfree_callocN(size_t len, const char *str)
{
  MemHead *memh;

  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)calloc(1, len + sizeof(MemHead));

  if (LIKELY(memh)) {
    memh->len = len;
    atomic_add_and_fetch_u(&totblock, 1);
    atomic_add_and_fetch_z(&mem_in_use, len);
    update_maximum(&peak_mem, mem_in_use);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Calloc returns null: len=" SIZET_FORMAT " in %s, total %u\n",
              SIZET_ARG(len),
              str,
              (unsigned int)mem_in_use);
  return NULL;
}

void *MEM_lockfree_calloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Calloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total %u\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        (unsigned int)mem_in_use);
    abort();
    return NULL;
  }

  return MEM_lockfree_callocN(total_size, str);
}

void *MEM_lockfree_mallocN(size_t len, const char *str)
{
  MemHead *memh;

  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)malloc(len + sizeof(MemHead));

  if (LIKELY(memh)) {
    if (UNLIKELY(malloc_debug_memset && len)) {
      memset(memh + 1, 255, len);
    }

    memh->len = len;
    atomic_add_and_fetch_u(&totblock, 1);
    atomic_add_and_fetch_z(&mem_in_use, len);
    update_maximum(&peak_mem, mem_in_use);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total %u\n",
              SIZET_ARG(len),
              str,
              (unsigned int)mem_in_use);
  return NULL;
}

void *MEM_lockfree_malloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Malloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total %u\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        (unsigned int)mem_in_use);
    abort();
    return NULL;
  }

  return MEM_lockfree_mallocN(total_size, str);
}

void *MEM_lockfree_mallocN_aligned(size_t len, size_t alignment, const char *str)
{
  /* Huge alignment values doesn't make sense and they wouldn't fit into 'short' used in the
   * MemHead. */
  assert(alignment < 1024);

  /* We only support alignments that are a power of two. */
  assert(IS_POW2(alignment));

  /* Some OS specific aligned allocators require a certain minimal alignment. */
  if (alignment < ALIGNED_MALLOC_MINIMUM_ALIGNMENT) {
    alignment = ALIGNED_MALLOC_MINIMUM_ALIGNMENT;
  }

  /* It's possible that MemHead's size is not properly aligned,
   * do extra padding to deal with this.
   *
   * We only support small alignments which fits into short in
   * order to save some bits in MemHead structure.
   */
  size_t extra_padding = MEMHEAD_ALIGN_PADDING(alignment);

  len = SIZET_ALIGN_4(len);

  MemHeadAligned *memh = (MemHeadAligned *)aligned_malloc(
      len + extra_padding + sizeof(MemHeadAligned), alignment);

  if (LIKELY(memh)) {
    /* We keep padding in the beginning of MemHead,
     * this way it's always possible to get MemHead
     * from the data pointer.
     */
    memh = (MemHeadAligned *)((char *)memh + extra_padding);

    if (UNLIKELY(malloc_debug_memset && len)) {
      memset(memh + 1, 255, len);
    }

    memh->len = len | (size_t)MEMHEAD_ALIGN_FLAG;
    memh->alignment = (short)alignment;
    atomic_add_and_fetch_u(&totblock, 1);
    atomic_add_and_fetch_z(&mem_in_use, len);
    update_maximum(&peak_mem, mem_in_use);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total %u\n",
              SIZET_ARG(len),
              str,
              (unsigned int)mem_in_use);
  return NULL;
}

void MEM_lockfree_printmemlist_pydict(void)
{
}

void MEM_lockfree_printmemlist(void)
{
}

/* unused */
void MEM_lockfree_callbackmemlist(void (*func)(void *))
{
  (void)func; /* Ignored. */
}

void MEM_lockfree_printmemlist_stats(void)
{
  printf("\ntotal memory len: %.3f MB\n", (double)mem_in_use / (double)(1024 * 1024));
  printf("peak memory len: %.3f MB\n", (double)peak_mem / (double)(1024 * 1024));
  printf(
      "\nFor more detailed per-block statistics run Blender with memory debugging command line "
      "argument.\n");

#ifdef HAVE_MALLOC_STATS
  printf("System Statistics:\n");
  malloc_stats();
#endif
}

void MEM_lockfree_set_error_callback(void (*func)(const char *))
{
  error_callback = func;
}

bool MEM_lockfree_consistency_check(void)
{
  return true;
}

void MEM_lockfree_set_memory_debug(void)
{
  malloc_debug_memset = true;
}

size_t MEM_lockfree_get_memory_in_use(void)
{
  return mem_in_use;
}

unsigned int MEM_lockfree_get_memory_blocks_in_use(void)
{
  return totblock;
}

/* dummy */
void MEM_lockfree_reset_peak_memory(void)
{
  peak_mem = mem_in_use;
}

size_t MEM_lockfree_get_peak_memory(void)
{
  return peak_mem;
}

#ifndef NDEBUG
const char *MEM_lockfree_name_ptr(void *vmemh)
{
  if (vmemh) {
    return "unknown block name ptr";
  }
  else {
    return "MEM_lockfree_name_ptr(NULL)";
  }
}
#endif /* NDEBUG */
